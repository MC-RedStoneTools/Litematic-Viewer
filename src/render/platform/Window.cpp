#include "Window.h"
#include "../../utils/Log.h"

static LogSource gLog("Window");

bool Window::Init(int width, int height, const std::string &title)
{
    // 初始化GLFW
    if (!glfwInit())
    {
        gLog.Error("GLFW初始化失败");
        return false;
    }

    // 设置OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 创建窗口
    m_Window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_Window)
    {
        gLog.Error("窗口创建失败");
        glfwTerminate();
        return false;
    }

    // 设置OpenGL上下文为当前线程
    glfwMakeContextCurrent(m_Window);
    glfwSetFramebufferSizeCallback(m_Window, FramebufferSizeCallback);

    // 用GLAD加载OpenGL函数
    int version = gladLoadGL(glfwGetProcAddress);
    if (!version)
    {
        gLog.Error("GLAD初始化失败");
        glfwTerminate();
        return false;
    }
    gLog.Info("OpenGL版本: %d.%d", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));

    // 设置视口和深度测试
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);

    return true;
}

bool Window::ShouldClose() const
{
    return glfwWindowShouldClose(m_Window);
}

void Window::PollEvents() const
{
    glfwSwapBuffers(m_Window);
    glfwPollEvents();
}

void Window::Clear(float r, float g, float b) const
{
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::Shutdown()
{
    if (m_Window)
    {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    glfwTerminate();
}

void Window::FramebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}
