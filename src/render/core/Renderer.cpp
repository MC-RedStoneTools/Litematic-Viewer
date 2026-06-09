#include "Renderer.h"
#include <iostream>

Renderer::~Renderer()
{
    Destroy();
}

bool Renderer::Init(const MeshData &mesh)
{
    m_VertexCount = mesh.GetVertexCount();
    m_WireVertexCount = mesh.GetWireVertexCount();

    // 上传三角形顶点数据
    if (m_VertexCount > 0)
    {
        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(float),
                     mesh.vertices.data(), GL_STATIC_DRAW);

        // 位置 (location=0): 3 float
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, VERTEX_FLOAT_COUNT * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        // UV (location=1): 2 float
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, VERTEX_FLOAT_COUNT * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        // 颜色 (location=2): 3 float
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, VERTEX_FLOAT_COUNT * sizeof(float), (void *)(5 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

    // 上传线框顶点数据
    if (m_WireVertexCount > 0)
    {
        glGenVertexArrays(1, &m_WireVAO);
        glGenBuffers(1, &m_WireVBO);

        glBindVertexArray(m_WireVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_WireVBO);
        glBufferData(GL_ARRAY_BUFFER, mesh.wireVertices.size() * sizeof(float),
                     mesh.wireVertices.data(), GL_STATIC_DRAW);

        // 位置 (location=0): 3 float
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, WIRE_VERTEX_FLOAT_COUNT * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        // 颜色 (location=2): 3 float
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, WIRE_VERTEX_FLOAT_COUNT * sizeof(float), (void *)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

    return true;
}

void Renderer::BindDrawCallTexture(const Shader &shader, const TextureManager &texMgr,
    const MeshData &mesh, const BlockDrawCall &dc) const
{
    std::string texName = dc.textureName;
    if (texName.empty())
        texName = mesh.blockNames[dc.blockIndex];
    GLuint tex = texMgr.GetBlockTexture(texName);

    if (tex)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(glGetUniformLocation(shader.GetID(), "useTexture"), 1);
        glUniform1i(glGetUniformLocation(shader.GetID(), "texSampler"), 0);
    }
    else
    {
        glUniform1i(glGetUniformLocation(shader.GetID(), "useTexture"), 0);
    }
}

void Renderer::Draw(const Shader &shader, const TextureManager &texMgr, const MeshData &mesh) const
{
    if (m_VertexCount == 0) return;

    glBindVertexArray(m_VAO);

    // 第一遍：渲染不透明方块（深度写入开启）
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    for (auto &dc : mesh.drawCalls)
    {
        if (dc.isTransparent) continue;
        BindDrawCallTexture(shader, texMgr, mesh, dc);
        glDrawArrays(GL_TRIANGLES, dc.firstVertex, dc.vertexCount);
    }

    // 第二遍：渲染半透明方块（开启混合，深度写入关闭）
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    for (auto &dc : mesh.drawCalls)
    {
        if (!dc.isTransparent) continue;
        BindDrawCallTexture(shader, texMgr, mesh, dc);
        glDrawArrays(GL_TRIANGLES, dc.firstVertex, dc.vertexCount);
    }

    // 恢复状态
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void Renderer::DrawDebug(const Shader &shader, int mode) const
{
    if (m_VertexCount == 0) return;

    glDisable(GL_DEPTH_TEST);
    // 调试模式禁用背面剔除，确保从所有角度可见
    glDisable(GL_CULL_FACE);
    glUniform1i(glGetUniformLocation(shader.GetID(), "debugMode"), mode);
    glUniform1i(glGetUniformLocation(shader.GetID(), "useTexture"), 0);

    if (mode == 1)
    {
        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, m_VertexCount);
    }
    else if (mode == 2)
    {
        if (m_WireVertexCount > 0)
        {
            glBindVertexArray(m_WireVAO);
            glLineWidth(2.0f);
            glDrawArrays(GL_LINES, 0, m_WireVertexCount);
        }
    }

    // 恢复状态
    glUniform1i(glGetUniformLocation(shader.GetID(), "debugMode"), 0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void Renderer::Destroy()
{
    if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
    if (m_VBO) { glDeleteBuffers(1, &m_VBO); m_VBO = 0; }
    if (m_WireVAO) { glDeleteVertexArrays(1, &m_WireVAO); m_WireVAO = 0; }
    if (m_WireVBO) { glDeleteBuffers(1, &m_WireVBO); m_WireVBO = 0; }
}
