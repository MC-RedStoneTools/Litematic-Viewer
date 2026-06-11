#include "ShaderPackRuntime.h"
#include "../loader/ShaderPreprocessor.h"
#include "../../utils/Log.h"
#include <glad/gl.h>
#include <sstream>
#include <set>

static LogSource gLog("ShaderPackRuntime");

ShaderPackRuntime::~ShaderPackRuntime()
{
    Destroy();
}

bool ShaderPackRuntime::Load(const ShaderPackData &pack)
{
    Destroy();
    if (pack.passes.empty())
        return false;

    m_PackData = pack;
    m_Config = pack.config;
    m_Loaded = true;
    return true;
}

bool ShaderPackRuntime::InitGPU(int screenWidth, int screenHeight)
{
    if (!m_Loaded)
    {
        gLog.Error("光影包数据未加载，无法初始化 GPU");
        return false;
    }

    if (m_GPUReady)
        return true;

    // 清理上次失败的残留 GPU 资源
    for (auto &[name, pass] : m_Passes)
    {
        if (pass.program)
            glDeleteProgram(pass.program);
    }
    m_Passes.clear();
    for (auto &[name, fbo] : m_FBOs)
    {
        if (fbo.colorTexture) glDeleteTextures(1, &fbo.colorTexture);
        if (fbo.depthTexture) glDeleteTextures(1, &fbo.depthTexture);
        if (fbo.fbo) glDeleteFramebuffers(1, &fbo.fbo);
    }
    m_FBOs.clear();

    int compiledCount = 0;
    for (auto &[name, source] : m_PackData.passes)
    {
        if (CompilePass(name, source))
            compiledCount++;
        else
            gLog.Warn("着色器 Pass 编译失败: %s", name.c_str());
    }

    // 分析错误并生成报告
    if (m_ErrorAnalyzer.HasErrors())
    {
        m_ErrorAnalyzer.AnalyzeErrors();

        // 输出简化版错误统计
        const auto &stats = m_ErrorAnalyzer.GetStats();
        gLog.Info("=== 着色器错误统计 ===");
        gLog.Info("总错误数: %d", stats.totalErrors);
        gLog.Info("  语法错误: %d", stats.syntaxErrors);
        gLog.Info("  未定义变量: %d", stats.undefinedVars);
        gLog.Info("  未定义函数: %d", stats.undefinedFuncs);
        gLog.Info("  其他错误: %d", stats.otherErrors);

        // 输出高频错误（前5个）
        auto frequentErrors = m_ErrorAnalyzer.GetFrequentErrors(2);
        if (!frequentErrors.empty())
        {
            gLog.Info("=== 高频错误 (出现>=2次) ===");
            std::set<std::string> printed;
            int count = 0;
            for (const auto &error : frequentErrors)
            {
                if (count >= 5) break;
                if (printed.insert(error.errorMessage).second)
                {
                    gLog.Info("  %s", error.errorMessage.c_str());
                    count++;
                }
            }
        }
    }

    if (compiledCount == 0)
    {
        gLog.Error("所有着色器 Pass 编译失败");
        return false;
    }

    CreateDefaultFBOs(screenWidth, screenHeight);

    m_GPUReady = true;
    gLog.Info("光影包 GPU 初始化完成: %d/%zu 个 Pass, %zu 个 FBO",
              compiledCount, m_PackData.passes.size(), m_FBOs.size());
    return true;
}

void ShaderPackRuntime::Destroy()
{
    m_Loaded = false;
    m_GPUReady = false;
    m_PackData = {};
    // 删除着色器程序
    for (auto &[name, pass] : m_Passes)
    {
        if (pass.program)
        {
            glDeleteProgram(pass.program);
            pass.program = 0;
        }
    }
    m_Passes.clear();

    // 删除 FBO 和纹理
    for (auto &[name, fbo] : m_FBOs)
    {
        if (fbo.colorTexture) glDeleteTextures(1, &fbo.colorTexture);
        if (fbo.depthTexture) glDeleteTextures(1, &fbo.depthTexture);
        if (fbo.fbo) glDeleteFramebuffers(1, &fbo.fbo);
    }
    m_FBOs.clear();
}

void ShaderPackRuntime::ApplyAppConfig(int shadowResolution, float shadowDistance)
{
    if (shadowResolution > 0)
        m_Config.shadowMapResolution = shadowResolution;
    if (shadowDistance > 0.0f)
        m_Config.shadowDistance = shadowDistance;
}

const CompiledPass* ShaderPackRuntime::GetPass(const std::string &name) const
{
    auto it = m_Passes.find(name);
    return (it != m_Passes.end() && it->second.compiled) ? &it->second : nullptr;
}

