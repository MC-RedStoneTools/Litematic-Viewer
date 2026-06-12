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
    if (m_VertexCount == 0 || m_VBO == 0)
        return;
    if (m_ShaderPackVAO)
        return; // 已创建

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

    gLogR.Info("InitShaderPackVAO: VAO=%u, stride=%d", m_ShaderPackVAO, stride);
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
        // 光影包可能用 gtexture/texture/tex/texSampler 任一名称
        GLint loc = glGetUniformLocation(program, "gtexture");
        if (loc < 0) loc = glGetUniformLocation(program, "texture");
        if (loc < 0) loc = glGetUniformLocation(program, "tex");
        if (loc < 0) loc = glGetUniformLocation(program, "texSampler");
        if (loc >= 0) glUniform1i(loc, 0);
        // 设置 useTexture = 1（使用纹理）
        GLint useTexLoc = glGetUniformLocation(program, "useTexture");
        if (useTexLoc >= 0) glUniform1i(useTexLoc, 1);
    }
    else
    {
        // 没有纹理，使用顶点颜色
        GLint useTexLoc = glGetUniformLocation(program, "useTexture");
        if (useTexLoc >= 0) glUniform1i(useTexLoc, 0);
    }
}

void Renderer::DrawWithProgram(GLuint program, const TextureManager &texMgr, const MeshData &mesh) const
{
    if (m_VertexCount == 0 || program == 0) return;

    // 根据着色器属性布局选择 VAO：
    // 转换后的光影包着色器（_ia_texCoord0 at location=8）→ ShaderPackVAO (0,2,3,8,9)
    // 原生 core profile 着色器（aUV at location=1）→ 标准 m_VAO (0,1,2,3,4)
    GLuint vao = m_VAO;
    if (m_ShaderPackVAO) {
        GLint texCoord0Loc = glGetAttribLocation(program, "_ia_texCoord0");
        if (texCoord0Loc >= 0)
            vao = m_ShaderPackVAO;
    }
    glBindVertexArray(vao);
    glUseProgram(program);

    // 验证着色器程序状态
    static bool validateOnce = false;
    if (!validateOnce) {
        validateOnce = true;
        GLint linkStatus = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        gLogR.Info("程序链接状态: %d", linkStatus);
        glValidateProgram(program);
        GLint validateStatus = 0;
        glGetProgramiv(program, GL_VALIDATE_STATUS, &validateStatus);
        char validateLog[512] = {};
        glGetProgramInfoLog(program, sizeof(validateLog), nullptr, validateLog);
        gLogR.Info("程序验证: status=%d, log='%s'", validateStatus, validateLog);

        // 检查 FBO 状态
        GLint curFBO = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &curFBO);
        if (curFBO != 0) {
            GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            gLogR.Info("当前 FBO=%d, 状态=0x%X", curFBO, fboStatus);
        }

        // 检查 GL 错误
        GLenum preErr = glGetError();
        gLogR.Info("绘制前 GL错误: 0x%X", preErr);
    }

    // 内置渲染属性位置已在 Init() 中配置：
    // location 0 = 位置 (3 floats, offset=0)
    // location 1 = UV (2 floats, offset=3)
    // location 2 = 颜色 (3 floats, offset=5)
    // location 3 = 法线 (3 floats, offset=8)
    // location 4 = 光照UV (2 floats, offset=11)
    // 不需要重新配置，直接使用 m_VAO 的配置;

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

    // 【第一步】设置内置渲染所需的 uniform（但不设置 useTexture，让 BindDrawCallTextureProgram 设置）
    // debugMode = 0（正常渲染）
    GLint debugModeLoc = glGetUniformLocation(program, "debugMode");
    if (debugModeLoc >= 0) {
        glUniform1i(debugModeLoc, 0);  // 正常渲染
    }
    // texSampler = 0（纹理槽位）
    GLint texSamplerLoc = glGetUniformLocation(program, "texSampler");
    if (texSamplerLoc >= 0) {
        glUniform1i(texSamplerLoc, 0);  // 纹理槽位 0
    }

    // 绘制所有不透明 drawCalls
    int drawCallCount = 0;
    int totalVerts = 0;
    for (auto &dc : mesh.drawCalls)
    {
        if (dc.isTransparent) continue;
        BindDrawCallTextureProgram(program, texMgr, mesh, dc);
        glDrawArrays(GL_TRIANGLES, dc.firstVertex, dc.vertexCount);
        drawCallCount++;
        totalVerts += dc.vertexCount;

        // 首次绘制后检查 GL 错误（仅第一帧打印）
        static bool firstDrawChecked = false;
        if (!firstDrawChecked && drawCallCount == 1) {
            firstDrawChecked = true;
            GLenum err1 = glGetError();
            if (err1 != GL_NO_ERROR)
                gLogR.Error("第一次 glDrawArrays 后 GL错误: 0x%X (first=%d, count=%d)",
                            err1, dc.firstVertex, dc.vertexCount);
            else
                gLogR.Info("第一次 glDrawArrays 成功 (first=%d, count=%d)", dc.firstVertex, dc.vertexCount);
        }
    }

    // 一次性诊断：检查 draw call 和矩阵
    static bool diagOnce = false;
    if (!diagOnce) {
        diagOnce = true;
        gLogR.Info("DrawWithProgram 诊断: m_VertexCount=%d, drawCalls=%zu, 非透明drawCall=%d, 非透明顶点=%d",
                   m_VertexCount, mesh.drawCalls.size(), drawCallCount, totalVerts);
        // 检查 GL 错误
        GLenum err = glGetError();
        gLogR.Info("DrawWithProgram GL错误: 0x%X", err);
        // 检查当前绑定的程序
        GLint curProg = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &curProg);
        gLogR.Info("当前程序: %d, 期望: %u", curProg, program);
        // 检查 VAO
        GLint curVAO = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &curVAO);
        gLogR.Info("当前VAO: %d, m_VAO: %u", curVAO, m_VAO);
        // 读取当前绑定的矩阵（检查 location 有效后再读取，避免 GL_INVALID_OPERATION）
        float proj[16] = {}, view[16] = {}, model[16] = {};
        GLint projLoc = glGetUniformLocation(program, "u_ProjectionMatrix");
        if (projLoc < 0) projLoc = glGetUniformLocation(program, "projection");
        if (projLoc >= 0) glGetUniformfv(program, projLoc, proj);
        GLint viewLoc = glGetUniformLocation(program, "u_ModelViewMatrix");
        if (viewLoc < 0) viewLoc = glGetUniformLocation(program, "view");
        if (viewLoc >= 0) glGetUniformfv(program, viewLoc, view);
        GLint modelLoc = glGetUniformLocation(program, "model");
        if (modelLoc >= 0) glGetUniformfv(program, modelLoc, model);
        gLogR.Info("projection[0]=%.3f,%.3f,%.3f,%.3f", proj[0], proj[1], proj[2], proj[3]);
        gLogR.Info("view[12]=%.3f,view[13]=%.3f,view[14]=%.3f", view[12], view[13], view[14]);
        // 检查第一个顶点位置
        if (mesh.vertices.size() >= 3) {
            gLogR.Info("第一个顶点: (%.3f, %.3f, %.3f)", mesh.vertices[0], mesh.vertices[1], mesh.vertices[2]);
        }
        // 手动计算第一个顶点的 clip space 位置
        float vx = mesh.vertices[0], vy = mesh.vertices[1], vz = mesh.vertices[2];
        // view * vertex
        float ex = view[0]*vx + view[4]*vy + view[8]*vz + view[12];
        float ey = view[1]*vx + view[5]*vy + view[9]*vz + view[13];
        float ez = view[2]*vx + view[6]*vy + view[10]*vz + view[14];
        float ew = view[3]*vx + view[7]*vy + view[11]*vz + view[15];
        gLogR.Info("eye space: (%.3f, %.3f, %.3f, %.3f)", ex, ey, ez, ew);
        // projection * eye
        float cx = proj[0]*ex + proj[4]*ey + proj[8]*ez + proj[12]*ew;
        float cy = proj[1]*ex + proj[5]*ey + proj[9]*ez + proj[13]*ew;
        float cz = proj[2]*ex + proj[6]*ey + proj[10]*ez + proj[14]*ew;
        float cw = proj[3]*ex + proj[7]*ey + proj[11]*ez + proj[15]*ew;
        gLogR.Info("clip space: (%.3f, %.3f, %.3f, %.3f)", cx, cy, cz, cw);
        if (cw != 0.0f)
            gLogR.Info("NDC: (%.3f, %.3f, %.3f)", cx/cw, cy/cw, cz/cw);

        // 深度缓冲扫描：检查是否有任何像素被渲染
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        int w = viewport[2], h = viewport[3];
        float minDepth = 1.0f;
        int minDepthCount = 0;
        float depthSample;
        for (int sy = 0; sy < h; sy += h/4) {
            for (int sx = 0; sx < w; sx += w/4) {
                glReadPixels(sx, sy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depthSample);
                if (depthSample < 1.0f) {
                    minDepthCount++;
                    if (depthSample < minDepth) minDepth = depthSample;
                }
            }
        }
        gLogR.Info("深度扫描: %d 个像素 < 1.0, 最小深度=%.6f (viewport=%dx%d)", minDepthCount, minDepth, w, h);
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
