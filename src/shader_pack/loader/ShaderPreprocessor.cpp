#include "ShaderPreprocessor.h"
#include <regex>
#include <sstream>
#include <algorithm>
#include <cctype>

// 预处理着色器源码，将 GLSL 120/130 转换为 330+
std::string ShaderPreprocessor::Process(const std::string &source, bool isVertex,
                                         const std::map<std::string, std::string> &fileMap,
                                         const std::string &passName)
{
    std::string result = source;

    // 统一换行符为 \n（ZIP 文件可能使用 \r\n）
    result.erase(std::remove(result.begin(), result.end(), '\r'), result.end());

    // 1. 解析 #include 指令（如果提供了文件映射表）
    if (!fileMap.empty())
        result = ResolveIncludes(result, fileMap, 0);

    // 2. 替换旧版关键字
    result = ReplaceKeywords(result, isVertex);

    // 2.5 修复 reserved word 'for' 冲突
    // 将变量名 'for' 替换为 '_iris_for_'
    {
        static const std::regex forRegex(R"(\bfor\b)");
        std::string processed;
        auto it = std::sregex_iterator(result.begin(), result.end(), forRegex);
        auto end = std::sregex_iterator();
        size_t lastPos = 0;

        for (; it != end; ++it) {
            std::smatch match = *it;
            processed += result.substr(lastPos, match.position() - lastPos);

            bool isLoop = false;
            size_t nextPos = match.position() + match.length();
            while (nextPos < result.size() && std::isspace(static_cast<unsigned char>(result[nextPos]))) nextPos++;
            if (nextPos < result.size() && result[nextPos] == '(') isLoop = true;

            bool isMember = false;
            if (match.position() > 0) {
                size_t prevPos = match.position() - 1;
                while (prevPos > 0 && std::isspace(static_cast<unsigned char>(result[prevPos]))) prevPos--;
                if (result[prevPos] == '.') isMember = true;
            }

            if (!isLoop && !isMember)
                processed += "_iris_for_";
            else
                processed += "for";

            lastPos = match.position() + match.length();
        }
        processed += result.substr(lastPos);
        result = processed;
    }

    // 2.6 增加对 layout (location = 0) out vec4 outColor; 的支持
    // 有些旧版光影包可能还在用 gl_FragData[0]，Iris 会自动处理，但我们需要确保版本号匹配
    if (!isVertex && result.find("gl_FragData") == std::string::npos && result.find("outColor") == std::string::npos) {
        // 如果没有输出且没有使用旧版接口，尝试注入默认输出
    }

    // 3. 替换纹理函数
    result = ReplaceTextureFunctions(result);

    // 4. 替换片段输出（仅片段着色器）
    if (!isVertex)
        result = ReplaceFragDataOutput(result);

    // 5. 添加自定义宏定义（需要知道着色器阶段和 pass 名称）
    result = AddCustomDefines(result, isVertex, passName);

    // 6. 添加 Iris 标准 Uniform 声明（在全局作用域）
    result = AddIrisUniforms(result, isVertex);

    // 7. 添加版本声明（必须最后处理，放在最前面）
    result = AddVersionHeader(result);

    return result;
}

// ============================================================
// NormalizePath：规范化路径（统一斜杠、去掉前导 /）
// ============================================================
std::string ShaderPreprocessor::NormalizePath(const std::string &path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    while (!result.empty() && result[0] == '/')
        result.erase(0, 1);
    return result;
}

