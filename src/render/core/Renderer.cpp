#include "Renderer.h"
#include "../../utils/Log.h"
#include <iostream>

static LogSource gLogR("Renderer");

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
        // 法线 (location=3): 3 float
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, VERTEX_FLOAT_COUNT * sizeof(float), (void *)(8 * sizeof(float)));
        glEnableVertexAttribArray(3);
        // 光照UV (location=4): 2 float
        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, VERTEX_FLOAT_COUNT * sizeof(float), (void *)(11 * sizeof(float)));
        glEnableVertexAttribArray(4);

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

// 创建光影包专用 VAO，属性位置匹配 GLSL 330 compatibility 模式内置变量
// gl_Vertex=0, gl_Normal=2, gl_Color=3, gl_MultiTexCoord0=8, gl_MultiTexCoord1=9
void Renderer::InitShaderPackVAO()
{
    // 使用 fprintf 直接输出确保日志可见
    fprintf(stderr, "[DEBUG] InitShaderPackVAO 被调用: 顶点数=%d, VBO=%u, 已有VAO=%u\n", m_VertexCount, m_VBO, m_ShaderPackVAO);
    fflush(stderr);

    gLogR.Info("InitShaderPackVAO 被调用: 顶点数=%d, VBO=%u, 已有VAO=%u", m_VertexCount, m_VBO, m_ShaderPackVAO);

    if (m_VertexCount == 0 || m_VBO == 0) {
        fprintf(stderr, "[DEBUG] InitShaderPackVAO: 跳过，顶点数=%d, VBO=%u\n", m_VertexCount, m_VBO);
        fflush(stderr);
        gLogR.Warn("InitShaderPackVAO: 跳过，顶点数=%d, VBO=%u", m_VertexCount, m_VBO);
        return;
    }
    if (m_ShaderPackVAO) {
        fprintf(stderr, "[DEBUG] InitShaderPackVAO: VAO 已存在=%u\n", m_ShaderPackVAO);
        fflush(stderr);
        gLogR.Info("InitShaderPackVAO: VAO 已存在=%u", m_ShaderPackVAO);
        return; // 已创建
    }

    glGenVertexArrays(1, &m_ShaderPackVAO);
    glBindVertexArray(m_ShaderPackVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO); // 共享同一 VBO

    GLsizei stride = VERTEX_FLOAT_COUNT * sizeof(float);

    // attribute 0 = gl_Vertex（位置）
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);
    glEnableVertexAttribArray(0);
    // attribute 2 = gl_Normal（法线，offset=8*4=32）
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void *)(8 * sizeof(float)));
    glEnableVertexAttribArray(2);
    // attribute 3 = gl_Color（颜色，offset=5*4=20）
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void *)(5 * sizeof(float)));
    glEnableVertexAttribArray(3);
    // attribute 8 = gl_MultiTexCoord0（UV，offset=3*4=12）
    glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(8);
    // attribute 9 = gl_MultiTexCoord1（光照UV，offset=11*4=44）
    glVertexAttribPointer(9, 2, GL_FLOAT, GL_FALSE, stride, (void *)(11 * sizeof(float)));
    glEnableVertexAttribArray(9);

    // 注意：mc_Entity 和 mc_midTexCoord 属性位置需要根据着色器动态查询
    // 在 DrawWithProgram 中设置，因为属性位置取决于着色器程序

    glBindVertexArray(0);

    gLogR.Info("InitShaderPackVAO: 创建成功，VAO=%u, VBO=%u, stride=%d", m_ShaderPackVAO, m_VBO, stride);
    fprintf(stderr, "[DEBUG] InitShaderPackVAO: 创建成功 VAO=%u\n", m_ShaderPackVAO);
    fflush(stderr);
}

