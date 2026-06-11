#include "ShaderPackLoader.h"
#include "MiniZipReader.h"
#include "../../utils/Log.h"

#include <sstream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

static LogSource log("ShaderPackLoader");

// ============================================================
// 已知的着色器 Pass 文件名映射表
// ============================================================
struct PassMapping
{
    const char *fileName;       // ZIP 内的文件名（不含 shaders/ 前缀）
    const char *passName;       // 内部 Pass 名称
    bool isVertex;              // 是否为顶点着色器
};

static const PassMapping k_PassMappings[] = {
    // GBuffer 系列
    {"gbuffers_terrain.vsh",        "gbuffers_terrain",        true},
    {"gbuffers_terrain.fsh",        "gbuffers_terrain",        false},
    {"gbuffers_entities.vsh",       "gbuffers_entities",       true},
    {"gbuffers_entities.fsh",       "gbuffers_entities",       false},
    {"gbuffers_basic.vsh",          "gbuffers_basic",          true},
    {"gbuffers_basic.fsh",          "gbuffers_basic",          false},
    {"gbuffers_textured.vsh",       "gbuffers_textured",       true},
    {"gbuffers_textured.fsh",       "gbuffers_textured",       false},
    {"gbuffers_skybasic.vsh",       "gbuffers_skybasic",       true},
    {"gbuffers_skybasic.fsh",       "gbuffers_skybasic",       false},
    {"gbuffers_skytextured.vsh",    "gbuffers_skytextured",    true},
    {"gbuffers_skytextured.fsh",    "gbuffers_skytextured",    false},
    {"gbuffers_hand.vsh",           "gbuffers_hand",           true},
    {"gbuffers_hand.fsh",           "gbuffers_hand",           false},
    {"gbuffers_water.vsh",          "gbuffers_water",          true},
    {"gbuffers_water.fsh",          "gbuffers_water",          false},
    {"gbuffers_damagedblock.vsh",   "gbuffers_damagedblock",   true},
    {"gbuffers_damagedblock.fsh",   "gbuffers_damagedblock",   false},
    // Composite 系列
    {"composite.vsh",               "composite",               true},
    {"composite.fsh",               "composite",               false},
    {"composite1.vsh",              "composite1",              true},
    {"composite1.fsh",              "composite1",              false},
    {"composite2.vsh",              "composite2",              true},
    {"composite2.fsh",              "composite2",              false},
    // Final Pass
    {"final.vsh",                   "final",                   true},
    {"final.fsh",                   "final",                   false},
    // Shadow Pass（在 shadow/ 子目录下）
    {"shadow/shadow.vsh",           "shadow",                  true},
    {"shadow/shadow.fsh",           "shadow",                  false},
    {"shadow/gbuffers_terrain.vsh", "shadow/gbuffers_terrain", true},
    {"shadow/gbuffers_terrain.fsh", "shadow/gbuffers_terrain", false},
};

// ============================================================
// MapPathToPassName：将 ZIP 内路径映射到 Pass 名称
// ============================================================
bool ShaderPackLoader::MapPathToPassName(const std::string &path,
                                          std::string &outPassName,
                                          bool &isVertex)
{
    // 统一将反斜杠转为正斜杠
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    // 去掉 "shaders/" 前缀（如果存在）
    const std::string prefix = "shaders/";
    if (normalized.find(prefix) == 0)
        normalized = normalized.substr(prefix.length());

    // 去掉 "program/" 子目录前缀（Iris/OptiFine 新格式）
    const std::string programPrefix = "program/";
    if (normalized.find(programPrefix) == 0)
        normalized = normalized.substr(programPrefix.length());

    // 在映射表中查找
    for (const auto &mapping : k_PassMappings)
    {
        if (normalized == mapping.fileName)
        {
            outPassName = mapping.passName;
            isVertex = mapping.isVertex;
            return true;
        }
    }

    return false;
}