FrameBufferObject* ShaderPackRuntime::GetFBO(const std::string &name)
{
    auto it = m_FBOs.find(name);
    return (it != m_FBOs.end()) ? &it->second : nullptr;
}

bool ShaderPackRuntime::RecompilePass(const std::string &name, const ShaderPassSource &source)
{
    // 删除旧程序
    auto it = m_Passes.find(name);
    if (it != m_Passes.end() && it->second.program)
    {
        glDeleteProgram(it->second.program);
    }

    return CompilePass(name, source);
}

bool ShaderPackRuntime::CompilePass(const std::string &name, const ShaderPassSource &source)
{
    CompiledPass pass;

    // 跳过无 shader 源码的 pass（材质配置等非渲染 pass）
    if (!source.hasVertex && !source.hasFragment)
        return false;

    // 预处理着色器源码（传递文件映射表用于 #include 解析，传递 pass 名称用于宏注入）
    std::string vertSrc = source.hasVertex ?
        ShaderPreprocessor::Process(source.vertexSource, true, m_PackData.fileMap, name) : "";
    std::string fragSrc = source.hasFragment ?
        ShaderPreprocessor::Process(source.fragmentSource, false, m_PackData.fileMap, name) : "";

    // 跳过没有 main() 的 utility/library pass
    bool vertHasMain = vertSrc.find("void main") != std::string::npos;
    bool fragHasMain = fragSrc.find("void main") != std::string::npos;
    if (source.hasVertex && !vertHasMain && source.hasFragment && !fragHasMain)
    {
        m_ErrorAnalyzer.AddError(name, "跳过: 无 main() 函数 (utility pass)");
        return false;
    }
    if (source.hasVertex && !vertHasMain && !source.hasFragment)
    {
        m_ErrorAnalyzer.AddError(name, "跳过: 顶点着色器无 main() 函数");
        return false;
    }
    if (source.hasFragment && !fragHasMain && !source.hasVertex)
    {
        m_ErrorAnalyzer.AddError(name, "跳过: 片段着色器无 main() 函数");
        return false;
    }

    // 替换 compatibility 模式内置 uniform 为自定义名称
    // 因为 glGetUniformLocation 无法获取内置 uniform 的位置
    auto replaceCompatUniforms = [](std::string &src) {
        // 检查是否使用了这些矩阵
        bool useModelView = src.find("gl_ModelViewMatrix") != std::string::npos;
        bool useProjection = src.find("gl_ProjectionMatrix") != std::string::npos;
        bool useNormal = src.find("gl_NormalMatrix") != std::string::npos;
        
        // 替换 gl_ModelViewMatrix -> u_ModelViewMatrix
        size_t pos = 0;
        while ((pos = src.find("gl_ModelViewMatrix", pos)) != std::string::npos) {
            src.replace(pos, 18, "u_ModelViewMatrix");
            pos += 17;
        }
        // 替换 gl_ProjectionMatrix -> u_ProjectionMatrix
        pos = 0;
        while ((pos = src.find("gl_ProjectionMatrix", pos)) != std::string::npos) {
            src.replace(pos, 19, "u_ProjectionMatrix");
            pos += 18;
        }
        // 替换 gl_NormalMatrix -> u_NormalMatrix
        pos = 0;
        while ((pos = src.find("gl_NormalMatrix", pos)) != std::string::npos) {
            src.replace(pos, 15, "u_NormalMatrix");
            pos += 14;
        }
        
        // 添加 uniform 声明（在 #version 行之后）
        if (useModelView || useProjection || useNormal) {
            pos = src.find("\n", src.find("#version"));
            if (pos != std::string::npos) {
                std::string uniforms = "\n// Compatibility mode uniforms\n";
                if (useModelView) uniforms += "uniform mat4 u_ModelViewMatrix;\n";
                if (useProjection) uniforms += "uniform mat4 u_ProjectionMatrix;\n";
                if (useNormal) uniforms += "uniform mat3 u_NormalMatrix;\n";
                src.insert(pos + 1, uniforms);
            }
        }
    };

    if (source.hasVertex) {
        size_t before = vertSrc.find("gl_ModelViewMatrix");
        replaceCompatUniforms(vertSrc);
        size_t after = vertSrc.find("gl_ModelViewMatrix");
        if (name == "gbuffers_terrain") {
            gLog.Info("gbuffers_terrain vertex: before=%d, after=%d, hasVertex=%d", 
                      (int)before, (int)after, source.hasVertex);
        }
    }
    if (source.hasFragment) {
        replaceCompatUniforms(fragSrc);
    }

    // 编译顶点着色器
    GLuint vertShader = 0;
    if (source.hasVertex)
    {
        vertShader = CompileShader(GL_VERTEX_SHADER, vertSrc);
        if (!vertShader)
        {
            // 收集顶点着色器错误
            m_ErrorAnalyzer.AddError(name, "顶点着色器编译失败");
            return false;
        }
    }

    // 编译片段着色器
    GLuint fragShader = 0;
    if (source.hasFragment)
    {
        fragShader = CompileShader(GL_FRAGMENT_SHADER, fragSrc);
        if (!fragShader)
        {
            // 收集片段着色器错误
            m_ErrorAnalyzer.AddError(name, "片段着色器编译失败");
            if (vertShader) glDeleteShader(vertShader);
            return false;
        }
    }

    // 调试：dump gbuffers_terrain 着色器到文件（替换后）
    if (name == "gbuffers_terrain")
    {
        if (source.hasVertex) {
            FILE *f = fopen("gbuffers_terrain_vert.glsl", "w");
            if (f) {
                fprintf(f, "%s", vertSrc.c_str());
                fclose(f);
                gLog.Info("Dumped gbuffers_terrain vertex shader to gbuffers_terrain_vert.glsl (%d chars)", (int)vertSrc.size());
            }
        }
        if (source.hasFragment) {
            FILE *f = fopen("gbuffers_terrain_frag.glsl", "w");
            if (f) {
                fprintf(f, "%s", fragSrc.c_str());
                fclose(f);
                gLog.Info("Dumped gbuffers_terrain fragment shader to gbuffers_terrain_frag.glsl (%d chars)", (int)fragSrc.size());
            }
        }
    }

    // 创建着色器程序
    pass.program = glCreateProgram();

    // 在链接之前绑定属性位置（compatibility 模式内置变量）
    // 这些绑定确保内置变量映射到正确的属性位置
    glBindAttribLocation(pass.program, 0, "gl_Vertex");
    glBindAttribLocation(pass.program, 2, "gl_Normal");
    glBindAttribLocation(pass.program, 3, "gl_Color");
    glBindAttribLocation(pass.program, 8, "gl_MultiTexCoord0");
    glBindAttribLocation(pass.program, 9, "gl_MultiTexCoord1");
    // Iris 特有属性
    glBindAttribLocation(pass.program, 10, "mc_Entity");
    glBindAttribLocation(pass.program, 11, "mc_midTexCoord");
    glBindAttribLocation(pass.program, 12, "at_tangent");
    glBindAttribLocation(pass.program, 13, "at_midBlock");

    if (vertShader) glAttachShader(pass.program, vertShader);
    if (fragShader) glAttachShader(pass.program, fragShader);
    glLinkProgram(pass.program);

    // 检查链接状态
    GLint success;
    glGetProgramiv(pass.program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[1024];
        glGetProgramInfoLog(pass.program, sizeof(infoLog), nullptr, infoLog);
        gLog.Error("着色器程序链接失败 [%s]: %s", name.c_str(), infoLog);
        m_ErrorAnalyzer.AddError(name, std::string("链接失败: ") + infoLog);
        glDeleteProgram(pass.program);
        if (vertShader) glDeleteShader(vertShader);
        if (fragShader) glDeleteShader(fragShader);
        return false;
    }

    // 清理着色器对象
    if (vertShader) glDeleteShader(vertShader);
    if (fragShader) glDeleteShader(fragShader);

    pass.compiled = true;
    m_Passes[name] = pass;

    gLog.Info("着色器 Pass 编译成功: %s", name.c_str());
    return true;
}