// ============================================================
// ResolveIncludes：递归展开 #include 指令
// 支持格式：#include "/lib/common.glsl" 或 #include "lib/common.glsl"
// ============================================================
std::string ShaderPreprocessor::ResolveIncludes(const std::string &source,
                                                  const std::map<std::string, std::string> &fileMap,
                                                  int depth)
{
    static const int MAX_INCLUDE_DEPTH = 16;
    if (depth >= MAX_INCLUDE_DEPTH)
        return source;

    static const std::regex includeRegex(R"(\s*#\s*include\s+[\"<]([^\">]+)[\">])");
    std::string result;
    std::istringstream stream(source);
    std::string line;

    while (std::getline(stream, line))
    {
        // 去掉行尾 \r（ZIP 文件可能使用 \r\n 行尾）
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::smatch match;
        if (std::regex_match(line, match, includeRegex))
        {
            // 提取 include 路径并规范化
            std::string includePath = NormalizePath(match[1].str());

            // 在文件映射中查找（尝试多种路径格式）
            auto it = fileMap.find(includePath);
            if (it == fileMap.end())
            {
                // 尝试添加 shaders/ 前缀
                it = fileMap.find("shaders/" + includePath);
            }
            if (it == fileMap.end())
            {
                // 尝试去掉 lib/ 前缀（某些光影包用不同路径格式）
                for (const auto &[path, content] : fileMap)
                {
                    if (path.size() >= includePath.size() &&
                        path.substr(path.size() - includePath.size()) == includePath)
                    {
                        it = fileMap.find(path);
                        break;
                    }
                }
            }

            if (it != fileMap.end())
            {
                // 递归展开被包含文件的内容
                std::string resolved = ResolveIncludes(it->second, fileMap, depth + 1);
                result += resolved;
            }
            else
            {
                // 找不到文件，保留注释提示
                result += "// [WARN] include not found: " + includePath + "\n";
            }
        }
        else
        {
            result += line + "\n";
        }
    }

    return result;
}

// ============================================================
// FindGlobalInsertPoint：查找全局作用域的插入点
// 跳过 #version、#extension、#define 等预处理指令
// 正确处理 #ifdef/#endif 嵌套块，避免在条件块内部插入
// 返回第一个非预处理指令行的位置（全局作用域）
// ============================================================
size_t ShaderPreprocessor::FindGlobalInsertPoint(const std::string &source)
{
    size_t pos = 0;
    size_t lastPreprocEnd = 0;  // 最后一个顶层预处理指令行末尾
    int ifDepth = 0;            // #ifdef/#if 嵌套深度

    while (pos < source.size())
    {
        size_t lineEnd = source.find('\n', pos);
        if (lineEnd == std::string::npos)
            lineEnd = source.size();

        std::string line = source.substr(pos, lineEnd - pos);

        // 去掉行首空白和行尾 \r
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (firstNonSpace != std::string::npos && line[firstNonSpace] == '#')
        {
            // 提取指令关键字
            std::string trimmed = line.substr(firstNonSpace);

            if (trimmed.find("#ifdef") == 0 || trimmed.find("#ifndef") == 0 ||
                (trimmed.find("#if") == 0 && trimmed.find("#ifdef") != 0 && trimmed.find("#ifndef") != 0))
            {
                ifDepth++;
            }
            else if (trimmed.find("#endif") == 0)
            {
                ifDepth--;
                if (ifDepth < 0) ifDepth = 0;
            }

            // 只记录顶层预处理指令的结束位置
            if (ifDepth == 0)
                lastPreprocEnd = lineEnd + 1;
        }
        else if (ifDepth == 0 && !line.empty() && firstNonSpace != std::string::npos)
        {
            // 在顶层遇到非预处理指令行，插入点在此行之前
            break;
        }

        pos = lineEnd + 1;
    }

    return lastPreprocEnd;
}

// 添加 #version 330 声明
std::string ShaderPreprocessor::AddVersionHeader(const std::string &source)
{
    static const std::regex versionRegex(R"(#version\s+\d+[^\n]*\n?)");
    // 检查是否已有版本声明
    if (source.find("#version") != std::string::npos)
    {
        // 替换现有版本声明
        // 使用 330 compatibility 以支持现代 layout(location) 和旧版内置变量
        return std::regex_replace(source, versionRegex, "#version 330 compatibility\n");
    }

    // 在最前面添加版本声明
    return "#version 330 compatibility\n" + source;
}

// 替换关键字：attribute → in, varying → out/in
std::string ShaderPreprocessor::ReplaceKeywords(const std::string &source, bool isVertex)
{
    // 缓存 regex 为 static const，避免每次调用重新构造
    static const std::regex re_attribute(R"(\battribute\b)");
    static const std::regex re_varying(R"(\bvarying\b)");
    static const std::regex re_mediump(R"(\bmediump\s+)");
    static const std::regex re_highp(R"(\bhighp\s+)");
    static const std::regex re_lowp(R"(\blowp\s+)");

    std::string result = source;

    // attribute → in（顶点着色器输入）
    result = std::regex_replace(result, re_attribute, "in");

    // varying → out（顶点着色器输出）或 in（片段着色器输入）
    if (isVertex)
        result = std::regex_replace(result, re_varying, "out");
    else
        result = std::regex_replace(result, re_varying, "in");

    // 清理 mediump/highp/lowp 精度限定符（GLSL 330 不支持非 sampler 的精度限定符）
    result = std::regex_replace(result, re_mediump, "");
    result = std::regex_replace(result, re_highp, "");
    result = std::regex_replace(result, re_lowp, "");

    return result;
}

