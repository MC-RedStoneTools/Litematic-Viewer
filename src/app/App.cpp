#include "App.h"

#include <glad/gl.h>
#include <algorithm>
#include <sstream>

#include "../utils/Log.h"
#include "../shader_pack/runtime/ShaderPackRuntime.h"

static LogSource gLog("App");

// 阴影顶点着色器：输出光源空间位置用于阴影计算
static const char *shadowVertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
)";

// 阴影片段着色器（只需要深度，不需要颜色输出）
static const char *shadowFragmentShaderSrc = R"(
#version 330 core

void main()
{
    // 深度会自动写入
}
)";

// 带点光源的顶点着色器
static const char *shadowMainVertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aUV;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

out vec2 texCoord;
out vec3 vertexColor;
out vec3 fragNormal;
out vec3 fragWorldPos;
out vec4 fragPosLightSpace;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    gl_Position = projection * view * worldPos;
    texCoord = aUV;
    vertexColor = aColor;
    fragNormal = mat3(model) * aNormal;
    fragWorldPos = worldPos.xyz;
    fragPosLightSpace = lightSpaceMatrix * worldPos;
}
)";

// 带点光源的片段着色器（暂不含阴影，先验证光照）
static const char *shadowMainFragmentShaderSrc = R"(
#version 330 core
in vec2 texCoord;
in vec3 vertexColor;
in vec3 fragNormal;
in vec3 fragWorldPos;
in vec4 fragPosLightSpace;

uniform sampler2D texSampler;
uniform int useTexture;
uniform int debugMode;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform float lightIntensity;

out vec4 FragColor;

void main()
{
    if (debugMode == 1) { FragColor = vec4(vertexColor, 1.0); return; }
    if (debugMode == 2) { FragColor = vec4(0.2, 1.0, 0.2, 0.3); return; }
    if (debugMode == 3) { FragColor = vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0); return; }
    if (debugMode == 6) { FragColor = vec4(fragWorldPos / 10.0, 1.0); return; }

    vec4 baseColor;
    if (useTexture == 1)
    {
        vec4 texColor = texture(texSampler, texCoord);
        if (texColor.a < 0.1) discard;
        baseColor = texColor;
    }
    else
        baseColor = vec4(vertexColor, 1.0);

    // 点光源
    vec3 lightVec = lightPos - fragWorldPos;
    float dist = length(lightVec);
    vec3 L = lightVec / dist;
    vec3 N = normalize(fragNormal);
    float NdotL = max(dot(N, L), 0.0);

    float attenuation = lightIntensity / (1.0 + 0.09 * dist + 0.032 * dist * dist);

    float ambient = 0.2;
    vec3 diffuse = NdotL * lightColor * attenuation;
    vec3 lighting = vec3(ambient) + diffuse;

    FragColor = vec4(baseColor.rgb * lighting, baseColor.a);
}
)";

// 原始顶点着色器（无阴影）
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

// 原始片段着色器（无阴影）
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

bool App::Init(std::unique_ptr<IPlatform> platform, PipelineContext &ctx, int debugMode)
{
    m_Platform = std::move(platform);
    m_Ctx = &ctx;
    m_DebugMode = debugMode;

    // 平台初始化
    std::string title = MakeWindowTitle(ctx.modelSize);
    if (!m_Platform->Init(800, 600, title))
        return false;

    // 着色器编译
    if (!m_Shader.Init(vertexShaderSrc, fragmentShaderSrc))
    {
        gLog.Error("着色器初始化失败");
        return false;
    }

    // 阴影着色器编译
    if (!m_ShadowShader.Init(shadowVertexShaderSrc, shadowFragmentShaderSrc))
    {
        gLog.Warn("阴影着色器初始化失败，阴影功能将不可用");
        m_EnableShadow = false;
    }

    // 带阴影的主着色器编译
    if (!m_ShadowMainShader.Init(shadowMainVertexShaderSrc, shadowMainFragmentShaderSrc))
    {
        gLog.Warn("阴影主着色器初始化失败，阴影功能将不可用");
        m_EnableShadow = false;
    }

    // 阴影贴图初始化
    if (m_EnableShadow)
    {
        int shadowRes = 1024; // 默认阴影分辨率
        if (ctx.appConfig.shaderPack.shadowResolution > 0)
            shadowRes = ctx.appConfig.shaderPack.shadowResolution;

        if (!m_ShadowMap.Init(shadowRes))
        {
            gLog.Warn("阴影贴图初始化失败，阴影功能将不可用");
            m_EnableShadow = false;
        }
    }

    // 光影包 GPU 初始化（必须在 OpenGL 上下文创建之后）
    if (ctx.shaderPackRuntime && ctx.shaderPackRuntime->IsLoaded())
    {
        int fbWidth = 0, fbHeight = 0;
        m_Platform->GetFramebufferSize(fbWidth, fbHeight);

        ctx.shaderPackRuntime->ApplyAppConfig(
            ctx.appConfig.shaderPack.shadowResolution,
            ctx.appConfig.shaderPack.shadowDistance);

        if (!ctx.shaderPackRuntime->InitGPU(fbWidth, fbHeight))
        {
            gLog.Warn("光影包 GPU 初始化失败，回退到默认渲染");
            ctx.shaderPackRuntime = nullptr;
        }
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
    if (m_Ctx && m_Ctx->shaderPackRuntime)
        m_Ctx->shaderPackRuntime->Destroy();

    m_ShadowMap.Destroy();
    m_Renderer.Destroy();
    m_TexMgr.Destroy();

    if (m_Platform)
        m_Platform->Shutdown();
}