void ShaderPackRuntime::CreateDefaultFBOs(int screenWidth, int screenHeight)
{
    int shadowRes = m_Config.shadowMapResolution;

    // Shadow FBO（只有深度）
    if (m_Config.hasShadow)
    {
        CreateFBO("shadow", shadowRes, shadowRes, false, true);
    }

    // GBuffer FBO（颜色 + 深度）
    CreateFBO("gbuffer", screenWidth, screenHeight, true, true);

    // Composite 系列 FBO（只有颜色）- 支持 composite, composite1, ..., composite15
    for (int i = 0; i <= 15; i++)
    {
        // composite pass 名称：composite (i=0), composite1, composite2, ...
        std::string passName = (i == 0) ? "composite" : ("composite" + std::to_string(i));
        // 只为已编译成功的 composite pass 创建 FBO
        if (m_Passes.count(passName) && m_Passes[passName].compiled)
        {
            CreateFBO(passName, screenWidth, screenHeight, true, false);
        }
    }
}

bool ShaderPackRuntime::CreateFBO(const std::string &name, int width, int height,
                                   bool hasColor, bool hasDepth)
{
    FrameBufferObject fbo;
    fbo.width = width;
    fbo.height = height;

    glGenFramebuffers(1, &fbo.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.fbo);

    // 创建颜色附件
    if (hasColor)
    {
        glGenTextures(1, &fbo.colorTexture);
        glBindTexture(GL_TEXTURE_2D, fbo.colorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0,
                     GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, fbo.colorTexture, 0);
    }

    // 创建深度附件
    if (hasDepth)
    {
        glGenTextures(1, &fbo.depthTexture);
        glBindTexture(GL_TEXTURE_2D, fbo.depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                               GL_TEXTURE_2D, fbo.depthTexture, 0);
    }

    // 检查 FBO 完整性
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        gLog.Error("FBO 创建失败: %s", name.c_str());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (fbo.colorTexture) glDeleteTextures(1, &fbo.colorTexture);
        if (fbo.depthTexture) glDeleteTextures(1, &fbo.depthTexture);
        glDeleteFramebuffers(1, &fbo.fbo);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_FBOs[name] = fbo;
    gLog.Info("FBO 创建成功: %s (%dx%d)", name.c_str(), width, height);
    return true;
}