// 替换纹理函数：texture2D/textureCube → texture
std::string ShaderPreprocessor::ReplaceTextureFunctions(const std::string &source)
{
    // 缓存 regex 为 static const，避免每次调用重新构造
    static const std::regex re_texture2D(R"(\btexture2D\s*\()");
    static const std::regex re_textureCube(R"(\btextureCube\s*\()");
    static const std::regex re_texture2DProj(R"(\btexture2DProj\s*\()");
    static const std::regex re_texture2DLod(R"(\btexture2DLod\s*\()");
    static const std::regex re_textureCubeLod(R"(\btextureCubeLod\s*\()");
    static const std::regex re_shadow2DLod(R"(\bshadow2DLod\s*\()");

    std::string result = source;

    // texture2D → texture
    result = std::regex_replace(result, re_texture2D, "texture(");

    // textureCube → texture
    result = std::regex_replace(result, re_textureCube, "texture(");

    // texture2DProj → textureProj
    result = std::regex_replace(result, re_texture2DProj, "textureProj(");

    // texture2DLod → textureLod
    result = std::regex_replace(result, re_texture2DLod, "textureLod(");

    // textureCubeLod → textureLod
    result = std::regex_replace(result, re_textureCubeLod, "textureLod(");

    // shadow2DLod → textureLod（Iris 光影包使用）
    result = std::regex_replace(result, re_shadow2DLod, "textureLod(");

    return result;
}

// 替换片段输出：gl_FragData[N] → out 变量
std::string ShaderPreprocessor::ReplaceFragDataOutput(const std::string &source)
{
    static const std::regex re_fragData0(R"(gl_FragData\s*\[\s*0\s*\])");

    std::string result = source;

    // 检查是否使用 gl_FragData
    if (result.find("gl_FragData") == std::string::npos)
        return result;

    // 添加输出变量声明（在全局作用域插入点之后）
    std::string outputDecl = "\nlayout(location = 0) out vec4 fragColor;\n";

    // 替换 gl_FragData[0] 为 fragColor
    result = std::regex_replace(result, re_fragData0, "fragColor");

    // 处理其他 MRT 输出（如果有的话）
    for (int i = 1; i < 8; i++)
    {
        std::string pattern = "gl_FragData\\[\\s*" + std::to_string(i) + "\\s*\\]";
        std::string replacement = "fragColor" + std::to_string(i);

        if (std::regex_search(result, std::regex(pattern)))
        {
            // 添加对应的输出声明
            outputDecl += "layout(location = " + std::to_string(i) + ") out vec4 " + replacement + ";\n";
            result = std::regex_replace(result, std::regex(pattern), replacement);
        }
    }

    // 在全局作用域插入点之后插入输出声明
    size_t insertPos = FindGlobalInsertPoint(result);
    result.insert(insertPos, outputDecl);

    return result;
}

