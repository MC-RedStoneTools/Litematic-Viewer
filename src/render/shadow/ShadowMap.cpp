#include "ShadowMap.h"
#include "../../utils/Log.h"
#include <cmath>
#include <cstring>

static LogSource gLog("ShadowMap");

// 简单的矩阵工具函数
namespace MatUtil
{
    static void Identity(float *m)
    {
        memset(m, 0, 16 * sizeof(float));
        m[0] = m[5] = m[10] = m[15] = 1.0f;
    }

    // 正交投影矩阵
    static void Ortho(float *m, float left, float right, float bottom, float top, float nearVal, float farVal)
    {
        Identity(m);
        m[0] = 2.0f / (right - left);
        m[5] = 2.0f / (top - bottom);
        m[10] = -2.0f / (farVal - nearVal);
        m[12] = -(right + left) / (right - left);
        m[13] = -(top + bottom) / (top - bottom);
        m[14] = -(farVal + nearVal) / (farVal - nearVal);
    }

    // LookAt 视图矩阵
    static void LookAt(float *m, float eyeX, float eyeY, float eyeZ,
                       float centerX, float centerY, float centerZ,
                       float upX, float upY, float upZ)
    {
        float fx = centerX - eyeX, fy = centerY - eyeY, fz = centerZ - eyeZ;
        float len = sqrtf(fx * fx + fy * fy + fz * fz);
        if (len > 0) { fx /= len; fy /= len; fz /= len; }

        // s = f x up
        float sx = fy * upZ - fz * upY;
        float sy = fz * upX - fx * upZ;
        float sz = fx * upY - fy * upX;
        len = sqrtf(sx * sx + sy * sy + sz * sz);
        if (len > 0) { sx /= len; sy /= len; sz /= len; }

        // u = s x f
        float ux = sy * fz - sz * fy;
        float uy = sz * fx - sx * fz;
        float uz = sx * fy - sy * fx;

        Identity(m);
        m[0] = sx;  m[4] = sy;  m[8]  = sz;
        m[1] = ux;  m[5] = uy;  m[9]  = uz;
        m[2] = -fx; m[6] = -fy; m[10] = -fz;
        m[12] = -(sx * eyeX + sy * eyeY + sz * eyeZ);
        m[13] = -(ux * eyeX + uy * eyeY + uz * eyeZ);
        m[14] = (fx * eyeX + fy * eyeY + fz * eyeZ);
    }

    // 矩阵乘法 C = A * B (列主序)
    static void Multiply(float *c, const float *a, const float *b)
    {
        float tmp[16];
        for (int col = 0; col < 4; col++)
            for (int row = 0; row < 4; row++)
                tmp[col * 4 + row] = a[row] * b[col * 4] + a[4 + row] * b[col * 4 + 1] +
                                     a[8 + row] * b[col * 4 + 2] + a[12 + row] * b[col * 4 + 3];
        memcpy(c, tmp, sizeof(tmp));
    }
}

ShadowMap::~ShadowMap()
{
    Destroy();
}

bool ShadowMap::Init(int resolution)
{
    m_Resolution = resolution;

    // 创建深度纹理
    glGenTextures(1, &m_DepthTexture);
    glBindTexture(GL_TEXTURE_2D, m_DepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
                 resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // 创建 FBO
    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // 检查 FBO 完整性
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        gLog.Error("阴影贴图 FBO 创建失败");
        Destroy();
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    gLog.Info("阴影贴图初始化成功: %dx%d", resolution, resolution);
    return true;
}

void ShadowMap::SetLightDirection(float x, float y, float z)
{
    float len = sqrtf(x * x + y * y + z * z);
    if (len > 0) { m_LightDir[0] = x / len; m_LightDir[1] = y / len; m_LightDir[2] = z / len; }
}

void ShadowMap::CalcLightSpaceMatrix(const Camera &camera)
{
    // 获取相机位置和目标，计算场景包围盒
    float camPos[3], camTarget[3];
    camera.GetPosition(camPos[0], camPos[1], camPos[2]);
    camera.GetTarget(camTarget[0], camTarget[1], camTarget[2]);

    // 计算场景中心和半径（简化：使用相机位置和目标的中点）
    float sceneCenter[3] = {
        (camPos[0] + camTarget[0]) * 0.5f,
        (camPos[1] + camTarget[1]) * 0.5f,
        (camPos[2] + camTarget[2]) * 0.5f
    };

    // 估算场景半径
    float dx = camPos[0] - camTarget[0];
    float dy = camPos[1] - camTarget[1];
    float dz = camPos[2] - camTarget[2];
    float sceneRadius = sqrtf(dx * dx + dy * dy + dz * dz) * 1.5f;
    if (sceneRadius < 50.0f) sceneRadius = 50.0f;

    // 光源位置：从场景中心沿光源方向偏移
    float lightPos[3] = {
        sceneCenter[0] + m_LightDir[0] * sceneRadius * 2.0f,
        sceneCenter[1] + m_LightDir[1] * sceneRadius * 2.0f,
        sceneCenter[2] + m_LightDir[2] * sceneRadius * 2.0f
    };

    // 计算光源视图矩阵
    float view[16];
    MatUtil::LookAt(view, lightPos[0], lightPos[1], lightPos[2],
                    sceneCenter[0], sceneCenter[1], sceneCenter[2],
                    0.0f, 1.0f, 0.0f);

    // 计算正交投影矩阵
    float proj[16];
    MatUtil::Ortho(proj, -sceneRadius, sceneRadius, -sceneRadius, sceneRadius,
                   0.1f, sceneRadius * 4.0f);

    // 光源空间 VP 矩阵
    MatUtil::Multiply(m_LightSpaceMatrix, proj, view);
}

void ShadowMap::BeginShadowPass(const Camera &camera)
{
    // 计算光源空间矩阵
    CalcLightSpaceMatrix(camera);

    // 保存当前视口
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // 切换到阴影 FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Resolution, m_Resolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    // 阴影 pass 只需要深度测试，禁用颜色写入和混合
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
}

void ShadowMap::EndShadowPass()
{
    // 恢复状态
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 恢复视口（由调用者处理）
}

void ShadowMap::BindShadowTexture(GLuint textureUnit) const
{
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_2D, m_DepthTexture);
}

void ShadowMap::Destroy()
{
    if (m_FBO) { glDeleteFramebuffers(1, &m_FBO); m_FBO = 0; }
    if (m_DepthTexture) { glDeleteTextures(1, &m_DepthTexture); m_DepthTexture = 0; }
}
