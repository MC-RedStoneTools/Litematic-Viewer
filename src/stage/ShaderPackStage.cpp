#include "ShaderPackStage.h"
#include "../pipeline/Pipeline.h"
#include "../shader_pack/loader/ShaderPackLoader.h"
#include "../shader_pack/runtime/ShaderPackRuntime.h"
#include "../utils/Log.h"

#include <filesystem>

namespace fs = std::filesystem;

static LogSource gLog("ShaderPackStage");

// 光影包运行时实例（全局，生命周期由 Stage + App 管理）
static ShaderPackRuntime g_ShaderPackRuntime;

static bool ProcessShaderPack(PipelineContext &ctx)
{
    if (ctx.shaderPackPath.empty())
    {
        gLog.Info("未配置光影包，跳过");
        return true;
    }

    ShaderPackLoader loader;
    ShaderPackData packData;
    bool loaded = false;

    if (fs::is_directory(ctx.shaderPackPath))
        loaded = loader.LoadFromDirectory(ctx.shaderPackPath, packData);
    else
        loaded = loader.LoadFromFile(ctx.shaderPackPath, packData);

    if (!loaded)
    {
        gLog.Warn("加载光影包失败，将使用默认渲染: %s", ctx.shaderPackPath.c_str());
        return true;
    }

    if (!g_ShaderPackRuntime.Load(packData))
    {
        gLog.Warn("光影包数据无效，将使用默认渲染");
        return true;
    }

    ctx.shaderPackRuntime = &g_ShaderPackRuntime;
    gLog.Info("光影包数据加载成功: %s（GPU 初始化将在窗口创建后进行）", packData.packName.c_str());
    return true;
}

void RegisterShaderPackStage()
{
    Pipeline::Register(StageType::ShaderPack, "光影包加载", ProcessShaderPack);
}
