#pragma once

#include "../render/platform/IPlatform.h"
#include "../render/core/Shader.h"
#include "../render/core/Renderer.h"
#include "../render/scene/Camera.h"
#include "../render/resource/Texture.h"
#include "../render/shadow/ShadowMap.h"
#include "../pipeline/Pipeline.h"
#include <memory>

// 应用程序生命周期管理：初始化平台、着色器、纹理、相机、阴影
class App
{
public:
    // 初始化所有资源，成功返回true
    bool Init(std::unique_ptr<IPlatform> platform, PipelineContext &ctx, int debugMode);

    // 清理所有资源
    void Shutdown();

    // 获取器，供RenderLoop使用
    IPlatform &GetPlatform() { return *m_Platform; }
    Shader &GetShader() { return m_Shader; }
    Shader &GetShadowShader() { return m_ShadowShader; }
    Shader &GetShadowMainShader() { return m_ShadowMainShader; }
    Renderer &GetRenderer() { return m_Renderer; }
    TextureManager &GetTextureManager() { return m_TexMgr; }
    Camera &GetCamera() { return m_Camera; }
    ShadowMap &GetShadowMap() { return m_ShadowMap; }
    int GetDebugMode() const { return m_DebugMode; }
    void SetDebugMode(int mode) { m_DebugMode = mode; }
    const PipelineContext &GetContext() const { return *m_Ctx; }
    bool IsShadowEnabled() const { return m_EnableShadow; }
    void SetShadowEnabled(bool enabled) { m_EnableShadow = enabled; }
    bool IsShaderPackEnabled() const { return m_EnableShaderPack; }
    void SetShaderPackEnabled(bool enabled) { m_EnableShaderPack = enabled; }

    // 生成包含尺寸信息的窗口标题
    static std::string MakeWindowTitle(const ModelSize &size);

private:
    std::unique_ptr<IPlatform> m_Platform;
    Shader m_Shader;
    Shader m_ShadowShader;
    Shader m_ShadowMainShader;
    Renderer m_Renderer;
    TextureManager m_TexMgr;
    Camera m_Camera;
    ShadowMap m_ShadowMap;
    int m_DebugMode = 0;
    bool m_EnableShadow = false; // 默认关闭，按 F 开启
    bool m_EnableShaderPack = false; // 默认关闭，按 G 开启
    const PipelineContext *m_Ctx = nullptr;

    // 计算所有Region的包围盒中心
    static void CalcRegionsCenter(const PipelineContext &ctx, float &cx, float &cy, float &cz);
};
