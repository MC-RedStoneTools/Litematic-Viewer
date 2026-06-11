#include "UniformBinder.h"
#include "../../render/scene/Camera.h"
#include <cmath>

// 获取 uniform 位置（带缓存）
GLint UniformBinder::GetLocation(GLuint program, const char *name)
{
    std::string key = std::to_string(program) + ":" + name;
    auto it = m_LocationCache.find(key);
    if (it != m_LocationCache.end())
        return it->second;

    // 查询 OpenGL 并缓存
    GLint loc = glGetUniformLocation(program, name);
    m_LocationCache[key] = loc;
    return loc;
}

// 绑定矩阵 uniform
void UniformBinder::BindMatrix(GLuint program, const char *name, const float *matrix)
{
    GLint loc = GetLocation(program, name);
    if (loc >= 0)
        glUniformMatrix4fv(loc, 1, GL_FALSE, matrix);
}

// 绑定 vec3 uniform
void UniformBinder::BindVector3(GLuint program, const char *name, float x, float y, float z)
{
    GLint loc = GetLocation(program, name);
    if (loc >= 0)
        glUniform3f(loc, x, y, z);
}

// 绑定 float uniform
void UniformBinder::BindFloat(GLuint program, const char *name, float value)
{
    GLint loc = GetLocation(program, name);
    if (loc >= 0)
        glUniform1f(loc, value);
}

// 绑定 int uniform
void UniformBinder::BindInt(GLuint program, const char *name, int value)
{
    GLint loc = GetLocation(program, name);
    if (loc >= 0)
        glUniform1i(loc, value);
}

// 设置太阳位置
void UniformBinder::SetSunPosition(float x, float y, float z)
{
    m_SunX = x;
    m_SunY = y;
    m_SunZ = z;
}

// 设置屏幕尺寸
void UniformBinder::SetScreenSize(int width, int height)
{
    m_ScreenWidth = width;
    m_ScreenHeight = height;
}

// 设置世界时间
void UniformBinder::SetWorldTime(int ticks)
{
    m_WorldTime = ticks;
}

// 4x4 矩阵求逆（伴随矩阵法）
void UniformBinder::InvertMatrix4x4(const float *m, float *o)
{
    float a0 = m[0]*m[5] - m[1]*m[4];
    float a1 = m[0]*m[6] - m[2]*m[4];
    float a2 = m[0]*m[7] - m[3]*m[4];
    float a3 = m[1]*m[6] - m[2]*m[5];
    float a4 = m[1]*m[7] - m[3]*m[5];
    float a5 = m[2]*m[7] - m[3]*m[6];
    float b0 = m[8]*m[13] - m[9]*m[12];
    float b1 = m[8]*m[14] - m[10]*m[12];
    float b2 = m[8]*m[15] - m[11]*m[12];
    float b3 = m[9]*m[14] - m[10]*m[13];
    float b4 = m[9]*m[15] - m[11]*m[13];
    float b5 = m[10]*m[15] - m[11]*m[14];
    float det = a0*b5 - a1*b4 + a2*b3 + a3*b2 - a4*b1 + a5*b0;
    if (det == 0.0f) return; // 奇异矩阵，不求逆
    float invDet = 1.0f / det;
    o[0]  = ( m[5]*b5 - m[6]*b4 + m[7]*b3) * invDet;
    o[1]  = (-m[1]*b5 + m[2]*b4 - m[3]*b3) * invDet;
    o[2]  = ( m[13]*a5 - m[14]*a4 + m[15]*a3) * invDet;
    o[3]  = (-m[9]*a5 + m[10]*a4 - m[11]*a3) * invDet;
    o[4]  = (-m[4]*b5 + m[6]*b2 - m[7]*b1) * invDet;
    o[5]  = ( m[0]*b5 - m[2]*b2 + m[3]*b1) * invDet;
    o[6]  = (-m[12]*a5 + m[14]*a2 - m[15]*a1) * invDet;
    o[7]  = ( m[8]*a5 - m[10]*a2 + m[11]*a1) * invDet;
    o[8]  = ( m[4]*b4 - m[5]*b2 + m[7]*b0) * invDet;
    o[9]  = (-m[0]*b4 + m[1]*b2 - m[3]*b0) * invDet;
    o[10] = ( m[12]*a4 - m[13]*a2 + m[15]*a0) * invDet;
    o[11] = (-m[8]*a4 + m[9]*a2 - m[11]*a0) * invDet;
    o[12] = (-m[4]*b3 + m[5]*b1 - m[6]*b0) * invDet;
    o[13] = ( m[0]*b3 - m[1]*b1 + m[2]*b0) * invDet;
    o[14] = (-m[12]*a3 + m[13]*a1 - m[14]*a0) * invDet;
    o[15] = ( m[8]*a3 - m[9]*a1 + m[10]*a0) * invDet;
}

