#pragma once

#include "IPlatform.h"

// 避免 GLFW 引入系统 GL 头头文件（使用 GLAD）
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// GLFW 平台实现：封装 GLFW 窗口和输入管理
class GlfwPlatform : public IPlatform
{
public:
    // IPlatform 接口实现
    bool Init(int width, int height, const std::string &title) override;
    void Shutdown() override;
    bool ShouldClose() const override;
    void RequestClose() override;
    void PollEvents() override;
    void SwapBuffers() override;
    void Clear(float r, float g, float b) override;
    void GetFramebufferSize(int &width, int &height) override;
    void SetInputCallback(InputCallback callback, void *userData) override;
    bool IsKeyDown(int key) const override;
    double GetTime() const override;

    // 获取 GLFW 窗口句柄（特殊情况使用）
    GLFWwindow *GetHandle() const { return m_Window; }

private:
    GLFWwindow *m_Window = nullptr;
    InputCallback m_Callback = nullptr;
    void *m_UserData = nullptr;

    // GLFW 回调转接到 InputCallback
    static void MouseButtonCallback(GLFWwindow *w, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow *w, double x, double y);
    static void ScrollCallback(GLFWwindow *w, double x, double y);
    static void KeyCallback(GLFWwindow *w, int key, int scancode, int action, int mods);
    static void FramebufferSizeCallback(GLFWwindow *w, int width, int height);
};
