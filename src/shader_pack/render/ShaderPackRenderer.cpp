#include "ShaderPackRenderer.h"
#include "../../render/core/Renderer.h"
#include "../../utils/Log.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>

static LogSource gLog("ShaderPackRenderer");

// GL 错误检查宏
#define GL_CHECK(call) do { \
    call; \
    GLenum err = glGetError(); \
    if (err != GL_NO_ERROR) { \
        gLog.Error("GL错误 0x%X 在 %s:%d", err, __FILE__, __LINE__); \
    } \
} while(0)

// 检查 FBO 状态
static bool CheckFBOStatus(const char *name) {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        gLog.Error("FBO '%s' 状态不完整: 0x%X", name, status);
        return false;
    }
    return true;
}

// 全屏四边形顶点数据（位置 + UV）
static const float QUAD_VERTICES[] = {
    // 位置       // UV
    -1.0f, -1.0f, 0.0f, 0.0f,
     1.0f, -1.0f, 1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f, 1.0f,
     1.0f,  1.0f, 1.0f, 1.0f,
};

bool ShaderPackRenderer::Init(ShaderPackRuntime &runtime, int screenWidth, int screenHeight)
{
    m_ScreenWidth = screenWidth;
    m_ScreenHeight = screenHeight;

    // 创建全屏四边形
    CreateFullscreenQuad();
    CreateLightmapTexture();

    gLog.Info("光影包渲染器初始化完成 (%dx%d)", screenWidth, screenHeight);
    return true;
}

void ShaderPackRenderer::Destroy()
{
    if (m_QuadVAO) glDeleteVertexArrays(1, &m_QuadVAO);
    if (m_QuadVBO) glDeleteBuffers(1, &m_QuadVBO);
    if (m_LightmapTex) glDeleteTextures(1, &m_LightmapTex);
    m_QuadVAO = 0;
    m_QuadVBO = 0;
    m_LightmapTex = 0;
}

void ShaderPackRenderer::Render(ShaderPackRuntime &runtime,
                                 const Camera &camera,
                                 const MeshData &mesh,
                                 const TextureManager &texMgr,
                                 const Renderer &renderer,
                                 float deltaTime)
{
    m_UniformBinder.SetScreenSize(m_ScreenWidth, m_ScreenHeight);

    if (runtime.GetConfig().hasShadow)
        RenderShadowPass(runtime, camera, mesh, renderer);

    RenderGBufferPass(runtime, camera, mesh, texMgr, renderer);

    if (runtime.GetConfig().hasComposite)
        RenderCompositePass(runtime, camera);

    RenderFinalPass(runtime);
}