// ============================================================
// ReadFileFromZip：从 ZIP 中读取指定路径的文件内容
// 使用自实现的 MiniZipReader 替代 minizip
// ============================================================
bool ShaderPackLoader::ReadFileFromZip(void *zipHandle, const std::string &path,
                                        std::vector<uint8_t> &outData)
{
    MiniZipReader *reader = static_cast<MiniZipReader *>(zipHandle);

    // 查找目标文件条目
    const MiniZipReader::Entry *entry = reader->FindEntry(path);
    if (!entry)
    {
        log.Warn("ZIP 中未找到文件: %s", path.c_str());
        return false;
    }

    // 读取并解压文件内容
    if (!reader->ReadFile(*entry, outData))
    {
        log.Error("读取 ZIP 文件失败: %s", path.c_str());
        return false;
    }

    return true;
}

// ============================================================
// ParseProperties：解析 shaders.properties 配置文件
// 格式：key=value，# 开头为注释
// ============================================================
bool ShaderPackLoader::ParseProperties(const std::string &content, ShaderPackConfig &config)
{
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line))
    {
        // 去除行首尾空白
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos)
            continue;

        line = line.substr(start);
        size_t end = line.find_last_not_of(" \t\r\n");
        if (end != std::string::npos)
            line = line.substr(0, end + 1);

        // 跳过注释和空行
        if (line.empty() || line[0] == '#')
            continue;

        // 解析 key=value
        auto eqPos = line.find('=');
        if (eqPos == std::string::npos)
            continue;

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        // 去除 key 和 value 的空白
        auto trim = [](std::string &s) {
            size_t p = s.find_first_not_of(" \t");
            if (p != std::string::npos) s = s.substr(p);
            p = s.find_last_not_of(" \t");
            if (p != std::string::npos) s = s.substr(0, p + 1);
        };
        trim(key);
        trim(value);

        // 解析已知配置项
        if (key == "shadowMapResolution")
            config.shadowMapResolution = std::stoi(value);
        else if (key == "shadow.distance")
            config.shadowDistance = std::stof(value);
        else if (key == "noiseTextureResolution")
            config.noiseTextureResolution = std::stoi(value);
        else
            config.customProperties[key] = value;
    }

    return true;
}

// ============================================================
// SplitCombinedShader：将合并的 .glsl 文件拆分为顶点和片段部分
// 标准格式：#ifdef VERTEX_SHADER ... #endif 和 #ifdef FRAGMENT_SHADER ... #endif
// ============================================================
// 查找匹配的 #endif（正确处理嵌套的 #if/#ifdef/#ifndef/#endif）
static size_t FindMatchingEndIf(const std::string &source, size_t startPos)
{
    int depth = 1;  // 已进入一个 #if 块
    size_t pos = startPos;
    while (pos < source.size() && depth > 0)
    {
        // 查找下一个 #if 或 #endif
        size_t nextIf = source.find("#if", pos);
        size_t nextEndif = source.find("#endif", pos);
        size_t nextElse = source.find("#else", pos);

        // 找到最近的预处理指令
        size_t nextDirective = std::string::npos;
        if (nextIf != std::string::npos) nextDirective = nextIf;
        if (nextEndif != std::string::npos && (nextDirective == std::string::npos || nextEndif < nextDirective))
            nextDirective = nextEndif;
        if (nextElse != std::string::npos && (nextDirective == std::string::npos || nextElse < nextDirective))
            nextDirective = nextElse;

        if (nextDirective == std::string::npos)
            break;

        // 判断指令类型（确保 #endif 不被误判为 #if）
        if (nextDirective == nextEndif && source.substr(nextEndif, 6) == "#endif")
        {
            depth--;
            if (depth == 0)
                return nextEndif;
            pos = nextEndif + 6;
        }
        else if (nextDirective == nextIf && source.substr(nextIf, 6) != "#endif" && source.substr(nextIf, 5) != "#else")
        {
            // #ifdef, #ifndef, #if
            depth++;
            pos = nextIf + 3;
        }
        else
        {
            // #else 或 #elif，跳过
            pos = nextDirective + 5;
        }
    }
    return std::string::npos;
}

