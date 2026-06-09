#include "RenderLoop.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "../../utils/Log.h"

static LogSource gLog("RenderLoop");

RenderLoop *RenderLoop::s_Instance = nullptr;

void RenderLoop::MouseButtonCallback(GLFWwindow *w, int button, int action, int)
{
    if (!s_Instance) return;
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        s_Instance->m_MousePressed = (action == GLFW_PRESS);
        glfwGetCursorPos(w, &s_Instance->m_LastMouseX, &s_Instance->m_LastMouseY);
    }
}

void RenderLoop::CursorPosCallback(GLFWwindow *, double xpos, double ypos)
{
    if (!s_Instance || !s_Instance->m_MousePressed) return;

    // 通过静态实例获取App引用
    App &app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(
        glfwGetCurrentContext()));

    float dx = static_cast<float>(xpos - s_Instance->m_LastMouseX);
    float dy = static_cast<float>(s_Instance->m_LastMouseY - ypos);
    app.GetCamera().OnMouseMove(dx, dy);
    s_Instance->m_LastMouseX = xpos;
    s_Instance->m_LastMouseY = ypos;
}

void RenderLoop::ScrollCallback(GLFWwindow *, double, double yoffset)
{
    if (!s_Instance) return;

    App &app = *reinterpret_cast<App *>(glfwGetWindowUserPointer(
        glfwGetCurrentContext()));
    app.GetCamera().OnScroll(static_cast<float>(yoffset));
}

void RenderLoop::HandleInput(GLFWwindow *handle, App &app)
{
    if (glfwGetKey(handle, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(handle, true);

    // WASD 移动摄像机
    Camera &camera = app.GetCamera();
    float forward = 0, strafe = 0;
    if (glfwGetKey(handle, GLFW_KEY_W) == GLFW_PRESS) forward += 1.0f;
    if (glfwGetKey(handle, GLFW_KEY_S) == GLFW_PRESS) forward -= 1.0f;
    if (glfwGetKey(handle, GLFW_KEY_D) == GLFW_PRESS) strafe += 1.0f;
    if (glfwGetKey(handle, GLFW_KEY_A) == GLFW_PRESS) strafe -= 1.0f;
    if (forward != 0 || strafe != 0)
        camera.Move(forward, strafe);

    // 调试模式切换
    static bool key1Pressed = false, key2Pressed = false, key0Pressed = false;
    if (glfwGetKey(handle, GLFW_KEY_1) == GLFW_PRESS) { if (!key1Pressed) { app.SetDebugMode(1); gLog.Info("调试模式: 面方向着色"); } key1Pressed = true; } else key1Pressed = false;
    if (glfwGetKey(handle, GLFW_KEY_2) == GLFW_PRESS) { if (!key2Pressed) { app.SetDebugMode(2); gLog.Info("调试模式: 线框"); } key2Pressed = true; } else key2Pressed = false;
    if (glfwGetKey(handle, GLFW_KEY_0) == GLFW_PRESS) { if (!key0Pressed) { app.SetDebugMode(0); gLog.Info("调试模式: 关闭（正常渲染）"); } key0Pressed = true; } else key0Pressed = false;
}

int RenderLoop::Run(App &app)
{
    s_Instance = this;
    m_DebugMode = app.GetDebugMode();

    Window &window = app.GetWindow();
    Shader &shader = app.GetShader();
    Renderer &renderer = app.GetRenderer();
    TextureManager &texMgr = app.GetTextureManager();
    Camera &camera = app.GetCamera();
    const PipelineContext &ctx = app.GetContext();

    GLFWwindow *handle = window.GetHandle();

    // 存储App指针到窗口，供回调使用
    glfwSetWindowUserPointer(handle, &app);

    // 注册回调
    glfwSetMouseButtonCallback(handle, MouseButtonCallback);
    glfwSetCursorPosCallback(handle, CursorPosCallback);
    glfwSetScrollCallback(handle, ScrollCallback);

    float model[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    while (!window.ShouldClose())
    {
        HandleInput(handle, app);

        int width, height;
        glfwGetFramebufferSize(handle, &width, &height);
        camera.UpdateProjectionMatrix(width, height);
        camera.UpdateViewMatrix();

        window.Clear(0.15f, 0.15f, 0.15f);

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

        window.PollEvents();
    }

    return 0;
}
