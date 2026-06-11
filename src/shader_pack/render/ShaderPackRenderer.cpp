#include "ShaderPackRenderer.h"
#include "../../render/core/Renderer.h"
#include "../../utils/Log.h"
#include <glad/gl.h>
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
    // 每帧都打印，方便诊断
    gLog.Info("=== GBuffer Pass Start ===");

    const CompiledPass *pass = runtime.GetPass("gbuffers_terrain");
    FrameBufferObject *fbo = runtime.GetFBO("gbuffer");
    gLog.Info("GBuffer: pass=%p, fbo=%p", (void*)pass, (void*)fbo);
    if (!pass || !fbo)
    {
        gLog.Error("GBuffer pass 或 FBO 不存在");
        return;
    }

    // 诊断：打印相机信息
    static bool cameraLogged = false;
    if (!cameraLogged) {
        camera.PrintDebugInfo();
        const float *view = camera.GetViewMatrix();
        const float *proj = camera.GetProjectionMatrix();
        gLog.Info("视图矩阵:\n  [%.3f %.3f %.3f %.3f]\n  [%.3f %.3f %.3f %.3f]\n  [%.3f %.3f %.3f %.3f]\n  [%.3f %.3f %.3f %.3f]",
                  view[0], view[1], view[2], view[3],
                  view[4], view[5], view[6], view[7],
                  view[8], view[9], view[10], view[11],
                  view[12], view[13], view[14], view[15]);
        gLog.Info("投影矩阵:\n  [%.3f %.3f %.3f %.3f]\n  [%.3f %.3f %.3f %.3f]\n  [%.3f %.3f %.3f %.3f]\n  [%.3f %.3f %.3f %.3f]",
                  proj[0], proj[1], proj[2], proj[3],
                  proj[4], proj[5], proj[6], proj[7],
                  proj[8], proj[9], proj[10], proj[11],
                  proj[12], proj[13], proj[14], proj[15]);

        // 打印前几个顶点
        if (!mesh.vertices.empty()) {
            gLog.Info("前3个顶点:");
            for (int i = 0; i < 3 && i * 13 + 12 < (int)mesh.vertices.size(); i++) {
                int idx = i * 13;
                gLog.Info("  顶点%d: pos=(%.3f,%.3f,%.3f) uv=(%.3f,%.3f) color=(%.3f,%.3f,%.3f)",
                          i, mesh.vertices[idx], mesh.vertices[idx+1], mesh.vertices[idx+2],
                          mesh.vertices[idx+3], mesh.vertices[idx+4],
                          mesh.vertices[idx+5], mesh.vertices[idx+6], mesh.vertices[idx+7]);
            }
        }
        cameraLogged = true;
    }

    // 诊断：检查着色器程序
    GLint linkStatus;
    glGetProgramiv(pass->program, GL_LINK_STATUS, &linkStatus);
    if (linkStatus != GL_TRUE) {
        gLog.Error("GBuffer 着色器链接失败");
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, fbo->fbo);
    if (!CheckFBOStatus("gbuffer")) return;

    glViewport(0, 0, fbo->width, fbo->height);
    glClearColor(0.5f, 0.0f, 0.5f, 1.0f);  // 紫色背景，便于区分未渲染区域
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 诊断：检查绘制前 FBO 状态
    GLint drawFbo;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
    gLog.Info("GBuffer: 绑定 FBO=%d (期望=%u), viewport=%dx%d",
              drawFbo, fbo->fbo, fbo->width, fbo->height);

    m_UniformBinder.BindAll(pass->program, camera, 0.0f);

    // 绑定 lightmap 纹理到 slot 2（Iris 约定 lightmap = slot 2）
    if (m_LightmapTex)
    {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_LightmapTex);
    }

    // 绑定方块纹理到 slot 0
    glActiveTexture(GL_TEXTURE0);
    // 使用第一个可用的方块纹理
    GLuint defaultTex = texMgr.GetBlockTexture("obsidian");
    if (defaultTex)
        glBindTexture(GL_TEXTURE_2D, defaultTex);

    // 诊断：检查顶点数据
    gLog.Info("GBuffer: 顶点数=%d, drawCalls=%zu", mesh.GetVertexCount(), mesh.drawCalls.size());

    renderer.DrawWithProgram(pass->program, texMgr, mesh);

    // 诊断：检查绘制后是否有 GL 错误
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        gLog.Error("GBuffer 绘制后 GL 错误: 0x%X", err);
    }

    // 诊断：读取 GBuffer 多个位置的像素
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo->fbo);
    float pixel[4];

    // 中心像素
    glReadPixels(fbo->width/2, fbo->height/2, 1, 1, GL_RGBA, GL_FLOAT, pixel);
    gLog.Info("GBuffer 中心像素: (%.3f, %.3f, %.3f, %.3f)", pixel[0], pixel[1], pixel[2], pixel[3]);

    // 左上角像素
    glReadPixels(10, 10, 1, 1, GL_RGBA, GL_FLOAT, pixel);
    gLog.Info("GBuffer 左上像素: (%.3f, %.3f, %.3f, %.3f)", pixel[0], pixel[1], pixel[2], pixel[3]);

    // 检查深度缓冲
    float depth;
    glReadPixels(fbo->width/2, fbo->height/2, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    gLog.Info("GBuffer 中心深度: %.3f", depth);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShaderPackRenderer::RenderCompositePass(ShaderPackRuntime &runtime,
                                              const Camera &camera)
{
    FrameBufferObject *gbufFbo = runtime.GetFBO("gbuffer");
    if (!gbufFbo) return;

    // 诊断：检查 GBuffer 纹理
    gLog.Info("Composite: GBuffer colorTex=%u, depthTex=%u",
              gbufFbo->colorTexture, gbufFbo->depthTexture);

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
        glUniform1i(glGetUniformLocation(pass->program, "texture"), 0);
        glUniform1i(glGetUniformLocation(pass->program, "tex"), 0);

        DrawFullscreenQuad();

        // 诊断：检查 GL 错误
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            gLog.Error("Composite '%s' GL 错误: 0x%X", passName.c_str(), err);
        }

        // 诊断：读取输出像素
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo->fbo);
        float pixel[4];
        glReadPixels(fbo->width/2, fbo->height/2, 1, 1, GL_RGBA, GL_FLOAT, pixel);
        gLog.Info("Composite '%s' 中心像素: (%.3f, %.3f, %.3f, %.3f)",
                  passName.c_str(), pixel[0], pixel[1], pixel[2], pixel[3]);

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

    // 诊断：检查输入 FBO
    if (srcFbo) {
        gLog.Info("Final: 输入 FBO colorTex=%u, 尺寸=%dx%d",
                  srcFbo->colorTexture, srcFbo->width, srcFbo->height);

        // 读取输入纹理的像素值
        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo->fbo);
        float pixel[4];
        glReadPixels(srcFbo->width/2, srcFbo->height/2, 1, 1, GL_RGBA, GL_FLOAT, pixel);
        gLog.Info("Final: 输入纹理中心像素: (%.3f, %.3f, %.3f, %.3f)",
                  pixel[0], pixel[1], pixel[2], pixel[3]);
    }

    // 如果没有 final pass，使用默认的 blit 操作
    if (!pass)
    {
        if (!srcFbo) {
            gLog.Error("Final: 无输入 FBO");
            return;
        }

        gLog.Info("Final: 使用 blit 操作");

        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo->fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, srcFbo->width, srcFbo->height,
                          0, 0, m_ScreenWidth, m_ScreenHeight,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 诊断：检查 blit 后屏幕像素
        float pixel[4];
        glReadPixels(m_ScreenWidth/2, m_ScreenHeight/2, 1, 1, GL_RGBA, GL_FLOAT, pixel);
        gLog.Info("Final: blit 后屏幕中心像素: (%.3f, %.3f, %.3f, %.3f)",
                  pixel[0], pixel[1], pixel[2], pixel[3]);
        return;
    }

    // 使用 final pass 着色器
    gLog.Info("Final: 使用 final pass 着色器");

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

    // 诊断：检查 GL 错误
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        gLog.Error("Final GL 错误: 0x%X", err);
    }

    // 诊断：读取屏幕像素
    float pixel[4];
    glReadPixels(m_ScreenWidth/2, m_ScreenHeight/2, 1, 1, GL_RGBA, GL_FLOAT, pixel);
    gLog.Info("Final: 屏幕中心像素: (%.3f, %.3f, %.3f, %.3f)",
              pixel[0], pixel[1], pixel[2], pixel[3]);
}

void ShaderPackRenderer::DrawFullscreenQuad()
{
    glBindVertexArray(m_QuadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
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
            // 有效光照 = max(方块光, 天空光)
            float level = (float)std::max(block, sky) / 15.0f;
            // Minecraft 原版 gamma 曲线近似（gamma ≈ 1.5），底部保留最低环境光
            float brightness = level * level * level * 0.85f + 0.15f;
            brightness = std::min(brightness, 1.0f);

            unsigned char v = (unsigned char)(brightness * 255.0f);
            // 暖白色调（略微偏黄，模拟 Minecraft 火把光）
            int idx = (sky * SIZE + block) * 3;
            pixels[idx + 0] = v;                          // R
            pixels[idx + 1] = (unsigned char)(v * 0.95f); // G（略低）
            pixels[idx + 2] = (unsigned char)(v * 0.85f); // B（更暖）
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