// 添加自定义宏定义
// 关键：必须注入到 #version 之后、#ifdef 块之前，确保 OVERWORLD 在编译器处理 #ifdef 前已定义
// isVertex: 用于注入 VERTEX_SHADER / FRAGMENT_SHADER 阶段宏
// passName: 用于注入 pass 特定宏（如 gbuffers_terrain → GBUFFERS_TERRAIN）
std::string ShaderPreprocessor::AddCustomDefines(const std::string &source, bool isVertex,
                                                   const std::string &passName)
{
    std::string defines;
    // MC_GL_VERSION 标识（Iris 约定）
    defines += "#define MC_VERSION 12100\n";
    defines += "#define MC_GL_VERSION 330\n";
    // Iris 模式标识（激活光影包中的 Iris 代码路径）
    defines += "#ifndef IS_IRIS\n#define IS_IRIS\n#endif\n";
    // 预览器默认主世界维度
    defines += "#ifndef OVERWORLD\n#define OVERWORLD\n#endif\n";
    // Distant Horizons 模块标识（dh_terrain/dh_water 等 pass 需要）
    defines += "#ifndef DISTANT_HORIZONS\n#define DISTANT_HORIZONS\n#endif\n";
    // 着色器阶段宏（光影包用 #ifdef VERTEX_SHADER / FRAGMENT_SHADER 区分阶段特有代码）
    defines += isVertex ? "#define VERTEX_SHADER\n" : "#define FRAGMENT_SHADER\n";
    // pass 特定宏（如 gbuffers_terrain → GBUFFERS_TERRAIN，composite → COMPOSITE）
    if (!passName.empty())
    {
        std::string upperName = passName;
        for (auto &c : upperName) c = static_cast<char>(std::toupper(c));
        defines += "#define " + upperName + "\n";
    }

    // 找到 #version 和 #extension 行之后的位置（跳过这些必须在最前面的指令）
    size_t insertPos = 0;
    size_t pos = 0;
    while (pos < source.size())
    {
        size_t lineEnd = source.find('\n', pos);
        if (lineEnd == std::string::npos) lineEnd = source.size();

        std::string line = source.substr(pos, lineEnd - pos);
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace != std::string::npos)
        {
            std::string trimmed = line.substr(firstNonSpace);
            // 只跳过 #version 和 #extension（它们必须在文件最前面）
            if (trimmed.find("#version") == 0 || trimmed.find("#extension") == 0)
            {
                insertPos = lineEnd + 1;
                pos = lineEnd + 1;
                continue;
            }
        }
        break;
    }

    std::string result = source;
    result.insert(insertPos, defines);
    return result;
}

