#pragma once

#include "../data/ShaderPackData.h"
#include "../ShaderErrorAnalyzer.h"
#include <map>
#include <string>

// 编译后的着色器 Pass
struct CompiledPass
{
    unsigned int program = 0;   // 着色器程序ID (GLuint)
    bool compiled = false;      // 是否编译成功
};

// FBO 资源
struct FrameBufferObject
{
    unsigned int fbo = 0;           // FBO ID (GLuint)
    unsigned int colorTexture = 0;  // 颜色附件 0 (GLuint)
    unsigned int colorTexture2 = 0; // 颜色附件 1 (MRT, GLuint)
    unsigned int depthTexture = 0;  // 深度附件 (GLuint)
    int width = 0;
    int height = 0;
    int numColorAttachments = 1;    // 颜色附件数量
};

// 光影包运行时：管理编译后的着色器和 FBO
// 职责：着色器编译、FBO 创建、资源生命周期管理
class ShaderPackRuntime
{
public:
    ~ShaderPackRuntime();

    // 加载光影包数据（无需 OpenGL 上下文，可在 Pipeline 阶段调用）
    bool Load(const ShaderPackData &pack);

    // 初始化 GPU 资源：编译着色器、创建 FBO（必须在 OpenGL 上下文就绪后调用）
    bool InitGPU(int screenWidth, int screenHeight);

    // 是否已加载数据 / GPU 是否就绪
    bool IsLoaded() const { return m_Loaded; }
    bool IsReady() const { return m_GPUReady; }

    // 清理资源
    void Destroy();

    // 获取编译后的 Pass
    const CompiledPass* GetPass(const std::string &name) const;

    // 获取 FBO
    FrameBufferObject* GetFBO(const std::string &name);

    // 重新编译指定 Pass（热重载用）
    bool RecompilePass(const std::string &name, const ShaderPassSource &source);

    // 应用 config.json 中的覆盖项（在 InitGPU 之前调用）
    void ApplyAppConfig(int shadowResolution, float shadowDistance);

    // 获取配置
    const ShaderPackConfig& GetConfig() const { return m_Config; }

    // 获取错误分析器
    const ShaderErrorAnalyzer& GetErrorAnalyzer() const { return m_ErrorAnalyzer; }

    // 更新 Uniform 值（每帧调用）
    void UpdateUniforms(const float *viewMatrix, const float *projMatrix,
                        const float *camPos, float time, int width, int height);

private:
    ShaderPackConfig m_Config;
    ShaderErrorAnalyzer m_ErrorAnalyzer;
    ShaderPackData m_PackData;
    bool m_Loaded = false;
    bool m_GPUReady = false;
    std::map<std::string, CompiledPass> m_Passes;
    std::map<std::string, FrameBufferObject> m_FBOs;

    // 编译单个着色器 Pass
    bool CompilePass(const std::string &name, const ShaderPassSource &source);

    // 创建 FBO（numColorAttachments: 颜色附件数量，支持 MRT）
    bool CreateFBO(const std::string &name, int width, int height,
                   bool hasColor, bool hasDepth, int numColorAttachments = 1);

    // 创建默认 FBO（Shadow、GBuffer、Composite）
    void CreateDefaultFBOs(int screenWidth, int screenHeight);

    // 编译单个着色器（顶点或片段）
    unsigned int CompileShader(unsigned int type, const std::string &source);
};
