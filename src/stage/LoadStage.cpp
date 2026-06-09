#include "LoadStage.h"
#include "../pipeline/Pipeline.h"
#include "../utils/Log.h"
#include "../data/loader/LitematicLoader.h"
#include "../data/block_size/parsers/BlockSizeParser.h"
#include "../data/block_size/resolvers/ModelInheritanceResolver.h"

#include <algorithm>
#include <set>

static LogSource gLog("LoadStage");

// 从regions的palette中提取所有方块名称
static std::vector<std::string> ExtractBlockNames(const std::vector<RegionData> &regions)
{
    std::set<std::string> uniqueNames;
    for (auto &rd : regions)
    {
        for (auto &block : rd.palette)
        {
            if (!block.name.empty())
            {
                uniqueNames.insert(block.name);
            }
        }
    }
    return std::vector<std::string>(uniqueNames.begin(), uniqueNames.end());
}

static bool ProcessLoad(PipelineContext &ctx)
{
    if (!ctx.filePath.empty())
    {
        if (!LoadLitematic(ctx.filePath, ctx.regions))
        {
            gLog.Error("读取失败: %s", ctx.filePath.c_str());
            return false;
        }
    }
    else
    {
        gLog.Error("未指定文件路径");
        return false;
    }

    // 加载方块尺寸数据（使用新的继承解析器）
    if (!ctx.modelsDir.empty() && !ctx.regions.empty())
    {
        // 从palette中提取所有需要的方块名称
        auto blockNames = ExtractBlockNames(ctx.regions);
        gLog.Info("需要加载 %d 种方块的模型", (int)blockNames.size());

        // 使用继承解析器加载并解析完整的继承链
        ctx.blockSizes = ModelInheritanceResolver::Resolve(blockNames, ctx.modelsDir);
    }

    // 加载Blockstate数据（用于模型旋转映射）
    if (!ctx.blockstatesDir.empty())
    {
        ctx.blockstates = BlockstateParser::ParseDirectory(ctx.blockstatesDir);
    }

    if (!ctx.regions.empty())
    {
        // 打印各Region信息
        for (size_t i = 0; i < ctx.regions.size(); i++)
        {
            auto &rd = ctx.regions[i];
            gLog.Info("Region %d尺寸: %dx%dx%d offset(%d,%d,%d)",
                (int)i, rd.sizeX, rd.sizeY, rd.sizeZ,
                rd.offsetX, rd.offsetY, rd.offsetZ);
        }

        // 计算所有Region的总包围盒尺寸
        int minX, minY, minZ, maxX, maxY, maxZ;
        CalcRegionsBoundingBox(ctx.regions, minX, minY, minZ, maxX, maxY, maxZ);

        ctx.modelSize.sizeX = maxX - minX;
        ctx.modelSize.sizeY = maxY - minY;
        ctx.modelSize.sizeZ = maxZ - minZ;

        // 统计总方块数
        ctx.modelSize.totalBlocks = 0;
        for (auto &rd : ctx.regions)
        {
            for (size_t i = 0; i < rd.blocks.size(); i++)
            {
                auto &block = rd.palette[rd.blocks[i]];
                if (block.name != "minecraft:air" &&
                    block.name != "minecraft:cave_air" &&
                    block.name != "minecraft:void_air")
                    ctx.modelSize.totalBlocks++;
            }
        }

        gLog.Info("模型总尺寸: %dx%dx%d, 总方块数: %d",
            ctx.modelSize.sizeX, ctx.modelSize.sizeY, ctx.modelSize.sizeZ,
            ctx.modelSize.totalBlocks);
    }

    return !ctx.regions.empty();
}

void RegisterLoadStage()
{
    Pipeline::Register(StageType::Load, "加载", ProcessLoad);
}