static void SplitCombinedShader(const std::string &source,
                                 std::string &vertexPart,
                                 std::string &fragmentPart)
{
    // 查找公共代码（第一个 #ifdef 之前的部分）
    size_t firstIfdef = source.find("#ifdef");
    std::string commonCode = (firstIfdef != std::string::npos) ?
        source.substr(0, firstIfdef) : "";

    // 查找 VERTEX_SHADER 代码段
    size_t vertTag = source.find("VERTEX_SHADER");
    if (vertTag != std::string::npos)
    {
        // 定位到 #ifdef VERTEX_SHADER 行的末尾
        size_t vertStart = source.find('\n', vertTag) + 1;
        // 使用嵌套感知的方式查找匹配的 #endif
        size_t vertEnd = FindMatchingEndIf(source, vertStart);
        if (vertEnd != std::string::npos)
            vertexPart = commonCode + source.substr(vertStart, vertEnd - vertStart);
    }

    // 查找 FRAGMENT_SHADER 代码段
    size_t fragTag = source.find("FRAGMENT_SHADER");
    if (fragTag != std::string::npos)
    {
        size_t fragStart = source.find('\n', fragTag) + 1;
        size_t fragEnd = FindMatchingEndIf(source, fragStart);
        if (fragEnd != std::string::npos)
            fragmentPart = commonCode + source.substr(fragStart, fragEnd - fragStart);
    }
}

// ============================================================
// NormalizePath：规范化路径（统一斜杠、去掉前导 /）
// ============================================================
std::string ShaderPackLoader::NormalizePath(const std::string &path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');

    // 去掉前导斜杠
    while (!result.empty() && result[0] == '/')
        result.erase(0, 1);

    return result;
}

// ============================================================
// BuildFileMap：从 ZIP 所有条目构建文件路径 → 内容映射表
// ============================================================
void ShaderPackLoader::BuildFileMap(void *zipHandle, FileMap &outMap)
{
    MiniZipReader *reader = static_cast<MiniZipReader *>(zipHandle);
    const auto &entries = reader->GetEntries();

    for (const auto &entry : entries)
    {
        // 只加载文本文件（.glsl, .vsh, .fsh, .properties, .cfg）
        std::string name = entry.name;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        bool isTextFile = false;
        const char *exts[] = {".glsl", ".vsh", ".fsh", ".properties", ".cfg", ".h"};
        for (const char *ext : exts)
        {
            if (name.size() >= strlen(ext) &&
                name.substr(name.size() - strlen(ext)) == ext)
            {
                isTextFile = true;
                break;
            }
        }
        if (!isTextFile)
            continue;

        std::vector<uint8_t> data;
        if (!reader->ReadFile(entry, data))
            continue;

        std::string normalized = NormalizePath(entry.name);
        outMap[normalized] = std::string(data.begin(), data.end());

        // 同时存储不带 shaders/ 前缀的路径，方便 include 查找
        const std::string prefix = "shaders/";
        if (normalized.find(prefix) == 0)
        {
            std::string withoutPrefix = normalized.substr(prefix.length());
            if (outMap.find(withoutPrefix) == outMap.end())
                outMap[withoutPrefix] = outMap[normalized];
        }
    }

    log.Info("文件映射表构建完成: %zu 个文本文件", outMap.size());
}

