#pragma once

#include <glad/gl.h>
#include <map>
#include <string>

// 前向声明 Camera（避免在头文件中包含 Camera.h）
class Camera;

// Uniform 绑定器：将 MC 约定的 uniform 绑定到 OpenGL
class UniformBinder
{
public:
    // 绑定所有 uniform（每帧调用）
    void BindAll(GLuint program, const Camera &camera, float deltaTime);

    // 单独绑定
    void BindMatrix(GLuint program, const char *name, const float *matrix);
    void BindMatrix3(GLuint program, const char *name, const float *matrix);
    void BindVector3(GLuint program, const char *name, float x, float y, float z);
    void BindFloat(GLuint program, const char *name, float value);
    void BindInt(GLuint program, const char *name, int value);

    // 设置太阳位置
    void SetSunPosition(float x, float y, float z);

    // 设置屏幕尺寸
    void SetScreenSize(int width, int height);

    // 设置世界时间（tick）
    void SetWorldTime(int ticks);

    // 清除 uniform 位置缓存（切换着色器程序时调用）
    void ClearCache() { m_LocationCache.clear(); }

private:
    // 缓存的 uniform 位置
    std::map<std::string, GLint> m_LocationCache;

    // 太阳位置
    float m_SunX = 0.0f, m_SunY = 1.0f, m_SunZ = 0.0f;

    // 屏幕尺寸
    int m_ScreenWidth = 800, m_ScreenHeight = 600;

    // 世界时间（tick，0-24000）
    int m_WorldTime = 6000;  // 默认正午

    // 动画时间计数器
    float m_FrameTimeCounter = 0.0f;

    // 获取 uniform 位置（带缓存）
    GLint GetLocation(GLuint program, const char *name);

    // 绑定矩阵相关 uniform
    void BindMatrixUniforms(GLuint program, const Camera &camera);

    // 绑定时间相关 uniform
    void BindTimeUniforms(GLuint program, float deltaTime);

    // 绑定位置相关 uniform
    void BindPositionUniforms(GLuint program, const Camera &camera);

    // 绑定纹理采样器 uniform
    void BindSamplerUniforms(GLuint program);

    // 4x4 矩阵求逆（静态工具函数）
    static void InvertMatrix4x4(const float *m, float *out);

    // 帧计数器
    int m_FrameCounter = 0;
};
