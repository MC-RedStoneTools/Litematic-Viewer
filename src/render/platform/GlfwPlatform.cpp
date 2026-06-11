#include "GlfwPlatform.h"
#include "../../utils/Log.h"
#include <glad/gl.h>

static LogSource gLog("GlfwPlatform");

// 初始化 GLFW 窗口和 OpenGL 上下文
bool GlfwPlatform::Init(int width, int height, const std::string &title)
{
    if (!glfwInit())
    {
        gLog.Error("GLFW 初始化失败");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_FALSE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    m_Window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_Window)
    {
        gLog.Error("窗口创建失败");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_Window);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);

    // 加载 OpenGL 函数
    int version = gladLoadGL(glfwGetProcAddress);
    if (!version)
    {
        gLog.Error("GLAD 初始化失败");
        glfwTerminate();
        return false;
    }
    gLog.Info("OpenGL 版本: %d.%d", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);

    return true;
}

void GlfwPlatform::Shutdown()
{
    if (m_Window)
    {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    glfwTerminate();
}

bool GlfwPlatform::ShouldClose() const
{
    return glfwWindowShouldClose(m_Window);
}

void GlfwPlatform::RequestClose()
{
    glfwSetWindowShouldClose(m_Window, true);
}

void GlfwPlatform::PollEvents()
{
    glfwPollEvents();
}

void GlfwPlatform::SwapBuffers()
{
    glfwSwapBuffers(m_Window);
}

void GlfwPlatform::Clear(float r, float g, float b)
{
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GlfwPlatform::GetFramebufferSize(int &width, int &height)
{
    glfwGetFramebufferSize(m_Window, &width, &height);
}

void GlfwPlatform::SetInputCallback(InputCallback callback, void *userData)
{
    m_Callback = callback;
    m_UserData = userData;

    // 注册 GLFW 回调，传递 this 指针以便回调中访问 m_Callback
    glfwSetWindowUserPointer(m_Window, this);
    glfwSetMouseButtonCallback(m_Window, MouseButtonCallback);
    glfwSetCursorPosCallback(m_Window, CursorPosCallback);
    glfwSetScrollCallback(m_Window, ScrollCallback);
    glfwSetKeyCallback(m_Window, KeyCallback);
}

bool GlfwPlatform::IsKeyDown(int key) const
{
    return glfwGetKey(m_Window, key) == GLFW_PRESS;
}

double GlfwPlatform::GetTime() const
{
    return glfwGetTime();
}

// GLFW 回调转接到 InputCallback
void GlfwPlatform::MouseButtonCallback(GLFWwindow *w, int button, int action, int mods)
{
    GlfwPlatform *self = static_cast<GlfwPlatform *>(glfwGetWindowUserPointer(w));
    if (!self || !self->m_Callback) return;

    InputEvent event;
    event.type = InputEvent::MouseButton;
    event.button = button;
    event.action = (action == GLFW_PRESS) ? 1 : 0;
    glfwGetCursorPos(w, &event.x, &event.y);
    self->m_Callback(event, self->m_UserData);
}

void GlfwPlatform::CursorPosCallback(GLFWwindow *w, double x, double y)
{
    GlfwPlatform *self = static_cast<GlfwPlatform *>(glfwGetWindowUserPointer(w));
    if (!self || !self->m_Callback) return;

    InputEvent event;
    event.type = InputEvent::MouseMove;
    event.x = x;
    event.y = y;
    self->m_Callback(event, self->m_UserData);
}

void GlfwPlatform::ScrollCallback(GLFWwindow *w, double x, double y)
{
    GlfwPlatform *self = static_cast<GlfwPlatform *>(glfwGetWindowUserPointer(w));
    if (!self || !self->m_Callback) return;

    InputEvent event;
    event.type = InputEvent::Scroll;
    event.x = x;
    event.y = y;
    self->m_Callback(event, self->m_UserData);
}

void GlfwPlatform::KeyCallback(GLFWwindow *w, int key, int scancode, int action, int mods)
{
    GlfwPlatform *self = static_cast<GlfwPlatform *>(glfwGetWindowUserPointer(w));
    if (!self || !self->m_Callback) return;

    InputEvent event;
    event.type = InputEvent::Key;
    event.key = key;
    event.action = (action == GLFW_PRESS) ? 1 : 0;
    self->m_Callback(event, self->m_UserData);
}

void GlfwPlatform::FramebufferSizeCallback(GLFWwindow *w, int width, int height)
{
    glViewport(0, 0, width, height);
}