// ============================================================
// ResolveIncludes：递归展开 #include 指令
// 支持格式：#include "/lib/common.glsl" 或 #include "lib/common.glsl"
// ============================================================
std::string ShaderPackLoader::ResolveIncludes(const std::string &source,
                                                const FileMap &fileMap,
                                                int depth)
{
    // 防止无限递归
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
            // 提取 include 路径
            std::string includePath = NormalizePath(match[1].str());

            // 在文件映射中查找
            auto it = fileMap.find(includePath);
            if (it != fileMap.end())
            {
                // 递归展开被包含文件的内容
                std::string resolved = ResolveIncludes(it->second, fileMap, depth + 1);
                result += resolved;
            }
            else
            {
                // 找不到文件，保留原始 include 并添加警告
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
// ScanShaderPasses：遍历 ZIP 扫描所有着色器文件
// ============================================================
void ShaderPackLoader::ScanShaderPasses(void *zipHandle, ShaderPackData &pack)
{
    MiniZipReader *reader = static_cast<MiniZipReader *>(zipHandle);
    const auto &entries = reader->GetEntries();

    // 先构建文件映射表，用于 #include 解析
    FileMap fileMap;
    BuildFileMap(zipHandle, fileMap);

    // 保存文件映射表到 ShaderPackData（供运行时 #include 解析使用）
    pack.fileMap = fileMap;

    for (const auto &entry : entries)
    {
        std::string normalized = entry.name;
        std::replace(normalized.begin(), normalized.end(), '\\', '/');

        // 处理 program/*.glsl 合并着色器文件（Iris/OptiFine 新格式）
        if (normalized.size() > 5 &&
            normalized.substr(normalized.size() - 5) == ".glsl")
        {
            // 从文件名提取 Pass 名称
            size_t lastSlash = normalized.rfind('/');
            std::string filename = (lastSlash != std::string::npos) ?
                normalized.substr(lastSlash + 1) : normalized;
            std::string passName = filename.substr(0, filename.size() - 5);

            // 读取合并着色器源码
            std::vector<uint8_t> data;
            if (!reader->ReadFile(entry, data))
                continue;

            std::string source(data.begin(), data.end());

            // 拆分为顶点和片段着色器
            std::string vertexPart, fragmentPart;
            SplitCombinedShader(source, vertexPart, fragmentPart);

            // 展开 #include 指令
            if (!vertexPart.empty())
                vertexPart = ResolveIncludes(vertexPart, fileMap);
            if (!fragmentPart.empty())
                fragmentPart = ResolveIncludes(fragmentPart, fileMap);

            auto &pass = pack.passes[passName];
            if (!vertexPart.empty())
            {
                pass.vertexSource = std::move(vertexPart);
                pass.hasVertex = true;
            }
            if (!fragmentPart.empty())
            {
                pass.fragmentSource = std::move(fragmentPart);
                pass.hasFragment = true;
            }

            log.Debug("加载合并着色器: %s (V:%s F:%s)", passName.c_str(),
                      pass.hasVertex ? "是" : "否", pass.hasFragment ? "是" : "否");
            continue;
        }

        // 处理传统的 .vsh/.fsh 分离着色器文件
        std::string passName;
        bool isVertex = false;
        if (!MapPathToPassName(entry.name, passName, isVertex))
            continue;

        // 读取着色器源码
        std::vector<uint8_t> data;
        if (!reader->ReadFile(entry, data))
            continue;

        std::string source(data.begin(), data.end());

        // 展开 #include 指令
        source = ResolveIncludes(source, fileMap);

        // 存入 Pass 映射
        auto &pass = pack.passes[passName];
        if (isVertex)
        {
            pass.vertexSource = std::move(source);
            pass.hasVertex = true;
        }
        else
        {
            pass.fragmentSource = std::move(source);
            pass.hasFragment = true;
        }

        log.Debug("加载着色器: %s (%s)", passName.c_str(), isVertex ? "VSH" : "FSH");
    }

    // 更新配置中的标志
    pack.config.hasShadow = pack.passes.count("shadow") > 0;
    pack.config.hasComposite = pack.passes.count("composite") > 0;

    log.Info("扫描完成，共加载 %zu 个 Pass", pack.passes.size());
}

// ============================================================
// LoadFromFile：从 ZIP 文件加载光影包
// ============================================================
bool ShaderPackLoader::LoadFromFile(const std::string &zipPath, ShaderPackData &out)
{
    log.Info("加载光影包: %s", zipPath.c_str());

    // 检查文件是否存在
    if (!fs::exists(zipPath))
    {
        log.Error("光影包文件不存在: %s", zipPath.c_str());
        return false;
    }

    // 使用 MiniZipReader 打开 ZIP 文件
    MiniZipReader reader;
    if (!reader.Open(zipPath))
    {
        log.Error("无法打开 ZIP 文件: %s", zipPath.c_str());
        return false;
    }

    // 从文件名提取光影包名称
    fs::path p(zipPath);
    out.packName = p.stem().string();

    // 尝试加载 shaders.properties（兼容不同 ZIP 路径格式）
    const MiniZipReader::Entry *propsEntry = reader.FindEntry("shaders/shaders.properties");
    if (!propsEntry)
        propsEntry = reader.FindEntry("shaders.properties");
    if (propsEntry)
    {
        std::vector<uint8_t> propsData;
        if (reader.ReadFile(*propsEntry, propsData))
        {
            std::string propsContent(propsData.begin(), propsData.end());
            ParseProperties(propsContent, out.config);
            log.Info("已加载 shaders.properties");
        }
    }
    else
    {
        log.Warn("未找到 shaders.properties，使用默认配置");
    }

    // 扫描并加载所有着色器 Pass
    ScanShaderPasses(&reader, out);

    if (out.passes.empty())
    {
        log.Error("光影包中未找到任何着色器文件");
        return false;
    }

    log.Info("光影包加载成功: %s (%zu 个 Pass)", out.packName.c_str(), out.passes.size());
    return true;
}

// ============================================================
// LoadFromDirectory：从目录加载光影包（调试用）
// ============================================================
bool ShaderPackLoader::LoadFromDirectory(const std::string &dirPath, ShaderPackData &out)
{
    log.Info("从目录加载光影包: %s", dirPath.c_str());

    fs::path root(dirPath);
    fs::path shadersDir = root / "shaders";

    if (!fs::exists(shadersDir) || !fs::is_directory(shadersDir))
    {
        // 尝试直接在根目录查找着色器文件
        shadersDir = root;
    }

    // 设置光影包名称
    out.packName = root.filename().string();

    // 加载 shaders.properties
    fs::path propsPath = shadersDir / "shaders.properties";
    if (fs::exists(propsPath))
    {
        std::ifstream file(propsPath);
        if (file.is_open())
        {
            std::string content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
            ParseProperties(content, out.config);
            log.Info("已加载 shaders.properties");
        }
    }

    // 遍历目录中的着色器文件
    for (auto &entry : fs::recursive_directory_iterator(shadersDir))
    {
        if (!entry.is_regular_file())
            continue;

        // 获取相对路径
        fs::path relative = fs::relative(entry.path(), shadersDir);
        std::string relStr = relative.generic_string();

        // 构造 "shaders/" 前缀路径用于匹配
        std::string fullPath = "shaders/" + relStr;

        std::string passName;
        bool isVertex = false;
        if (!MapPathToPassName(fullPath, passName, isVertex))
            continue;

        // 读取文件内容
        std::ifstream file(entry.path(), std::ios::binary);
        if (!file.is_open())
            continue;

        std::string source((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());

        auto &pass = out.passes[passName];
        if (isVertex)
        {
            pass.vertexSource = std::move(source);
            pass.hasVertex = true;
        }
        else
        {
            pass.fragmentSource = std::move(source);
            pass.hasFragment = true;
        }

        log.Debug("加载着色器: %s (%s)", passName.c_str(), isVertex ? "VSH" : "FSH");
    }

    // 更新标志
    out.config.hasShadow = out.passes.count("shadow") > 0;
    out.config.hasComposite = out.passes.count("composite") > 0;

    if (out.passes.empty())
    {
        log.Error("目录中未找到任何着色器文件");
        return false;
    }

    log.Info("光影包加载成功: %s (%zu 个 Pass)", out.packName.c_str(), out.passes.size());
    return true;
}