// 更新所有已编译 Pass 的 Uniform 值（每帧调用）
// viewMatrix: 摄像机视图矩阵，projMatrix: 投影矩阵
// camPos: 摄像机世界坐标，time: 帧时间计数器
// width/height: 屏幕尺寸
void ShaderPackRuntime::UpdateUniforms(const float *viewMatrix, const float *projMatrix,
                                        const float *camPos, float time, int width, int height)
{
    for (auto &[name, pass] : m_Passes)
    {
        if (!pass.compiled) continue;
        glUseProgram(pass.program);

        GLint loc;
        // 矩阵 Uniform
        loc = glGetUniformLocation(pass.program, "gbufferModelView");
        if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, viewMatrix);

        loc = glGetUniformLocation(pass.program, "gbufferModelViewInverse");
        if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, viewMatrix); // 简化：实际需要求逆

        loc = glGetUniformLocation(pass.program, "gbufferProjection");
        if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, projMatrix);

        loc = glGetUniformLocation(pass.program, "gbufferProjectionInverse");
        if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, projMatrix); // 简化：实际需要求逆

        // 时间 Uniform
        loc = glGetUniformLocation(pass.program, "frameTimeCounter");
        if (loc >= 0) glUniform1f(loc, time);

        loc = glGetUniformLocation(pass.program, "frameTime");
        if (loc >= 0) glUniform1f(loc, 1.0f / 60.0f); // 假设60fps

        // 世界时间（0-24000，模拟昼夜循环）
        loc = glGetUniformLocation(pass.program, "worldTime");
        if (loc >= 0) glUniform1i(loc, static_cast<int>(time * 20.0f) % 24000);

        // 摄像机位置
        loc = glGetUniformLocation(pass.program, "cameraPosition");
        if (loc >= 0) glUniform3fv(loc, 1, camPos);

        // 屏幕尺寸
        loc = glGetUniformLocation(pass.program, "viewWidth");
        if (loc >= 0) glUniform1f(loc, static_cast<float>(width));

        loc = glGetUniformLocation(pass.program, "viewHeight");
        if (loc >= 0) glUniform1f(loc, static_cast<float>(height));

        loc = glGetUniformLocation(pass.program, "aspectRatio");
        if (loc >= 0) glUniform1f(loc, static_cast<float>(width) / static_cast<float>(height));

        // 裁剪面
        loc = glGetUniformLocation(pass.program, "near");
        if (loc >= 0) glUniform1f(loc, 0.05f);

        loc = glGetUniformLocation(pass.program, "far");
        if (loc >= 0) glUniform1f(loc, 1024.0f);

        // 阴影参数
        loc = glGetUniformLocation(pass.program, "shadowDistance");
        if (loc >= 0) glUniform1f(loc, m_Config.shadowDistance);

        loc = glGetUniformLocation(pass.program, "shadowMapResolution");
        if (loc >= 0) glUniform1i(loc, m_Config.shadowMapResolution);
    }
    glUseProgram(0);
}

unsigned int ShaderPackRuntime::CompileShader(unsigned int type, const std::string &source)
{
    GLuint shader = glCreateShader(type);
    const char *src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        gLog.Error("着色器编译失败 [%s]: %s",
                   type == GL_VERTEX_SHADER ? "vertex" : "fragment", infoLog);

        // 收集详细的错误信息到分析器
        std::string errorType = (type == GL_VERTEX_SHADER) ? "vertex" : "fragment";
        m_ErrorAnalyzer.AddError("shader_compile", std::string("[") + errorType + "] " + infoLog);

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}
