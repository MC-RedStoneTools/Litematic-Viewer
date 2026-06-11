#pragma once

#include <glad/gl.h>
#include "Shader.h"
#include "../resource/Texture.h"
#include "../../data/MeshData.h"

// 渲染器：接收MeshData，上传到GPU并绘制
// 负责所有OpenGL资源管理和渲染调用
// 注意：使用延迟初始化，OpenGL资源在Init()中创建
class Renderer
{
public:
    Renderer() = default;
    ~Renderer();

    // 从MeshData上传到GPU（必须在OpenGL上下文就绪后调用）
    bool Init(const MeshData &mesh);

    // 正常渲染：按方块类型分组，绑定纹理绘制
    void Draw(const Shader &shader, const TextureManager &texMgr, const MeshData &mesh) const;

    // 光影包渲染：使用外部着色器程序绘制已上传的网格
    void DrawWithProgram(GLuint program, const TextureManager &texMgr, const MeshData &mesh) const;

    // 创建光影包专用 VAO（需在 Init 之后调用，共享同一 VBO）
    void InitShaderPackVAO();

    // 调试绘制：mode=1面方向着色, mode=2线框
    void DrawDebug(const Shader &shader, int mode) const;

    // 释放GPU资源
    void Destroy();

private:
    // 禁止复制
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    // 绑定单个DrawCall的纹理（Shader 版本）
    void BindDrawCallTexture(const Shader &shader, const TextureManager &texMgr,
        const MeshData &mesh, const BlockDrawCall &dc) const;

    // 绑定单个DrawCall的纹理（原始 program 版本）
    void BindDrawCallTextureProgram(GLuint program, const TextureManager &texMgr,
        const MeshData &mesh, const BlockDrawCall &dc) const;

    // 三角形VAO/VBO（延迟初始化）
    GLuint m_VAO = 0;
    GLuint m_VBO = 0;
    int m_VertexCount = 0;

    // 光影包专用 VAO（兼容 compatibility 模式内置属性位置）
    GLuint m_ShaderPackVAO = 0;

    // 线框VAO/VBO（延迟初始化）
    GLuint m_WireVAO = 0;
    GLuint m_WireVBO = 0;
    int m_WireVertexCount = 0;
};