void Renderer::BindDrawCallTexture(const Shader &shader, const TextureManager &texMgr,
    const MeshData &mesh, const BlockDrawCall &dc) const
{
    std::string texName = dc.textureName;
    if (texName.empty())
        texName = dc.blockName;
    if (texName.empty() && dc.blockIndex >= 0 && dc.blockIndex < static_cast<int>(mesh.blockNames.size()))
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

void Renderer::BindDrawCallTextureProgram(GLuint program, const TextureManager &texMgr,
    const MeshData &mesh, const BlockDrawCall &dc) const
{
    std::string texName = dc.textureName;
    if (texName.empty())
        texName = dc.blockName;
    if (texName.empty() && dc.blockIndex >= 0 && dc.blockIndex < static_cast<int>(mesh.blockNames.size()))
        texName = mesh.blockNames[dc.blockIndex];
    GLuint tex = texMgr.GetBlockTexture(texName);

    if (tex)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        // 光影包可能用 texture/tex/texSampler 任一名称
        GLint loc = glGetUniformLocation(program, "texture");
        if (loc < 0) loc = glGetUniformLocation(program, "tex");
        if (loc < 0) loc = glGetUniformLocation(program, "texSampler");
        if (loc >= 0) glUniform1i(loc, 0);
    }
}

