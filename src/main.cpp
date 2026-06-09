#include <string>

#include "pipeline/Pipeline.h"
#include "stage/LoadStage.h"
#include "stage/MeshStage.h"
#include "app/App.h"
#include "render/platform/RenderLoop.h"

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

    // 设置资源目录路径（相对于exe: build_mingw/bin/ -> 项目根/assets/）
    std::string exeDir = std::string(argv[0]).substr(0, std::string(argv[0]).find_last_of("\\/") + 1);
    ctx.modelsDir = exeDir + "..\\..\\assets\\models\\block";
    ctx.texturesDir = exeDir + "..\\..\\assets\\textures\\block";
    ctx.blockstatesDir = exeDir + "..\\..\\assets\\models\\blockstates";

    // 加载配置文件
    ctx.appConfig = LoadConfig(exeDir);

    Log("模式: %s", ctx.useNoCull ? "无剔除" : "带剔除");

    // === 注册阶段实现 ===
    RegisterLoadStage();
    RegisterMeshStage();

    // === 运行数据处理流水线 ===
    Pipeline pipeline;
    pipeline.SetFlow({StageType::Load, StageType::MeshBuild});

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
    App app;
    if (!app.Init(ctx, debugMode))
    {
        LogError("应用程序初始化失败");
        return 1;
    }

    // === 运行渲染循环 ===
    RenderLoop loop;
    int result = loop.Run(app);

    // === 清理资源 ===
    app.Shutdown();

    return result;
}
