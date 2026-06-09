#include "App.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <sstream>

#include "../utils/Log.h"

static LogSource gLog("App");

static const char *vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec3 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 texCoord;
out vec3 vertexColor;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    texCoord = aUV;
    vertexColor = aColor;
}
)";

static const char *fragmentShaderSrc = R"(
#version 330 core
in vec2 texCoord;
in vec3 vertexColor;

uniform sampler2D texSampler;
uniform int useTexture;
uniform int debugMode;

out vec4 FragColor;

void main()
{
    if (debugMode == 1)
        FragColor = vec4(vertexColor, 1.0);
    else if (debugMode == 2)
        FragColor = vec4(0.2, 1.0, 0.2, 0.3);
    else if (useTexture == 1)
    {
        vec4 texColor = texture(texSampler, texCoord);
        // 完全透明的像素丢弃（不写入深度缓冲）
        if (texColor.a < 0.1) discard;
        FragColor = texColor;
    }
    else
        FragColor = vec4(vertexColor, 1.0);
}
)";

std::string App::MakeWindowTitle(const ModelSize &size)
{
    std::ostringstream oss;
    oss << "Litematica Preview - " << size.sizeX << "x" << size.sizeY << "x" << size.sizeZ;
    if (size.totalBlocks > 0)
        oss << " (" << size.totalBlocks << " blocks)";
    return oss.str();
}

void App::CalcRegionsCenter(const PipelineContext &ctx, float &cx, float &cy, float &cz)
{
    if (ctx.regions.empty()) { cx = cy = cz = 0; return; }

    int minX, minY, minZ, maxX, maxY, maxZ;
    CalcRegionsBoundingBox(ctx.regions, minX, minY, minZ, maxX, maxY, maxZ);

    cx = (minX + maxX) / 2.0f;
    cy = (minY + maxY) / 2.0f;
    cz = (minZ + maxZ) / 2.0f;
}

bool App::Init(const PipelineContext &ctx, int debugMode)
{
    m_Ctx = &ctx;
    m_DebugMode = debugMode;

    // 窗口初始化
    std::string title = MakeWindowTitle(ctx.modelSize);
    if (!m_Window.Init(800, 600, title))
        return false;

    // 着色器编译
    if (!m_Shader.Init(vertexShaderSrc, fragmentShaderSrc))
    {
        gLog.Error("着色器初始化失败");
        return false;
    }

    // 纹理加载
    if (!ctx.texturesDir.empty())
    {
        if (!m_TexMgr.LoadTexturesFromDir(ctx.texturesDir))
            gLog.Error("纹理加载失败，将使用纯色渲染");
    }
    else
    {
        gLog.Warn("未配置纹理目录，跳过纹理加载");
    }

    // 网格上传和相机设置
    if (!ctx.mesh.IsEmpty())
    {
        m_Renderer.Init(ctx.mesh);

        float cx, cy, cz;
        CalcRegionsCenter(ctx, cx, cy, cz);
        m_Camera.SetTarget(cx, cy, cz);
        m_Camera.OnScroll(0);
    }

    m_Camera.UpdateProjectionMatrix(800, 600);
    m_Camera.UpdateViewMatrix();
    m_Camera.PrintDebugInfo();

    // 设置OpenGL状态
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    return true;
}

void App::Shutdown()
{
    m_Renderer.Destroy();
    m_TexMgr.Destroy();
    m_Window.Shutdown();
}
