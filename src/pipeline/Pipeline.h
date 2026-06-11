#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>

#include "../utils/Log.h"
#include "../utils/ConfigLoader.h"
#include "../data/loader/BlockDecoder.h"
#include "../data/block_size/parsers/BlockSizeParser.h"
#include "../data/block_size/parsers/BlockstateParser.h"
#include "../data/block_size/mappers/SizeToModelMapper.h"
#include "../data/MeshData.h"

// 模型尺寸信息：所有Region的总包围盒
struct ModelSize
{
    int sizeX = 0;  // 总宽度
    int sizeY = 0;  // 总高度
    int sizeZ = 0;  // 总深度
    int totalBlocks = 0;  // 总方块数
};

// 光影包运行时前向声明
class ShaderPackRuntime;

// 流程上下文：在各阶段之间传递数据
struct PipelineContext
{
    // 输入
    std::string filePath;           // litematic文件路径
    std::string modelsDir;          // 方块模型目录路径
    std::string texturesDir;        // 纹理目录路径
    std::string blockstatesDir;     // Blockstate目录路径
    bool useHardcoded = false;      // 使用硬编码测试数据
    bool useNoCull = false;         // 无剔除模式
    AppConfig appConfig;            // 应用配置

    // 光影包相关（新增）
    std::string shaderPackPath;              // 光影包路径（空表示不使用）
    ShaderPackRuntime *shaderPackRuntime = nullptr;  // 光影包运行时
    int screenWidth = 0;                     // 屏幕宽度
    int screenHeight = 0;                    // 屏幕高度

    // 阶段输出
    std::vector<RegionData> regions;
    MeshData mesh;
    ModelSize modelSize;  // 模型总尺寸
    std::map<std::string, BlockSizeInfo> blockSizes;  // 方块尺寸数据（新系统）
    std::map<std::string, BlockstateData> blockstates;  // Blockstate数据
};

// 阶段枚举
enum class StageType
{
    Load,       // 加载+解码：文件 → RegionData
    MeshBuild,  // 网格生成：RegionData → MeshData
    ShaderPack, // 光影包加载（新增）
    Count       // 阶段总数（哨兵值）
};

// 阶段描述：名称 + 处理函数
struct StageDesc
{
    const char *name = "未注册";
    std::function<bool(PipelineContext &)> process;
};

// 阶段注册表：枚举索引 → 描述
class Pipeline
{
public:
    using Handler = std::function<bool(PipelineContext &)>;

    static void Register(StageType type, const char *name, Handler handler)
    {
        auto &desc = s_Table[static_cast<int>(type)];
        desc.name = name;
        desc.process = std::move(handler);
    }

    void SetFlow(std::initializer_list<StageType> stages)
    {
        m_Flow.assign(stages.begin(), stages.end());
    }

    bool Run(PipelineContext &ctx) const
    {
        int step = 0;
        int total = static_cast<int>(m_Flow.size());

        for (StageType type : m_Flow)
        {
            step++;
            auto &desc = s_Table[static_cast<int>(type)];

            Log("[%d/%d] %s - 开始", step, total, desc.name);

            if (!desc.process(ctx))
            {
                LogError("[%d/%d] %s - 失败", step, total, desc.name);
                return false;
            }

            Log("[%d/%d] %s - 完成", step, total, desc.name);
        }

        Log("全部 %d 个阶段执行完毕", total);
        return true;
    }

    static const char *GetStageName(StageType type)
    {
        return s_Table[static_cast<int>(type)].name;
    }

private:
    std::vector<StageType> m_Flow;
    static StageDesc s_Table[static_cast<int>(StageType::Count)];
};
