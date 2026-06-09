#include "Camera.h"
#include "../../utils/Log.h"
#include <algorithm>
#include <cmath>

static LogSource gLog("Camera");

static constexpr float DEG2RAD = 3.14159265f / 180.0f;

// 球坐标转笛卡尔坐标，计算相机位置
void Camera::CalcEyePosition(float &eyeX, float &eyeY, float &eyeZ) const
{
    float yawRad = m_Yaw * DEG2RAD;
    float pitchRad = m_Pitch * DEG2RAD;

    eyeX = m_TargetX + m_Distance * cosf(pitchRad) * cosf(yawRad);
    eyeY = m_TargetY + m_Distance * sinf(pitchRad);
    eyeZ = m_TargetZ + m_Distance * cosf(pitchRad) * sinf(yawRad);
}

void Camera::UpdateViewMatrix()
{
    float eyeX, eyeY, eyeZ;
    CalcEyePosition(eyeX, eyeY, eyeZ);

    MatrixLookAt(m_View, eyeX, eyeY, eyeZ, m_TargetX, m_TargetY, m_TargetZ, 0.0f, 1.0f, 0.0f);
}

// 调试：打印相机信息
void Camera::PrintDebugInfo() const
{
    float eyeX, eyeY, eyeZ;
    CalcEyePosition(eyeX, eyeY, eyeZ);

    gLog.Info("相机调试:");
    gLog.Info("  目标: (%.2f, %.2f, %.2f)", m_TargetX, m_TargetY, m_TargetZ);
    gLog.Info("  位置: (%.2f, %.2f, %.2f)", eyeX, eyeY, eyeZ);
    gLog.Info("  距离: %.2f, yaw: %.2f, pitch: %.2f", m_Distance, m_Yaw, m_Pitch);
}

void Camera::UpdateProjectionMatrix(int width, int height)
{
    float aspect = static_cast<float>(width) / static_cast<float>(height > 0 ? height : 1);
    MatrixPerspective(m_Projection, 45.0f, aspect, 0.1f, 500.0f);
}

void Camera::OnMouseMove(float deltaX, float deltaY)
{
    // 鼠标水平移动 → 改变yaw，垂直移动 → 改变pitch
    m_Yaw += deltaX * 0.3f;
    m_Pitch += deltaY * 0.3f;

    // 限制俯仰角范围
    m_Pitch = std::clamp(m_Pitch, -89.0f, 89.0f);
}

void Camera::OnScroll(float offset)
{
    m_Distance -= offset * 1.0f;
    m_Distance = std::clamp(m_Distance, 1.0f, 200.0f);
}

// 移动摄像机：根据yaw计算方向向量，移动目标点
// W:向前(远离相机), S:向后(靠近相机), A:向左, D:向右
void Camera::Move(float forward, float strafe)
{
    float yawRad = m_Yaw * DEG2RAD;

    // 前方向：从相机指向target（W键效果为向前）
    float fx = -cosf(yawRad);
    float fz = -sinf(yawRad);

    // 右方向（前方向顺时针旋转90度）
    float rx = -fz;
    float rz = fx;

    float speed = m_Distance * 0.02f; // 移动速度与距离成正比
    m_TargetX += (forward * fx + strafe * rx) * speed;
    m_TargetZ += (forward * fz + strafe * rz) * speed;
}

void Camera::MatrixPerspective(float *m, float fov, float aspect, float near, float far)
{
    // 列主序透视投影矩阵
    float tanHalf = tanf(fov * DEG2RAD * 0.5f);
    for (int i = 0; i < 16; i++) m[i] = 0.0f;

    m[0]  = 1.0f / (aspect * tanHalf);
    m[5]  = 1.0f / tanHalf;
    m[10] = -(far + near) / (far - near);
    m[11] = -1.0f;
    m[14] = -(2.0f * far * near) / (far - near);
}

void Camera::MatrixLookAt(float *m, float eyeX, float eyeY, float eyeZ,
                           float targetX, float targetY, float targetZ,
                           float upX, float upY, float upZ)
{
    // forward = normalize(target - eye)
    float fx = targetX - eyeX, fy = targetY - eyeY, fz = targetZ - eyeZ;
    float fLen = sqrtf(fx * fx + fy * fy + fz * fz);
    fx /= fLen; fy /= fLen; fz /= fLen;

    // right = normalize(cross(forward, up))
    float rx = fy * upZ - fz * upY;
    float ry = fz * upX - fx * upZ;
    float rz = fx * upY - fy * upX;
    float rLen = sqrtf(rx * rx + ry * ry + rz * rz);
    rx /= rLen; ry /= rLen; rz /= rLen;

    // newUp = cross(right, forward)
    float ux = ry * fz - rz * fy;
    float uy = rz * fx - rx * fz;
    float uz = rx * fy - ry * fx;

    // 列主序lookAt矩阵
    m[0]  = rx;  m[4]  = ry;  m[8]  = rz;  m[12] = -(rx * eyeX + ry * eyeY + rz * eyeZ);
    m[1]  = ux;  m[5]  = uy;  m[9]  = uz;  m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
    m[2]  = -fx; m[6]  = -fy; m[10] = -fz; m[14] = (fx * eyeX + fy * eyeY + fz * eyeZ);
    m[3]  = 0;   m[7]  = 0;   m[11] = 0;   m[15] = 1.0f;
}
