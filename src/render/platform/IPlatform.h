#pragma once

#include <string>

// 输入事件结构
struct InputEvent
{
    enum Type { MouseMove, MouseButton, Scroll, Key };
    Type type;
    double x, y;        // 鼠标位置/滚轮偏移
    int button;         // 鼠标按键
    int action;         // 按下(1)/释放(0)
    int key;            // 键盘按键
};

// 输入事件回调
using InputCallback = void(*)(const InputEvent &event, void *userData);

// 平台抽象接口：定义窗口、输入、时间等平台相关功能
class IPlatform
{
public:
    virtual ~IPlatform() = default;

    // 窗口管理
    virtual bool Init(int width, int height, const std::string &title) = 0;
    virtual void Shutdown() = 0;
    virtual bool ShouldClose() const = 0;
    virtual void RequestClose() = 0;
    virtual void PollEvents() = 0;
    virtual void SwapBuffers() = 0;

    // 画面
    virtual void Clear(float r, float g, float b) = 0;
    virtual void GetFramebufferSize(int &width, int &height) = 0;

    // 输入
    virtual void SetInputCallback(InputCallback callback, void *userData) = 0;
    virtual bool IsKeyDown(int key) const = 0;
    virtual double GetTime() const = 0;

    // 键码常量（统一定义，避免外部依赖）
    static constexpr int KEY_ESCAPE = 256;
    static constexpr int KEY_W = 87;
    static constexpr int KEY_A = 65;
    static constexpr int KEY_S = 83;
    static constexpr int KEY_D = 68;
    static constexpr int KEY_F = 70;
    static constexpr int KEY_G = 71;
    static constexpr int KEY_0 = 48;
    static constexpr int KEY_1 = 49;
    static constexpr int KEY_2 = 50;
};
