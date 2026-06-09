#pragma once

#include <string>
#include <glad/gl.h>
#include "GLResource.h"

// 着色器程序管理类（RAII自动管理）
class Shader
{
public:
    Shader() = default;

    // 从源码字符串编译并链接着色器程序
    bool Init(const std::string &vertexSrc, const std::string &fragmentSrc);

    // 激活着色器程序
    void Use() const;

    // 获取程序ID（用于设置uniform）
    GLuint GetID() const { return m_Program; }

    // 设置uniform矩阵4x4
    void SetMat4(const char *name, const float *value) const;

private:
    GLResource::Program m_Program;

    // 编译单个着色器
    GLuint CompileShader(GLenum type, const std::string &source);
};