// ============================================================
// AddIrisUniforms：添加 Iris 标准 Uniform 声明
// 关键修复：插入到全局作用域（在所有预处理指令之后，函数声明之前）
// isVertex: true=顶点着色器(out), false=片段着色器(in)
// ============================================================
std::string ShaderPreprocessor::AddIrisUniforms(const std::string &source, bool isVertex)
{
    // 辅助 lambda：精确检查源码中是否已有 uniform 声明（格式：uniform TYPE name）
    auto hasUniform = [&](const std::string &type, const std::string &name) -> bool {
        return source.find("uniform " + type + " " + name) != std::string::npos;
    };

    std::string injected;

    // ---- 矩阵 Uniform（如果源码中已有则跳过，避免冲突）----
    if (!hasUniform("mat4", "gbufferModelView"))
    {
        injected += "// Iris 矩阵 Uniform\n";
        injected += "uniform mat4 gbufferModelView;\n";
        injected += "uniform mat4 gbufferModelViewInverse;\n";
        injected += "uniform mat4 gbufferProjection;\n";
        injected += "uniform mat4 gbufferProjectionInverse;\n";
        injected += "uniform mat4 gbufferPreviousModelView;\n";
        injected += "uniform mat4 gbufferPreviousProjection;\n";
        injected += "uniform mat4 shadowModelView;\n";
        injected += "uniform mat4 shadowModelViewInverse;\n";
        injected += "uniform mat4 shadowProjection;\n";
        injected += "uniform mat4 shadowProjectionInverse;\n";
    }

    // ---- 参数 Uniform（仅在源码中没有对应声明时注入）----
    if (!hasUniform("float", "frameTimeCounter"))
    {
        injected += "uniform float frameTimeCounter;\n";
        injected += "uniform float frameTime;\n";
        injected += "uniform int frameCounter;\n";
        injected += "uniform int worldTime;\n";
        injected += "uniform int moonPhase;\n";
        injected += "uniform vec3 cameraPosition;\n";
        injected += "uniform vec3 previousCameraPosition;\n";
        injected += "uniform float viewWidth;\n";
        injected += "uniform float viewHeight;\n";
        injected += "uniform float aspectRatio;\n";
        injected += "uniform float near;\n";
        injected += "uniform float far;\n";
        injected += "uniform float shadowDistance;\n";
        injected += "uniform int shadowMapResolution;\n";
        injected += "uniform int renderStage;\n";
        injected += "uniform vec2 texelSize;\n";
        injected += "uniform int entityId;\n";
        injected += "uniform vec4 entityColor;\n";
        injected += "uniform vec3 cameraPositionFract;\n";
        injected += "uniform vec3 cameraPositionBestFract;\n";
    }

    // dhMaterialId 独立检查（Distant Horizons 模块需要，不能放在上面的批量检查中）
    if (!hasUniform("int", "dhMaterialId"))
        injected += "uniform int dhMaterialId;\n";

    // ---- Distant Horizons 方块类型常量（DH 模块需要）----
    if (source.find("DH_BLOCK_") != std::string::npos &&
        source.find("const int DH_BLOCK_") == std::string::npos)
    {
        injected += "// Distant Horizons block type constants\n";
        injected += "const int DH_BLOCK_OTHER = 0;\n";
        injected += "const int DH_BLOCK_LEAVES = 1;\n";
        injected += "const int DH_BLOCK_GRASS = 2;\n";
        injected += "const int DH_BLOCK_ILLUMINATED = 3;\n";
        injected += "const int DH_BLOCK_LAVA = 4;\n";
        injected += "const int DH_BLOCK_WATER = 5;\n";
        injected += "const int DH_BLOCK_SNOW = 6;\n";
    }

    // ---- 常量（检查源码中是否已有声明，避免冲突）----
    // 源码可能在 #ifdef 块内声明了 sunPathRotation，由于我们定义了 OVERWORLD，这些块现在是活跃的
    if (source.find("sunPathRotation") == std::string::npos)
        injected += "const float sunPathRotation = -40.0;\n";
    if (source.find("MC_RENDER_STAGE_TERRAIN_SOLID") == std::string::npos)
    {
        injected += "const int MC_RENDER_STAGE_TERRAIN_SOLID = 0;\n";
        injected += "const int MC_RENDER_STAGE_TERRAIN_CUTOUT = 1;\n";
        injected += "const int MC_RENDER_STAGE_TERRAIN_CUTOUT_MIPPED = 2;\n";
        injected += "const int MC_RENDER_STAGE_TERRAIN_TRANSLUCENT = 3;\n";
        injected += "const int MC_RENDER_STAGE_ENTITIES = 4;\n";
        injected += "const int MC_RENDER_STAGE_BLOCK_ENTITIES = 5;\n";
        injected += "const int MC_RENDER_STAGE_PARTICLES = 6;\n";
        injected += "const int MC_RENDER_STAGE_WEATHER = 7;\n";
        injected += "const int MC_RENDER_STAGE_SUN = 8;\n";
        injected += "const int MC_RENDER_STAGE_MOON = 9;\n";
    }

    // ---- Distant Horizons 材质 ID 常量（Iris mini-ID）----
    // 光影包可能在 #ifdef DISTANT_HORIZONS 块内定义了部分常量，用 #ifndef 保护避免冲突
    injected += "#ifndef DH_BLOCK_UNKNOWN\n#define DH_BLOCK_UNKNOWN 0\n#endif\n";
    injected += "#ifndef DH_BLOCK_LEAVES\n#define DH_BLOCK_LEAVES 1\n#endif\n";
    injected += "#ifndef DH_BLOCK_STONE\n#define DH_BLOCK_STONE 2\n#endif\n";
    injected += "#ifndef DH_BLOCK_WOOD\n#define DH_BLOCK_WOOD 3\n#endif\n";
    injected += "#ifndef DH_BLOCK_METAL\n#define DH_BLOCK_METAL 4\n#endif\n";
    injected += "#ifndef DH_BLOCK_DIRT\n#define DH_BLOCK_DIRT 5\n#endif\n";
    injected += "#ifndef DH_BLOCK_GRASS\n#define DH_BLOCK_GRASS 6\n#endif\n";
    injected += "#ifndef DH_BLOCK_LAVA\n#define DH_BLOCK_LAVA 7\n#endif\n";
    injected += "#ifndef DH_BLOCK_DEEPSLATE\n#define DH_BLOCK_DEEPSLATE 8\n#endif\n";
    injected += "#ifndef DH_BLOCK_SNOW\n#define DH_BLOCK_SNOW 9\n#endif\n";
    injected += "#ifndef DH_BLOCK_SAND\n#define DH_BLOCK_SAND 10\n#endif\n";
    injected += "#ifndef DH_BLOCK_TERRACOTTA\n#define DH_BLOCK_TERRACOTTA 11\n#endif\n";
    injected += "#ifndef DH_BLOCK_NETHER_STONE\n#define DH_BLOCK_NETHER_STONE 12\n#endif\n";
    injected += "#ifndef DH_BLOCK_WATER\n#define DH_BLOCK_WATER 13\n#endif\n";
    injected += "#ifndef DH_BLOCK_AIR\n#define DH_BLOCK_AIR 14\n#endif\n";
    injected += "#ifndef DH_BLOCK_ILLUMINATED\n#define DH_BLOCK_ILLUMINATED 15\n#endif\n";

    // ---- 通用 varying 变量（composite/gbuffers shader 共享）----
    // 仅在源码中未声明时注入
    std::string varyingPrefix = isVertex ? "out " : "in ";
    if (source.find("vec2 texelCoord") == std::string::npos)
        injected += varyingPrefix + "vec2 texelCoord;\n";
    if (source.find("vec2 lmCoord") == std::string::npos)
        injected += varyingPrefix + "vec2 lmCoord;\n";
    if (source.find("vec4 glColor") == std::string::npos &&
        source.find("vec4 color") == std::string::npos)
        injected += varyingPrefix + "vec4 glColor;\n";
    if (source.find("vec2 texCoord") == std::string::npos &&
        source.find("vec2 midTexCoord") == std::string::npos &&
        source.find("vec2 mc_midTexCoord") == std::string::npos)
        injected += varyingPrefix + "vec2 texCoord;\n";
    if (source.find("vec3 normal") == std::string::npos &&
        source.find("vec3 vaNormal") == std::string::npos)
        injected += varyingPrefix + "vec3 normal;\n";

    // ---- Iris 属性声明（顶点着色器输入）----
    if (isVertex)
    {
        if (source.find("mc_midTexCoord") == std::string::npos)
            injected += "in vec2 mc_midTexCoord;\n";
        if (source.find("at_midBlock") == std::string::npos)
            injected += "in vec3 at_midBlock;\n";
    }

    // ---- 函数 stub（总是注入，确保函数在 #ifdef 块外也有默认实现）----
    if (source.find("GetSunVector") != std::string::npos &&
        source.find("vec3 GetSunVector") == std::string::npos)
    {
        // 源码使用了 GetSunVector 但没有定义（可能在 #ifdef 块内）
        injected += R"(vec3 GetSunVector() {
    float sunAngle = radians(15.0);
    return normalize(vec3(cos(sunAngle), sin(sunAngle), 0.0));
}
)";
    }
    if (source.find("GetMoonVector") != std::string::npos &&
        source.find("vec3 GetMoonVector") == std::string::npos)
    {
        injected += R"(vec3 GetMoonVector() {
    float moonAngle = radians(195.0);
    return normalize(vec3(cos(moonAngle), sin(moonAngle), 0.0));
}
)";
    }
    if (source.find("GetLightMapCoordinates") != std::string::npos &&
        source.find("vec2 GetLightMapCoordinates") == std::string::npos)
    {
        injected += R"(vec2 GetLightMapCoordinates() {
    vec2 lm = (gl_TextureMatrix[1] * gl_MultiTexCoord1).xy;
    return clamp((lm - 0.03125) * 1.06667, 0.0, 1.0);
}
)";
    }

    // ---- 辅助函数 stub ----
    if (source.find("clamp01") != std::string::npos &&
        source.find("float clamp01") == std::string::npos)
    {
        injected += R"(float clamp01(float x) { return clamp(x, 0.0, 1.0); }
vec2 clamp01(vec2 x) { return clamp(x, 0.0, 1.0); }
vec3 clamp01(vec3 x) { return clamp(x, 0.0, 1.0); }
vec4 clamp01(vec4 x) { return clamp(x, 0.0, 1.0); }
)";
    }
    if (source.find("max0") != std::string::npos &&
        source.find("float max0") == std::string::npos)
        injected += "float max0(float x) { return max(x, 0.0); }\n";
    if (source.find("min0") != std::string::npos &&
        source.find("float min0") == std::string::npos)
        injected += "float min0(float x) { return min(x, 0.0); }\n";

    // ---- gl_Fog 兼容 ----
    // GLSL 330 compatibility 模式下 gl_Fog 是内置变量，无需注入
    // 但如果光影包声明了自定义 gl_FogFragCoord，需要提供一个变量
    if (source.find("gl_FogFragCoord") != std::string::npos &&
        source.find("float gl_FogFragCoord") == std::string::npos)
    {
        injected += "float gl_FogFragCoord = 0.0;\n";
    }

    // 如果没有需要注入的内容，直接返回
    if (injected.empty())
        return source;

    // 在全局作用域插入点注入
    size_t insertPos = FindGlobalInsertPoint(source);
    std::string result = source;
    result.insert(insertPos, injected);
    return result;
}
