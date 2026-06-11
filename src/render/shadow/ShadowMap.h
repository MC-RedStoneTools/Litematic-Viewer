#pragma once

#include <glad/gl.h>
#include "../scene/Camera.h"

// 最简单的阴影映射实现
// 1. 从光源视角渲染深度图
// 2. 主渲染时采样深度图判断是否在阴影中
class ShadowMap
{
public:
    ShadowMap() = default;
    ~ShadowMap();

    // 初始化阴影贴图 FBO
    bool Init(int resolution = 1024);

    // 开始从光源视角渲染阴影
    void BeginShadowPass(const Camera &camera);

    // 结束阴影渲染
    void EndShadowPass();

    // 绑定阴影贴图到纹理单元
    void BindShadowTexture(GLuint textureUnit = 1) const;

    // 获取光源空间矩阵
    const float *GetLightSpaceMatrix() const { return m_LightSpaceMatrix; }

    // 设置光源方向
    void SetLightDirection(float x, float y, float z);

    // 获取阴影分辨率
    int GetResolution() const { return m_Resolution; }

    // 销毁资源
    void Destroy();

private:
    GLuint m_FBO = 0;
    GLuint m_DepthTexture = 0;
    int m_Resolution = 1024;

    float m_LightDir[3] = { 0.5f, 1.0f, 0.3f }; // 默认光源方向
    float m_LightSpaceMatrix[16]; // 光源空间的 VP 矩阵

    // 计算光源空间矩阵
    void CalcLightSpaceMatrix(const Camera &camera);
};
