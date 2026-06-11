#include "RenderLoop.h"
#include "InputHandler.h"

#include "../../utils/Log.h"
#include "../../shader_pack/runtime/ShaderPackRuntime.h"
#include "../../shader_pack/render/ShaderPackRenderer.h"

static LogSource gLog("RenderLoop");

int RenderLoop::Run(IPlatform &platform, App &app)
{
    Shader &shader = app.GetShader();
    Renderer &renderer = app.GetRenderer();
    TextureManager &texMgr = app.GetTextureManager();
    Camera &camera = app.GetCamera();
    const PipelineContext &ctx = app.GetContext();

    // 注册输入
    InputHandler::SetApp(&app);
    platform.SetInputCallback(InputHandler::OnInputEvent, nullptr);

    float model[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    // 光影包渲染器
    ShaderPackRenderer shaderPackRenderer;
    bool hasShaderPack = (ctx.shaderPackRuntime != nullptr && ctx.shaderPackRuntime->IsReady());
    bool useShaderPack = hasShaderPack && app.IsShaderPackEnabled();
    fprintf(stderr, "[DEBUG] RenderLoop: hasShaderPack=%d, useShaderPack=%d\n",
            hasShaderPack, useShaderPack);
    fflush(stderr);

    if (useShaderPack)
    {
        int fbWidth, fbHeight;
        platform.GetFramebufferSize(fbWidth, fbHeight);
        fprintf(stderr, "[DEBUG] RenderLoop: 调用 InitShaderPackVAO 前\n");
        fflush(stderr);
        renderer.InitShaderPackVAO(); // 创建兼容 compatibility 模式的 VAO
        fprintf(stderr, "[DEBUG] RenderLoop: 调用 InitShaderPackVAO 后\n");
        fflush(stderr);
        shaderPackRenderer.Init(*ctx.shaderPackRuntime, fbWidth, fbHeight);
        gLog.Info("光影包渲染模式已启用");
    }

    double lastTime = platform.GetTime();

    while (!platform.ShouldClose())
    {
        InputHandler::HandleKeyboard(platform, app);

        int width, height;
        platform.GetFramebufferSize(width, height);
        camera.UpdateProjectionMatrix(width, height);
        camera.UpdateViewMatrix();

        // 动态检查是否使用光影包（支持运行时切换）
        bool currentUseShaderPack = hasShaderPack && app.IsShaderPackEnabled();

        // 调试输出（每60帧输出一次）
        static int frameCount = 0;
        frameCount++;
        if (frameCount % 60 == 1) {
            fprintf(stderr, "[DEBUG] Frame %d: currentUseShaderPack=%d, enableShadow=%d, meshEmpty=%d\n",
                    frameCount, currentUseShaderPack, app.IsShadowEnabled(), ctx.mesh.IsEmpty());
            fflush(stderr);
        }

        if (currentUseShaderPack)
        {
            // 渲染光影包之前，先清空主屏幕（防止某些 Pass 未覆盖全屏导致黑屏或重影）
            platform.Clear(0.0f, 0.0f, 0.0f);

            shaderPackRenderer.SetScreenSize(width, height);
            float dt = static_cast<float>(platform.GetTime() - lastTime);
            lastTime = platform.GetTime();
            shaderPackRenderer.Render(*ctx.shaderPackRuntime, camera, ctx.mesh, texMgr, renderer, dt);
        }
        else
        {
            // 确保视口和状态正确（从光影包切换回来时需要）
            glViewport(0, 0, width, height);
            platform.Clear(0.15f, 0.15f, 0.15f);

            bool useShadow = app.IsShadowEnabled() && !ctx.mesh.IsEmpty();

            // 主渲染 Pass（点光源）
            if (useShadow)
            {
                Shader &shadowMainShader = app.GetShadowMainShader();

                shadowMainShader.Use();
                shadowMainShader.SetMat4("model", model);
                shadowMainShader.SetMat4("view", camera.GetViewMatrix());
                shadowMainShader.SetMat4("projection", camera.GetProjectionMatrix());

                // 点光源参数：位于场景中心上方
                float lightPos[3] = { 2.5f, 8.0f, 2.5f };
                float lightColor[3] = { 1.0f, 0.95f, 0.9f };
                float lightIntensity = 8.0f;
                glUniform3fv(glGetUniformLocation(shadowMainShader.GetID(), "lightPos"), 1, lightPos);
                glUniform3fv(glGetUniformLocation(shadowMainShader.GetID(), "lightColor"), 1, lightColor);
                glUniform1f(glGetUniformLocation(shadowMainShader.GetID(), "lightIntensity"), lightIntensity);

                if (!ctx.mesh.IsEmpty())
                {
                    int debugMode = app.GetDebugMode();
                    if (debugMode != 0)
                        renderer.DrawDebug(shadowMainShader, debugMode);
                    else
                        renderer.Draw(shadowMainShader, texMgr, ctx.mesh);
                }
            }
            else
            {
                // 使用原始着色器（无阴影）
                shader.Use();
                shader.SetMat4("model", model);
                shader.SetMat4("view", camera.GetViewMatrix());
                shader.SetMat4("projection", camera.GetProjectionMatrix());

                if (!ctx.mesh.IsEmpty())
                {
                    int debugMode = app.GetDebugMode();
                    if (debugMode != 0)
                        renderer.DrawDebug(shader, debugMode);
                    else
                        renderer.Draw(shader, texMgr, ctx.mesh);
                }
            }
        }

        platform.SwapBuffers();
        platform.PollEvents();
    }

    if (hasShaderPack)
        shaderPackRenderer.Destroy();

    InputHandler::SetApp(nullptr);

    return 0;
}