void ShaderPackRenderer::CreateFullscreenQuad()
{
    glGenVertexArrays(1, &m_QuadVAO);
    glGenBuffers(1, &m_QuadVBO);

    glBindVertexArray(m_QuadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_QuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTICES), QUAD_VERTICES, GL_STATIC_DRAW);

    // 位置 (location=0): 2 float
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // UV (location=1): 2 float
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void *)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void ShaderPackRenderer::RenderShadowPass(ShaderPackRuntime &runtime,
                                            const Camera &camera,
                                            const MeshData &mesh,
                                            const Renderer &renderer)
{
    const CompiledPass *pass = runtime.GetPass("shadow");
    FrameBufferObject *fbo = runtime.GetFBO("shadow");
    if (!pass || !fbo) return;

    glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
    glViewport(0, 0, fbo->width, fbo->height);
    glClear(GL_DEPTH_BUFFER_BIT);

    m_UniformBinder.BindAll(pass->program, camera, 0.0f);
    renderer.DrawWithProgram(pass->program, TextureManager(), mesh);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShaderPackRenderer::RenderGBufferPass(ShaderPackRuntime &runtime,
                                            const Camera &camera,
                                            const MeshData &mesh,
                                            const TextureManager &texMgr,
                                            const Renderer &renderer)
{
    const CompiledPass *pass = runtime.GetPass("gbuffers_terrain");
    FrameBufferObject *fbo = runtime.GetFBO("gbuffer");
    if (!pass || !fbo)
    {
        gLog.Error("GBuffer pass 或 FBO 不存在");
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
    if (!CheckFBOStatus("gbuffer")) return;

    glViewport(0, 0, fbo->width, fbo->height);
    glClearColor(0.5f, 0.0f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(pass->program);
    m_UniformBinder.BindAll(pass->program, camera, 0.0f);

    // 绑定 lightmap 纹理到 slot 2（Iris 约定 lightmap = slot 2）
    if (m_LightmapTex)
    {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_LightmapTex);
    }

    // 绑定方块纹理到 slot 0
    glActiveTexture(GL_TEXTURE0);
    GLuint defaultTex = texMgr.GetBlockTexture("obsidian");
    if (defaultTex)
        glBindTexture(GL_TEXTURE_2D, defaultTex);

    renderer.DrawWithProgram(pass->program, texMgr, mesh);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        gLog.Error("GBuffer 绘制后 GL 错误: 0x%X", err);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShaderPackRenderer::RenderCompositePass(ShaderPackRuntime &runtime,
                                              const Camera &camera)
{
    FrameBufferObject *gbufFbo = runtime.GetFBO("gbuffer");
    if (!gbufFbo) return;

    // 阴影深度纹理（所有 composite pass 共享）
    FrameBufferObject *shadowFbo = runtime.GetFBO("shadow");

    // 遍历 composite, composite1, ..., composite15（Iris 多级后处理管线）
    // 跟踪上一个成功执行的 composite FBO，用于处理跳过的 pass
    FrameBufferObject *prevCompositeFbo = gbufFbo;  // 初始输入为 gbuffer

    int executedPasses = 0;
    for (int i = 0; i <= 15; i++)
    {
        std::string passName = (i == 0) ? "composite" : ("composite" + std::to_string(i));
        const CompiledPass *pass = runtime.GetPass(passName);
        FrameBufferObject *fbo = runtime.GetFBO(passName);
        if (!pass || !fbo) continue;  // 跳过编译失败的 pass

        // 绑定当前 composite FBO 作为输出
        glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
        if (!CheckFBOStatus(passName.c_str())) continue;

        glViewport(0, 0, fbo->width, fbo->height);
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);  // 蓝色背景，便于区分
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(pass->program);

        // 输入纹理来源：上一个成功执行的 composite（或 gbuffer）
        FrameBufferObject *inputFbo = prevCompositeFbo;

        // 绑定输入颜色纹理到 slot 0（colortex0）
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inputFbo->colorTexture);

        // 绑定 GBuffer 深度纹理到 slot 1（depthtex0）
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gbufFbo->depthTexture);

        // 绑定阴影深度纹理到 slot 3（shadowtex0）
        if (shadowFbo)
        {
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, shadowFbo->depthTexture);
        }

        // 绑定 lightmap 纹理到 slot 2
        if (m_LightmapTex)
        {
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, m_LightmapTex);
        }

        // 绑定所有 uniform
        m_UniformBinder.BindAll(pass->program, camera, 0.0f);

        // 设置采样器 uniform 映射
        glUniform1i(glGetUniformLocation(pass->program, "colortex0"), 0);
        glUniform1i(glGetUniformLocation(pass->program, "gcolor"), 0);
        glUniform1i(glGetUniformLocation(pass->program, "depthtex0"), 1);
        glUniform1i(glGetUniformLocation(pass->program, "shadowtex0"), 3);
        if (shadowFbo)
            glUniform1i(glGetUniformLocation(pass->program, "shadowtex1"), 4);
        glUniform1i(glGetUniformLocation(pass->program, "noisetex"), 5);
        glUniform1i(glGetUniformLocation(pass->program, "lightmap"), 2);
        glUniform1i(glGetUniformLocation(pass->program, "gtexture"), 0);
        glUniform1i(glGetUniformLocation(pass->program, "texture"), 0);
        glUniform1i(glGetUniformLocation(pass->program, "tex"), 0);

        DrawFullscreenQuad();

        // 更新上一个成功的 composite FBO（供下一个 pass 使用）
        prevCompositeFbo = fbo;
        m_LastCompositeFbo = fbo;  // 记录最后一个成功的 composite
        executedPasses++;

        gLog.Info("Composite pass '%s' 执行完成", passName.c_str());
    }

    gLog.Info("Composite: 共执行 %d 个 pass", executedPasses);

    // 如果没有执行任何 composite pass，使用 gbuffer
    if (prevCompositeFbo == gbufFbo)
        m_LastCompositeFbo = nullptr;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShaderPackRenderer::RenderFinalPass(ShaderPackRuntime &runtime)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_ScreenWidth, m_ScreenHeight);

    const CompiledPass *pass = runtime.GetPass("final");

    // 确定输入来源：最后一个成功的 composite 或 gbuffer
    FrameBufferObject *srcFbo = m_LastCompositeFbo ? m_LastCompositeFbo : runtime.GetFBO("gbuffer");

    // 如果没有 final pass，使用默认的 blit 操作
    if (!pass)
    {
        if (!srcFbo) {
            gLog.Error("Final: 无输入 FBO");
            return;
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo->fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, srcFbo->width, srcFbo->height,
                          0, 0, m_ScreenWidth, m_ScreenHeight,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    // 使用 final pass 着色器
    glUseProgram(pass->program);

    // 绑定输入纹理
    if (srcFbo)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, srcFbo->colorTexture);
    }

    // 设置采样器 uniform（final shader 通过 colortex0 读取输入）
    glUniform1i(glGetUniformLocation(pass->program, "colortex0"), 0);
    glUniform1i(glGetUniformLocation(pass->program, "gcolor"), 0);

    DrawFullscreenQuad();
}

