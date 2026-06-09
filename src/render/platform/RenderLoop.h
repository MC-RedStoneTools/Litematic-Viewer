#pragma once

#include "../../app/App.h"

// 渲染循环：只负责帧循环和输入处理
class RenderLoop
{
public:
    // 运行渲染循环，返回退出码
    int Run(App &app);

private:
    int m_DebugMode = 0;
    bool m_MousePressed = false;
    double m_LastMouseX = 0, m_LastMouseY = 0;

    // 指向当前App实例，用于回调访问
    static RenderLoop *s_Instance;

    // GLFW回调
    static void MouseButtonCallback(GLFWwindow *w, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow *w, double x, double y);
    static void ScrollCallback(GLFWwindow *w, double x, double y);

    // 处理键盘输入
    void HandleInput(GLFWwindow *handle, App &app);
};