void Renderer::DrawWithProgram(GLuint program, const TextureManager &texMgr, const MeshData &mesh) const
{
    if (m_VertexCount == 0 || program == 0) {
        std::cerr << "[WARN] DrawWithProgram: 跳过，顶点数=" << m_VertexCount << ", program=" << program << std::endl;
        return;
    }

    // 诊断：检查 VAO 状态
    GLuint vaoToUse = m_ShaderPackVAO ? m_ShaderPackVAO : m_VAO;
    if (vaoToUse == 0) {
        std::cerr << "[ERROR] DrawWithProgram: VAO=0, m_ShaderPackVAO=" << m_ShaderPackVAO << ", m_VAO=" << m_VAO << std::endl;
        return;
    }

    // 清除之前的 GL 错误
    while (glGetError() != GL_NO_ERROR);

    // 诊断：检查 glUseProgram 前后的错误
    glUseProgram(program);
    GLenum err1 = glGetError();
    if (err1 != GL_NO_ERROR) {
        fprintf(stderr, "[ERROR] DrawWithProgram: glUseProgram(%u) 后 GL 错误: 0x%X\n", program, err1);
        fflush(stderr);
    }

    // 诊断：输出 VAO 信息
    fprintf(stderr, "[DEBUG] DrawWithProgram: 使用 VAO=%u (m_ShaderPackVAO=%u, m_VAO=%u)\n",
            vaoToUse, m_ShaderPackVAO, m_VAO);
    fflush(stderr);

    // 使用光影包专用 VAO（属性位置匹配 compatibility 模式内置变量）
    glBindVertexArray(vaoToUse);
    GLenum err2 = glGetError();
    if (err2 != GL_NO_ERROR) {
        fprintf(stderr, "[ERROR] DrawWithProgram: glBindVertexArray(%u) 后 GL 错误: 0x%X\n", vaoToUse, err2);
        fflush(stderr);
    }

    // Compatibility 模式固定属性位置：
    // gl_Vertex = 0, gl_Normal = 2, gl_Color = 3
    // gl_MultiTexCoord0 = 8, gl_MultiTexCoord1 = 9
    // 直接使用固定位置配置 VAO
    GLsizei stride = VERTEX_FLOAT_COUNT * sizeof(float);

    // 属性 0 = gl_Vertex（位置）
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void *)0);

    // 属性 2 = gl_Normal（法线，offset=8*4=32）
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void *)(8 * sizeof(float)));

    // 属性 3 = gl_Color（颜色，offset=5*4=20）
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void *)(5 * sizeof(float)));

    // 属性 8 = gl_MultiTexCoord0（UV，offset=3*4=12）
    glEnableVertexAttribArray(8);
    glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, stride, (void *)(3 * sizeof(float)));

    // 属性 9 = gl_MultiTexCoord1（光照UV，offset=11*4=44）
    glEnableVertexAttribArray(9);
    glVertexAttribPointer(9, 2, GL_FLOAT, GL_FALSE, stride, (void *)(11 * sizeof(float)));

    // 诊断：查询着色器属性位置（输出前50次）
    // 强制输出，确保可见
    static int logCount = 0;
    logCount++;
    if (logCount <= 50) {
        fprintf(stderr, "[DEBUG-ATTR] 第%d次调用, program=%u\n", logCount, program);
        GLint loc;
        loc = glGetAttribLocation(program, "gl_Vertex");
        fprintf(stderr, "  gl_Vertex: %d\n", loc);
        loc = glGetAttribLocation(program, "gl_Normal");
        fprintf(stderr, "  gl_Normal: %d\n", loc);
        loc = glGetAttribLocation(program, "gl_Color");
        fprintf(stderr, "  gl_Color: %d\n", loc);
        loc = glGetAttribLocation(program, "gl_MultiTexCoord0");
        fprintf(stderr, "  gl_MultiTexCoord0: %d\n", loc);
        loc = glGetAttribLocation(program, "gl_MultiTexCoord1");
        fprintf(stderr, "  gl_MultiTexCoord1: %d\n", loc);
        loc = glGetAttribLocation(program, "mc_Entity");
        fprintf(stderr, "  mc_Entity: %d\n", loc);
        loc = glGetAttribLocation(program, "mc_midTexCoord");
        fprintf(stderr, "  mc_midTexCoord: %d\n", loc);
        fflush(stderr);
    }

    // 设置 Iris 特有属性的默认值
    // mc_Entity = vec4(方块ID, 是否为植物, 0, 1)
    GLint mcEntityLoc = glGetAttribLocation(program, "mc_Entity");
    if (mcEntityLoc >= 0) {
        // 使用 glVertexAttrib4f 设置常量值，禁用顶点数组
        glDisableVertexAttribArray(mcEntityLoc);
        glVertexAttrib4f(mcEntityLoc, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    // mc_midTexCoord = vec4(UV中点, 0, 1)
    GLint mcMidTexCoordLoc = glGetAttribLocation(program, "mc_midTexCoord");
    if (mcMidTexCoordLoc >= 0) {
        glDisableVertexAttribArray(mcMidTexCoordLoc);
        glVertexAttrib4f(mcMidTexCoordLoc, 0.5f, 0.5f, 0.0f, 1.0f);
    }
    // at_tangent = vec4(切线向量, 0)
    GLint atTangentLoc = glGetAttribLocation(program, "at_tangent");
    if (atTangentLoc >= 0) {
        glDisableVertexAttribArray(atTangentLoc);
        glVertexAttrib4f(atTangentLoc, 1.0f, 0.0f, 0.0f, 1.0f);
    }
    // at_midBlock = vec4(方块中心偏移, 0, 1)
    GLint atMidBlockLoc = glGetAttribLocation(program, "at_midBlock");
    if (atMidBlockLoc >= 0) {
        glDisableVertexAttribArray(atMidBlockLoc);
        glVertexAttrib4f(atMidBlockLoc, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    // 启用深度测试，暂时禁用背面剔除用于诊断
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);  // 暂时禁用背面剔除
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    int drawCallCount = 0;
    for (auto &dc : mesh.drawCalls)
    {
        if (dc.isTransparent) continue;
        BindDrawCallTextureProgram(program, texMgr, mesh, dc);
        glDrawArrays(GL_TRIANGLES, dc.firstVertex, dc.vertexCount);
        drawCallCount++;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    for (auto &dc : mesh.drawCalls)
    {
        if (!dc.isTransparent) continue;
        BindDrawCallTextureProgram(program, texMgr, mesh, dc);
        glDrawArrays(GL_TRIANGLES, dc.firstVertex, dc.vertexCount);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
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
    glDisable(GL_CULL_FACE);
    glUniform1i(glGetUniformLocation(shader.GetID(), "debugMode"), mode);
    glUniform1i(glGetUniformLocation(shader.GetID(), "useTexture"), 0);

    if (mode == 2)
    {
        if (m_WireVertexCount > 0)
        {
            glBindVertexArray(m_WireVAO);
            glLineWidth(2.0f);
            glDrawArrays(GL_LINES, 0, m_WireVertexCount);
        }
    }
    else
    {
        // 所有非线框模式都绘制三角形网格（mode 1,3,4,5,6...）
        glBindVertexArray(m_VAO);
        glDrawArrays(GL_TRIANGLES, 0, m_VertexCount);
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
    if (m_ShaderPackVAO) { glDeleteVertexArrays(1, &m_ShaderPackVAO); m_ShaderPackVAO = 0; }
    if (m_WireVAO) { glDeleteVertexArrays(1, &m_WireVAO); m_WireVAO = 0; }
    if (m_WireVBO) { glDeleteBuffers(1, &m_WireVBO); m_WireVBO = 0; }
}
