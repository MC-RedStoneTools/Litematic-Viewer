#include <string>
#include <memory>

#include "pipeline/Pipeline.h"
#include "stage/LoadStage.h"
#include "stage/MeshStage.h"
#include "stage/ShaderPackStage.h"
#include "app/App.h"
#include "render/platform/RenderLoop.h"
#include "render/platform/GlfwPlatform.h"
#include "utils/PathUtils.h"

int main(int argc, char *argv[])
{
    // === 解析命令行参数 ===
    PipelineContext ctx;
    int debugMode = 0;

    for (int i = 1; i < argc; i++)
    {
        std::string arg(argv[i]);
        if (arg == "--test" || arg == "-t")
            ctx.useHardcoded = true;
        else if (arg == "--nocull" || arg == "-n")
            ctx.useNoCull = true;
        else if (arg == "--debug" || arg == "-d")
            debugMode = 1;
        else
            ctx.filePath = arg;
    }

    // 资源目录（CMake 构建时复制到 exe 同级的 assets/）
    std::string exeDir = PathUtils::GetExeDir(argv[0]);
    ctx.modelsDir = PathUtils::Join(exeDir, {"assets", "models", "block"});
    ctx.texturesDir = PathUtils::Join(exeDir, {"assets", "textures", "block"});
    ctx.blockstatesDir = PathUtils::Join(exeDir, {"assets", "models", "blockstates"});

    // 加载配置文件
    ctx.appConfig = LoadConfig(exeDir);

    // 设置光影包路径（如果启用）
    if (ctx.appConfig.shaderPack.enabled && !ctx.appConfig.shaderPack.path.empty())
    {
        ctx.shaderPackPath = PathUtils::Join(exeDir, {ctx.appConfig.shaderPack.path.c_str()});
    }

    Log("模式: %s", ctx.useNoCull ? "无剔除" : "带剔除");

    // === 注册阶段实现 ===
    RegisterLoadStage();
    RegisterMeshStage();
    RegisterShaderPackStage();

    // === 运行数据处理流水线 ===
    Pipeline pipeline;
    pipeline.SetFlow({StageType::Load, StageType::MeshBuild, StageType::ShaderPack});

    if (!pipeline.Run(ctx))
    {
        if (!ctx.useHardcoded && ctx.filePath.empty())
        {
            LogError("用法: %s <文件.litematic> 或 %s --test", argv[0], argv[0]);
            LogError("  -n, --nocull    无剔除模式");
            LogError("  -d, --debug     调试模式（面方向着色）");
        }
        return 1;
    }

    // === 初始化应用程序 ===
    auto platform = std::make_unique<GlfwPlatform>();
    App app;
    if (!app.Init(std::move(platform), ctx, debugMode))
    {
        LogError("应用程序初始化失败");
        return 1;
    }

    // === 运行渲染循环 ===
    RenderLoop loop;
    int result = loop.Run(app.GetPlatform(), app);

    // === 清理资源 ===
    app.Shutdown();

    return result;
}
