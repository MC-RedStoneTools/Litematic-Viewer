#pragma once

#include "../render/platform/Window.h"
#include "../render/core/Shader.h"
#include "../render/core/Renderer.h"
#include "../render/scene/Camera.h"
#include "../render/resource/Texture.h"
#include "../pipeline/Pipeline.h"

// 应用程序生命周期管理：初始化窗口、着色器、纹理、相机
class App
{
public:
    // 初始化所有资源，成功返回true
    bool Init(const PipelineContext &ctx, int debugMode);

    // 清理所有资源
    void Shutdown();

    // 获取器，供RenderLoop使用
    Window &GetWindow() { return m_Window; }
    Shader &GetShader() { return m_Shader; }
    Renderer &GetRenderer() { return m_Renderer; }
    TextureManager &GetTextureManager() { return m_TexMgr; }
    Camera &GetCamera() { return m_Camera; }
    int GetDebugMode() const { return m_DebugMode; }
    void SetDebugMode(int mode) { m_DebugMode = mode; }
    const PipelineContext &GetContext() const { return *m_Ctx; }

    // 生成包含尺寸信息的窗口标题
    static std::string MakeWindowTitle(const ModelSize &size);

private:
    Window m_Window;
    Shader m_Shader;
    Renderer m_Renderer;
    TextureManager m_TexMgr;
    Camera m_Camera;
    int m_DebugMode = 0;
    const PipelineContext *m_Ctx = nullptr;

    // 计算所有Region的包围盒中心
    static void CalcRegionsCenter(const PipelineContext &ctx, float &cx, float &cy, float &cz);
};
