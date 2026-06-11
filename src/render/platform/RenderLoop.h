#pragma once

#include "../../app/App.h"
#include "IPlatform.h"

// 渲染循环：只负责帧循环和渲染调度
// 输入处理委托给 InputHandler
class RenderLoop
{
public:
    // 运行渲染循环，返回退出码
    int Run(IPlatform &platform, App &app);
};
