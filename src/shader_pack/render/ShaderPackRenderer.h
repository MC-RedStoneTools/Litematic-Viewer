#pragma once

#include "../runtime/ShaderPackRuntime.h"
#include "UniformBinder.h"
#include "../../render/scene/Camera.h"
#include "../../data/MeshData.h"
#include "../../render/resource/Texture.h"

class Renderer;

// 光影包渲染器：执行多 Pass 渲染
// 职责：Shadow → GBuffer → Composite → Final 四个 Pass
class ShaderPackRenderer
{
public:
    // 初始化
    bool Init(ShaderPackRuntime &runtime, int screenWidth, int screenHeight);

    // 渲染一帧（多 Pass）
    void Render(ShaderPackRuntime &runtime,
                const Camera &camera,
                const MeshData &mesh,
                const TextureManager &texMgr,
                const Renderer &renderer,
                float deltaTime);

    // 部分渲染：只用 gbuffers_terrain，跳过 composite，直接 blit 到屏幕
    void RenderPartial(ShaderPackRuntime &runtime,
                       const Camera &camera,
                       const MeshData &mesh,
                       const TextureManager &texMgr,
                       const Renderer &renderer);

    // 更新屏幕尺寸（窗口 resize 时调用）
    void SetScreenSize(int width, int height) { m_ScreenWidth = width; m_ScreenHeight = height; }

    // 清理
    void Destroy();

private:
    UniformBinder m_UniformBinder;
    int m_ScreenWidth = 0;
    int m_ScreenHeight = 0;
    unsigned int m_QuadVAO = 0;              // 全屏四边形 VAO (GLuint)
    unsigned int m_QuadVBO = 0;
    unsigned int m_LightmapTex = 0;          // Minecraft 风格光照图纹理 (GLuint)
    FrameBufferObject *m_LastCompositeFbo = nullptr;  // 最后一个成功执行的 composite FBO

    // 创建全屏四边形（用于后处理 Pass）
    void CreateFullscreenQuad();

    // 创建 Minecraft 风格 lightmap 纹理（16x16，方块光×天空光）
    void CreateLightmapTexture();

    // Pass 1: 渲染阴影
    void RenderShadowPass(ShaderPackRuntime &runtime,
                          const Camera &camera,
                          const MeshData &mesh,
                          const Renderer &renderer);

    // Pass 2: 渲染 G-Buffer
    void RenderGBufferPass(ShaderPackRuntime &runtime,
                           const Camera &camera,
                           const MeshData &mesh,
                           const TextureManager &texMgr,
                           const Renderer &renderer);

    // Pass 3: 后处理
    void RenderCompositePass(ShaderPackRuntime &runtime,
                             const Camera &camera);

    // Pass 4: 最终输出到屏幕
    void RenderFinalPass(ShaderPackRuntime &runtime);

    // 绘制全屏四边形
    void DrawFullscreenQuad();
};
