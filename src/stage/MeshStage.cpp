#include "MeshStage.h"
#include "FaceCuller.h"
#include "../pipeline/Pipeline.h"
#include "../utils/Log.h"
#include "../data/block_size/parsers/BlockSizeParser.h"
#include "../data/block_size/parsers/BlockstateParser.h"
#include "../data/block_size/mappers/SizeToModelMapper.h"
#include "../data/BlockTypeClassifier.h"
#include <map>

static LogSource gLog("MeshStage");

// 查找方块的尺寸信息
static const BlockSizeInfo* FindBlockSize(const std::map<std::string, BlockSizeInfo> &blockSizes,
                                           const std::string &blockName)
{
    // 从 "minecraft:xxx" 提取 "xxx"
    std::string name = blockName;
    auto colonPos = name.find(':');
    if (colonPos != std::string::npos)
        name = name.substr(colonPos + 1);

    auto it = blockSizes.find(name);
    if (it != blockSizes.end())
        return &it->second;

    return nullptr;
}

// 将BlockRenderData的顶点转换为MeshData格式（支持每面不同纹理）
static void ConvertToMeshData(const BlockRenderData &renderData, int blockIdx, MeshData &mesh, const std::string &blockName)
{
    bool isTransparent = BlockTypeClassifier::IsTransparentBlock(blockName);

    // 按FaceBatch转换，每个batch一个drawCall（支持每面不同纹理）
    for (auto &batch : renderData.faceBatches)
    {
        if (batch.vertices.empty()) continue;

        int firstVert = static_cast<int>(mesh.vertices.size()) / VERTEX_FLOAT_COUNT;
        for (auto &v : batch.vertices)
        {
            mesh.vertices.push_back(v.x);
            mesh.vertices.push_back(v.y);
            mesh.vertices.push_back(v.z);
            mesh.vertices.push_back(v.u);
            mesh.vertices.push_back(v.v);
            mesh.vertices.push_back(v.r);
            mesh.vertices.push_back(v.g);
            mesh.vertices.push_back(v.b);
            mesh.vertices.push_back(v.nx);
            mesh.vertices.push_back(v.ny);
            mesh.vertices.push_back(v.nz);
            mesh.vertices.push_back(v.lu);
            mesh.vertices.push_back(v.lv);
        }

        BlockDrawCall dc;
        dc.textureName = batch.textureName;
        dc.blockName = blockName;
        dc.blockIndex = blockIdx;
        dc.firstVertex = firstVert;
        dc.vertexCount = static_cast<int>(batch.vertices.size());
        dc.isTransparent = isTransparent;
        mesh.drawCalls.push_back(dc);
    }

    // 转换线框顶点
    mesh.wireVertices.insert(mesh.wireVertices.end(),
                              renderData.wireVertices.begin(),
                              renderData.wireVertices.end());
}

// === 网格生成（使用新的尺寸系统）===

static void BuildMesh(const RegionData &region, MeshData &mesh,
                      const std::map<std::string, BlockSizeInfo> &blockSizes,
                      const std::map<std::string, BlockstateData> &blockstates,
                      bool useNoCull, const AppConfig &appConfig)
{
    int ox = region.offsetX, oy = region.offsetY, oz = region.offsetZ;

    // 根据配置选择剔除策略
    FaceCuller culler;
    if (useNoCull)
        culler.SetStrategy(std::make_unique<NoCullStrategy>());

    for (int x = 0; x < region.sizeX; x++)
        for (int y = 0; y < region.sizeY; y++)
            for (int z = 0; z < region.sizeZ; z++)
            {
                if (BlockTypeClassifier::IsAir(region, x, y, z)) continue;

                int blockIdx = region.GetBlockIndex(x, y, z);
                auto &blockName = region.GetBlock(x, y, z).name;

                // 查找方块尺寸信息
                const BlockSizeInfo *sizeInfo = FindBlockSize(blockSizes, blockName);

                // 配置：剔除透明方块时，跳过没有模型的透明方块
                if (appConfig.cullTransparentBlocks && !sizeInfo && BlockTypeClassifier::IsTransparentBlock(blockName))
                    continue;

                // 根据是否有尺寸信息决定渲染方式
                BlockRenderData renderData;

                if (sizeInfo)
                {
                    // 使用新的尺寸系统生成渲染数据
                    renderData = SizeToModelMapper::GenerateRenderData(*sizeInfo, x, y, z, ox, oy, oz);
                }
                else
                {
                    // 没有尺寸信息，使用完整方块
                    renderData = SizeToModelMapper::GenerateFullBlock(x, y, z, ox, oy, oz);
                }

                // 查找Blockstate变体，应用模型级旋转
                // 从 "minecraft:xxx" 提取 "xxx" 用于blockstate查找
                std::string bsName = blockName;
                auto colonPos = bsName.find(':');
                if (colonPos != std::string::npos)
                    bsName = bsName.substr(colonPos + 1);

                auto bsIt = blockstates.find(bsName);
                if (bsIt != blockstates.end())
                {
                    const BlockstateVariant *variant = BlockstateParser::FindVariant(
                        bsIt->second, region.GetBlock(x, y, z).properties);
                    if (variant && (variant->x != 0 || variant->y != 0))
                    {
                        SizeToModelMapper::ApplyModelRotation(
                            renderData, variant->x, variant->y, x, y, z, ox, oy, oz);
                    }
                }

                // 应用面剔除策略
                BlockRenderData finalData = culler.CullFaces(region, x, y, z, renderData, blockName);
                ConvertToMeshData(finalData, blockIdx, mesh, blockName);
            }
}

// === 阶段处理函数 ===

static bool ProcessMeshBuild(PipelineContext &ctx)
{
    if (ctx.regions.empty()) return false;

    // 为每个Region生成网格
    for (size_t r = 0; r < ctx.regions.size(); r++)
    {
        const RegionData &region = ctx.regions[r];
        gLog.Info("Region %d: %dx%dx%d offset(%d,%d,%d)",
            (int)r, region.sizeX, region.sizeY, region.sizeZ,
            region.offsetX, region.offsetY, region.offsetZ);

        BuildMesh(region, ctx.mesh, ctx.blockSizes, ctx.blockstates, ctx.useNoCull, ctx.appConfig);
    }

    gLog.Info("生成 %d 个三角形", ctx.mesh.GetTriangleCount());
    return true;
}

void RegisterMeshStage()
{
    Pipeline::Register(StageType::MeshBuild, "网格生成", ProcessMeshBuild);
}