void ShaderPackRenderer::DrawFullscreenQuad()
{
    glBindVertexArray(m_QuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void ShaderPackRenderer::RenderPartial(ShaderPackRuntime &runtime,
                                        const Camera &camera,
                                        const MeshData &mesh,
                                        const TextureManager &texMgr,
                                        const Renderer &renderer)
{
    m_UniformBinder.SetScreenSize(m_ScreenWidth, m_ScreenHeight);

    const CompiledPass *pass = runtime.GetPass("gbuffers_terrain");
    if (!pass) {
        gLog.Error("RenderPartial: gbuffers_terrain pass 不存在");
        return;
    }

    // 使用 gbuffer FBO（有 MRT 多渲染目标，匹配着色器 layout(location=0/1) 输出）
    FrameBufferObject *fbo = runtime.GetFBO("gbuffer");
    if (!fbo) {
        gLog.Error("RenderPartial: gbuffer FBO 不存在");
        return;
    }

    // 第一步：渲染到 gbuffer FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);

    // 确保 MRT draw buffers 正确（gbuffer 有 2 个颜色附件）
    if (fbo->numColorAttachments >= 2) {
        GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
        glDrawBuffers(2, drawBuffers);
    }

    glViewport(0, 0, fbo->width, fbo->height);
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDisable(GL_CULL_FACE);

    // 检查 FBO 完整性
    GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
        gLog.Error("gbuffer FBO 不完整: 0x%X", fboStatus);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    glUseProgram(pass->program);
    m_UniformBinder.BindAll(pass->program, camera, 0.0f);

    // 绑定光照贴图纹理到 slot 2
    if (m_LightmapTex) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_LightmapTex);
        GLint lightmapLoc = glGetUniformLocation(pass->program, "lightmap");
        if (lightmapLoc >= 0) glUniform1i(lightmapLoc, 2);
    }

    // 绘制几何体到 gbuffer
    renderer.DrawWithProgram(pass->program, texMgr, mesh);

    GLenum drawErr = glGetError();
    if (drawErr != GL_NO_ERROR)
        gLog.Error("RenderPartial 绘制后 GL错误: 0x%X", drawErr);

    // 一次性诊断（在 blit 之前，从 gbuffer FBO 读取真实深度和颜色）
    static bool diagOnce = false;
    if (!diagOnce) {
        diagOnce = true;
        int cx = fbo->width / 2, cy = fbo->height / 2;
        // 从 gbuffer FBO 读取深度（此时 gbuffer FBO 仍绑定）
        float d0, d1, d2, d3, d4;
        glReadPixels(cx, cy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d0);
        glReadPixels(0, 0, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d1);
        glReadPixels(fbo->width-1, fbo->height-1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d2);
        glReadPixels(cx/2, cy/2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d3);
        glReadPixels(cx+cx/2, cy+cy/2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d4);
        gLog.Info("gbuffer深度(FBO): center=%.6f tl=%.6f br=%.6f q1=%.6f q3=%.6f", d0, d1, d2, d3, d4);

        // 深度扫描：查找深度 < 1.0 的像素数量
        int nonBgCount = 0;
        float minD = 1.0f;
        for (int sy = 0; sy < fbo->height; sy += 20) {
            for (int sx = 0; sx < fbo->width; sx += 20) {
                float d;
                glReadPixels(sx, sy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d);
                if (d < 0.999f) {
                    nonBgCount++;
                    if (d < minD) minD = d;
                }
            }
        }
        gLog.Info("gbuffer深度扫描: 非背景像素=%d, 最小深度=%.6f (步长20)", nonBgCount, minD);

        // 从 gbuffer FBO 读取颜色
        float pixel[4];
        glReadPixels(cx, cy, 1, 1, GL_RGBA, GL_FLOAT, pixel);
        gLog.Info("gbuffer颜色(FBO): center=(%.3f, %.3f, %.3f, %.3f)", pixel[0], pixel[1], pixel[2], pixel[3]);

        // 检查 FBO 状态
        GLint curFBO = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &curFBO);
        gLog.Info("当前FBO=%d (应为gbuffer=%u)", curFBO, fbo->fbo);
    }

    // 第二步：将 gbuffer 颜色附件 blit 到默认帧缓冲（屏幕）
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo->fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, fbo->width, fbo->height,
                      0, 0, m_ScreenWidth, m_ScreenHeight,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// 创建 Minecraft 风格 lightmap 纹理
// 16x16 纹理，X=方块光(0-15)，Y=天空光(0-15)
// 亮度遵循 Minecraft 原版 gamma 曲线，确保场景不会全黑
void ShaderPackRenderer::CreateLightmapTexture()
{
    const int SIZE = 16;
    unsigned char pixels[SIZE * SIZE * 3];

    for (int sky = 0; sky < SIZE; sky++)
    {
        for (int block = 0; block < SIZE; block++)
        {
            // 使用较高基础亮度，确保画面可见
            float level = (float)std::max(block, sky) / 15.0f;
            float brightness = level * 0.5f + 0.5f;  // 范围 0.5 ~ 1.0
            brightness = std::min(brightness, 1.0f);

            unsigned char v = (unsigned char)(brightness * 255.0f);
            int idx = (sky * SIZE + block) * 3;
            pixels[idx + 0] = v;
            pixels[idx + 1] = (unsigned char)(v * 0.97f);
            pixels[idx + 2] = (unsigned char)(v * 0.90f);
        }
    }

    glGenTextures(1, &m_LightmapTex);
    glBindTexture(GL_TEXTURE_2D, m_LightmapTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SIZE, SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    // 使用 NEAREST 采样（Minecraft 风格，不插值）
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}