// 绑定矩阵相关 uniform
void UniformBinder::BindMatrixUniforms(GLuint program, const Camera &camera)
{
    // 摄像机视图矩阵
    const float *view = camera.GetViewMatrix();
    BindMatrix(program, "gbufferModelView", view);
    // 计算并绑定逆矩阵
    float viewInv[16];
    InvertMatrix4x4(view, viewInv);
    BindMatrix(program, "gbufferModelViewInverse", viewInv);

    // 投影矩阵
    const float *proj = camera.GetProjectionMatrix();
    BindMatrix(program, "gbufferProjection", proj);
    // 计算并绑定逆矩阵
    float projInv[16];
    InvertMatrix4x4(proj, projInv);
    BindMatrix(program, "gbufferProjectionInverse", projInv);

    // compatibility 模式内置矩阵：gl_ModelViewMatrix 和 gl_ProjectionMatrix
    // 这些是着色器中使用的内置矩阵，必须手动设置
    // 注意：由于 glGetUniformLocation 无法获取内置 uniform，我们在着色器源码中
    // 将它们替换为 u_ModelViewMatrix 和 u_ProjectionMatrix
    BindMatrix(program, "u_ModelViewMatrix", view);
    BindMatrix(program, "u_ProjectionMatrix", proj);

    // 诊断：检查 uniform 位置（只输出一次）
    static bool loggedOnce = false;
    if (!loggedOnce) {
        GLint loc;
        loc = glGetUniformLocation(program, "gbufferModelView");
        fprintf(stderr, "[DEBUG] Uniform gbufferModelView: %d\n", loc);
        loc = glGetUniformLocation(program, "gbufferModelViewInverse");
        fprintf(stderr, "[DEBUG] Uniform gbufferModelViewInverse: %d\n", loc);
        loc = glGetUniformLocation(program, "u_ModelViewMatrix");
        fprintf(stderr, "[DEBUG] Uniform u_ModelViewMatrix: %d\n", loc);
        loc = glGetUniformLocation(program, "u_ProjectionMatrix");
        fprintf(stderr, "[DEBUG] Uniform u_ProjectionMatrix: %d\n", loc);
        loc = glGetUniformLocation(program, "gbufferProjection");
        fprintf(stderr, "[DEBUG] Uniform gbufferProjection: %d\n", loc);
        fflush(stderr);
        loggedOnce = true;
    }

    // gl_NormalMatrix = transpose(inverse(gl_ModelViewMatrix))
    // 用于正确变换法线向量
    float normalMat[16];
    InvertMatrix4x4(view, normalMat);
    // 转置（对于正交矩阵，逆矩阵等于转置，但这里我们还是显式转置）
    float normalMatTransposed[16] = {
        normalMat[0], normalMat[4], normalMat[8], normalMat[12],
        normalMat[1], normalMat[5], normalMat[9], normalMat[13],
        normalMat[2], normalMat[6], normalMat[10], normalMat[14],
        normalMat[3], normalMat[7], normalMat[11], normalMat[15]
    };
    BindMatrix(program, "u_NormalMatrix", normalMatTransposed);

    // gl_TextureMatrix[0] 和 gl_TextureMatrix[1]（compatibility 模式纹理坐标变换）
    // gl_TextureMatrix[0] = 单位矩阵（普通纹理坐标）
    float texMat0[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    BindMatrix(program, "gl_TextureMatrix[0]", texMat0);
    // gl_TextureMatrix[1] = 单位矩阵（光照图坐标，Minecraft 使用 0-15 范围）
    float texMat1[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    BindMatrix(program, "gl_TextureMatrix[1]", texMat1);

    // 阴影矩阵（基于 sunAngle 计算，从太阳方向俯视场景）
    // sunAngle: 0.25=正午(太阳在正上方), 0.0/0.5=日出/日落
    float sunAngleRad = 2.0f * 3.14159265f * ((m_WorldTime + 6000) % 24000) / 24000.0f;
    float sunDirX = sinf(sunAngleRad);
    float sunDirY = cosf(sunAngleRad);
    // 太阳方向归一化（Z 分量为 0，太阳在 XY 平面移动）
    float sunLen = sqrtf(sunDirX * sunDirX + sunDirY * sunDirY);
    if (sunLen < 0.001f) sunLen = 1.0f;
    float sdx = sunDirX / sunLen;
    float sdy = sunDirY / sunLen;
    // shadowModelView：从太阳方向看向原点（简化 lookAt）
    // 右方向 = cross(sunDir, up)，其中 up=(0,0,1)
    float rightX = -sdy, rightY = sdx, rightZ = 0.0f; // cross((sdx,sdy,0), (0,0,1))
    // 前方向 = cross(right, sunDir)
    float fwdX = 0.0f, fwdY = 0.0f, fwdZ = 1.0f;
    float shadowMV[16] = {
        rightX,  sdx, 0, 0,   // 列主序：列0
        rightY,  sdy, 0, 0,   // 列1
        rightZ,    0, 1, 0,   // 列2
        0,         0, 0, 1    // 列3
    };
    BindMatrix(program, "shadowModelView", shadowMV);
    float shadowMVI[16];
    InvertMatrix4x4(shadowMV, shadowMVI);
    BindMatrix(program, "shadowModelViewInverse", shadowMVI);

    // 阴影正交投影（覆盖场景范围）
    float shadowDist = 128.0f;
    float shadowProj[16] = {
        2.0f/shadowDist, 0, 0, 0,
        0, 2.0f/shadowDist, 0, 0,
        0, 0, -2.0f/256.0f, 0,
        0, 0, -1, 1
    };
    BindMatrix(program, "shadowProjection", shadowProj);
    float shadowProjI[16];
    InvertMatrix4x4(shadowProj, shadowProjI);
    BindMatrix(program, "shadowProjectionInverse", shadowProjI);
}

// 绑定时间相关 uniform
void UniformBinder::BindTimeUniforms(GLuint program, float deltaTime)
{
    // 帧时间计数器（循环累加）
    m_FrameTimeCounter += deltaTime;
    if (m_FrameTimeCounter > 3600.0f)
        m_FrameTimeCounter -= 3600.0f;

    BindFloat(program, "frameTimeCounter", m_FrameTimeCounter);
    BindFloat(program, "frameTime", deltaTime);

    // 帧计数器（递增）
    m_FrameCounter++;
    BindInt(program, "frameCounter", m_FrameCounter);

    // 世界时间（tick）
    BindInt(program, "worldTime", m_WorldTime);
    // 世界天数
    BindInt(program, "worldDay", m_WorldTime / 24000);

    // 太阳角度（0.0-1.0）
    // Minecraft: worldTime=6000 是正午，sunAngle=0.25 对应正午
    float sunAngle = static_cast<float>((m_WorldTime + 6000) % 24000) / 24000.0f;
    BindFloat(program, "sunAngle", sunAngle);

    // 雨量强度（暂无天气系统，固定为0）
    BindFloat(program, "rainStrength", 0.0f);
    BindFloat(program, "wetness", 0.0f);

    // 屏幕亮度（默认最大）
    BindFloat(program, "screenBrightness", 1.0f);

    // 玩家状态
    BindInt(program, "isEyeInWater", 0);
    BindFloat(program, "blindness", 0.0f);
    BindFloat(program, "nightVision", 0.0f);
    BindFloat(program, "playerMood", 0.0f);
}

// 绑定位置相关 uniform
void UniformBinder::BindPositionUniforms(GLuint program, const Camera &camera)
{
    // 摄像机位置（从视图矩阵提取）
    const float *view = camera.GetViewMatrix();
    // 视图矩阵的平移分量取反得到世界坐标
    float camX = -(view[0] * view[12] + view[1] * view[13] + view[2] * view[14]);
    float camY = -(view[4] * view[12] + view[5] * view[13] + view[6] * view[14]);
    float camZ = -(view[8] * view[12] + view[9] * view[13] + view[10] * view[14]);
    BindVector3(program, "cameraPosition", camX, camY, camZ);

    // 太阳位置
    BindVector3(program, "sunPosition", m_SunX, m_SunY, m_SunZ);

    // 月亮位置（太阳对面）
    BindVector3(program, "moonPosition", -m_SunX, -m_SunY, -m_SunZ);

    // 光源位置（与太阳相同）
    BindVector3(program, "shadowLightPosition", m_SunX, m_SunY, m_SunZ);

    // 上方向
    BindVector3(program, "upPosition", 0.0f, 1.0f, 0.0f);

    // 近远裁剪面
    BindFloat(program, "near", 0.05f);
    BindFloat(program, "far", 1000.0f);

    // 屏幕尺寸（Iris 约定 ivec2，但我们用 vec4 也能工作）
    BindVector3(program, "screenSize",
                static_cast<float>(m_ScreenWidth),
                static_cast<float>(m_ScreenHeight), 0.0f);

    // 眼部亮度（方块光, 天空光，范围 0-240）
    BindInt(program, "eyeBrightness", 240 << 8 | 240);  // 全亮
    BindInt(program, "eyeBrightnessSmooth", 240 << 8 | 240);

    // 月亮相位（0-7）
    BindInt(program, "moonPhase", 0);
}

// 绑定纹理采样器 uniform
void UniformBinder::BindSamplerUniforms(GLuint program)
{
    // 光影包约定的纹理槽位（Iris 标准：0=方块纹理, 1=叠加纹理, 2=光照图）
    BindInt(program, "texture", 0);         // 方块纹理
    BindInt(program, "tex", 0);             // 方块纹理（Iris 别名）
    BindInt(program, "gtexture", 0);        // 方块纹理（Iris 别名）
    BindInt(program, "lightmap", 2);        // 光照图（Iris 约定 slot 2）
    BindInt(program, "shadowtex0", 3);      // 阴影深度图
    BindInt(program, "shadowtex1", 4);      // 阴影深度图（透明）
    BindInt(program, "noisetex", 5);        // 噪声纹理
}

// 绑定所有 uniform（主入口）
void UniformBinder::BindAll(GLuint program, const Camera &camera, float deltaTime)
{
    BindMatrixUniforms(program, camera);
    BindTimeUniforms(program, deltaTime);
    BindPositionUniforms(program, camera);
    BindSamplerUniforms(program);

    // 屏幕尺寸
    float w = static_cast<float>(m_ScreenWidth);
    float h = static_cast<float>(m_ScreenHeight);
    GLint loc = GetLocation(program, "screenSize");
    if (loc >= 0)
        glUniform4f(loc, w, h, 1.0f / w, 1.0f / h);

    // 注：near/far 已在 BindPositionUniforms 中设置（near=0.05, far=1000）

    // texelSize（像素大小）
    float tw = 1.0f / w;
    float th = 1.0f / h;
    loc = GetLocation(program, "texelSize");
    if (loc >= 0) glUniform2f(loc, tw, th);

    // 实体相关
    BindInt(program, "entityId", 0);
    float entityColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    loc = GetLocation(program, "entityColor");
    if (loc >= 0) glUniform4fv(loc, 1, entityColor);
}
