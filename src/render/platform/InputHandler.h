#pragma once

#include "IPlatform.h"

class App;

// 输入处理器：独立管理鼠标事件和键盘轮询
// 不依赖具体平台实现（GLFW/SDL/...），只通过 IPlatform 接口交互
class InputHandler
{
public:
    // 注册到平台的输入回调（鼠标/滚轮事件）
    static void OnInputEvent(const InputEvent &event, void *userData);

    // 每帧调用，处理键盘轮询
    static void HandleKeyboard(IPlatform &platform, App &app);

    // 设置 App 引用（注册回调前调用）
    static void SetApp(App *app) { s_App = app; }

private:
    static App *s_App;

    // 鼠标拖拽状态
    static bool s_MousePressed;
    static double s_LastMouseX;
    static double s_LastMouseY;

    // 调试模式边沿检测
    static bool s_Key1Pressed;
    static bool s_Key2Pressed;
    static bool s_Key0Pressed;
    static bool s_KeyFPressed; // 阴影开关
    static bool s_KeyGPressed; // 光影包开关
};
