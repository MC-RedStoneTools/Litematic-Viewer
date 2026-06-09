#pragma once

#include <string>
#include <glad/gl.h>
#include <GLFW/glfw3.h>

// 窗口管理类：创建窗口、初始化OpenGL上下文、管理渲染循环
class Window
{
public:
    // 创建窗口并初始化OpenGL
    bool Init(int width, int height, const std::string &title);

    // 窗口是否应该关闭
    bool ShouldClose() const;

    // 交换缓冲区并处理事件
    void PollEvents() const;

    // 清屏
    void Clear(float r, float g, float b) const;

    // 销毁窗口并清理GLFW
    void Shutdown();

    // 获取GLFW窗口指针（用于输入处理等）
    GLFWwindow *GetHandle() const { return m_Window; }

private:
    GLFWwindow *m_Window = nullptr;

    // 窗口尺寸改变回调
    static void FramebufferSizeCallback(GLFWwindow *window, int width, int height);
};
