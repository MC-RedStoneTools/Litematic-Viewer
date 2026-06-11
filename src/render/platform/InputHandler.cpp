#include "InputHandler.h"
#include "../../app/App.h"
#include "../../utils/Log.h"

static LogSource gLog("Input");

// 静态成员初始化
App *InputHandler::s_App = nullptr;
bool InputHandler::s_MousePressed = false;
double InputHandler::s_LastMouseX = 0.0;
double InputHandler::s_LastMouseY = 0.0;
bool InputHandler::s_Key1Pressed = false;
bool InputHandler::s_Key2Pressed = false;
bool InputHandler::s_Key0Pressed = false;
bool InputHandler::s_KeyFPressed = false;
bool InputHandler::s_KeyGPressed = false;

// ============================================================
// 鼠标/滚轮事件回调（由平台回调触发）
// ============================================================
void InputHandler::OnInputEvent(const InputEvent &event, void *)
{
    if (!s_App) return;

    switch (event.type)
    {
    case InputEvent::MouseButton:
        if (event.button == 0) // 左键
        {
            s_MousePressed = (event.action == 1);
            s_LastMouseX = event.x;
            s_LastMouseY = event.y;
        }
        break;

    case InputEvent::MouseMove:
        if (s_MousePressed)
        {
            float dx = static_cast<float>(event.x - s_LastMouseX);
            float dy = static_cast<float>(s_LastMouseY - event.y);
            s_App->GetCamera().OnMouseMove(dx, dy);
            s_LastMouseX = event.x;
            s_LastMouseY = event.y;
        }
        break;

    case InputEvent::Scroll:
        s_App->GetCamera().OnScroll(static_cast<float>(event.y));
        break;

    case InputEvent::Key:
        break;
    }
}

// ============================================================
// 键盘轮询（每帧调用）
// ============================================================
void InputHandler::HandleKeyboard(IPlatform &platform, App &app)
{
    // ESC 关闭窗口
    if (platform.IsKeyDown(IPlatform::KEY_ESCAPE))
        platform.RequestClose();

    // WASD 移动摄像机
    Camera &camera = app.GetCamera();
    float forward = 0, strafe = 0;
    if (platform.IsKeyDown(IPlatform::KEY_W)) forward += 1.0f;
    if (platform.IsKeyDown(IPlatform::KEY_S)) forward -= 1.0f;
    if (platform.IsKeyDown(IPlatform::KEY_D)) strafe += 1.0f;
    if (platform.IsKeyDown(IPlatform::KEY_A)) strafe -= 1.0f;
    if (forward != 0 || strafe != 0)
        camera.Move(forward, strafe);

    // 调试模式切换（边沿检测）
    if (platform.IsKeyDown(IPlatform::KEY_1)) {
        if (!s_Key1Pressed) { app.SetDebugMode(1); gLog.Info("调试模式: 面方向着色"); }
        s_Key1Pressed = true;
    } else s_Key1Pressed = false;

    if (platform.IsKeyDown(IPlatform::KEY_2)) {
        if (!s_Key2Pressed) { app.SetDebugMode(2); gLog.Info("调试模式: 线框"); }
        s_Key2Pressed = true;
    } else s_Key2Pressed = false;

    if (platform.IsKeyDown(IPlatform::KEY_0)) {
        if (!s_Key0Pressed) { app.SetDebugMode(0); gLog.Info("调试模式: 关闭（正常渲染）"); }
        s_Key0Pressed = true;
    } else s_Key0Pressed = false;

    // F 键切换阴影（边沿检测）
    if (platform.IsKeyDown(IPlatform::KEY_F)) {
        if (!s_KeyFPressed) {
            bool newState = !app.IsShadowEnabled();
            app.SetShadowEnabled(newState);
            gLog.Info("阴影: %s", newState ? "开启" : "关闭");
        }
        s_KeyFPressed = true;
    } else s_KeyFPressed = false;

    // G 键切换光影包（边沿检测）
    if (platform.IsKeyDown(IPlatform::KEY_G)) {
        if (!s_KeyGPressed) {
            bool newState = !app.IsShaderPackEnabled();
            app.SetShaderPackEnabled(newState);
            gLog.Info("光影包: %s", newState ? "开启" : "关闭");
        }
        s_KeyGPressed = true;
    } else s_KeyGPressed = false;
}
