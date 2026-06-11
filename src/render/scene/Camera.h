#pragma once

#include <cmath>

// 简易相机类：绕目标点旋转 + 滚轮缩放
class Camera
{
public:
    // 更新View矩阵（lookAt）
    void UpdateViewMatrix();

    // 更新Projection矩阵（透视投影）
    void UpdateProjectionMatrix(int width, int height);

    // 处理鼠标输入
    void OnMouseMove(float deltaX, float deltaY);
    void OnScroll(float offset);

    // 移动摄像机（根据当前朝向）
    // forward: 前后移动，strafe: 左右移动
    void Move(float forward, float strafe);

    // 获取矩阵指针
    const float *GetViewMatrix() const { return m_View; }
    const float *GetProjectionMatrix() const { return m_Projection; }

    // 设置目标点（通常是模型中心）
    void SetTarget(float x, float y, float z) { m_TargetX = x; m_TargetY = y; m_TargetZ = z; }

    // 获取目标点
    void GetTarget(float &x, float &y, float &z) const { x = m_TargetX; y = m_TargetY; z = m_TargetZ; }

    // 获取相机位置
    void GetPosition(float &x, float &y, float &z) const { CalcEyePosition(x, y, z); }

    // 调试：打印相机信息
    void PrintDebugInfo() const;

private:
    // 球坐标参数
    float m_Yaw = 45.0f;     // 水平角度
    float m_Pitch = 30.0f;    // 垂直角度
    float m_Distance = 20.0f; // 距离目标的距离

    // 目标点
    float m_TargetX = 0.0f;
    float m_TargetY = 0.0f;
    float m_TargetZ = 0.0f;

    // 矩阵（列主序）
    float m_View[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    float m_Projection[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};

    // 计算相机位置（球坐标转笛卡尔）
    void CalcEyePosition(float &eyeX, float &eyeY, float &eyeZ) const;

    // 辅助函数
    static void MatrixPerspective(float *m, float fov, float aspect, float near, float far);
    static void MatrixLookAt(float *m, float eyeX, float eyeY, float eyeZ,
                             float targetX, float targetY, float targetZ,
                             float upX, float upY, float upZ);
};
