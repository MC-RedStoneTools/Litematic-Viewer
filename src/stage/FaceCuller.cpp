#include "FaceCuller.h"
#include "../data/BlockTypeClassifier.h"

// === DefaultCullStrategy 实现 ===

BlockRenderData DefaultCullStrategy::CullFaces(
    const RegionData &region,
    int x, int y, int z,
    const BlockRenderData &renderData,
    const std::string &blockName
) const
{
    // 只对完整方块应用面剔除
    if (renderData.state != BlockSizeState::FullBlock)
        return renderData;

    BlockRenderData culledData;
    culledData.state = renderData.state;

    // 遍历6个面
    for (int f = 0; f < 6; f++)
    {
        // 计算邻居坐标
        int nx = x + (f == 0 ? 1 : (f == 1 ? -1 : 0));
        int ny = y + (f == 2 ? 1 : (f == 3 ? -1 : 0));
        int nz = z + (f == 4 ? 1 : (f == 5 ? -1 : 0));

        // 判断是否剔除该面
        if (BlockTypeClassifier::ShouldCullFace(region, x, y, z, nx, ny, nz))
            continue;

        // 透明方块特殊处理：与不透明邻居相邻的面也要剔除
        if (BlockTypeClassifier::IsTransparentBlock(blockName))
        {
            if (nx >= 0 && nx < region.sizeX && ny >= 0 && ny < region.sizeY && nz >= 0 && nz < region.sizeZ)
            {
                if (!BlockTypeClassifier::IsTransparentBlock(region.GetBlock(nx, ny, nz).name))
                    continue;
            }
        }

        // 复制该面的顶点（通过FaceBatch索引，完整方块每面对应一个batch）
        if (f < (int)renderData.faceBatches.size())
        {
            culledData.faceBatches.push_back(renderData.faceBatches[f]);
        }

        // 复制该面的线框
        int wireStartIdx = f * 36;  // 每面4条边，每条6个float
        for (int w = wireStartIdx; w < wireStartIdx + 36 && w < (int)renderData.wireVertices.size(); w++)
            culledData.wireVertices.push_back(renderData.wireVertices[w]);
    }

    return culledData;
}

// === NoCullStrategy 实现 ===

BlockRenderData NoCullStrategy::CullFaces(
    const RegionData &region,
    int x, int y, int z,
    const BlockRenderData &renderData,
    const std::string &blockName
) const
{
    // 无剔除模式，直接返回原始数据
    return renderData;
}

// === FaceCuller 实现 ===

FaceCuller::FaceCuller(std::unique_ptr<IFaceCullStrategy> strategy)
    : m_Strategy(std::move(strategy))
{
    // 默认使用 DefaultCullStrategy
    if (!m_Strategy)
        m_Strategy = std::make_unique<DefaultCullStrategy>();
}

void FaceCuller::SetStrategy(std::unique_ptr<IFaceCullStrategy> strategy)
{
    m_Strategy = std::move(strategy);
    // 确保策略不为空
    if (!m_Strategy)
        m_Strategy = std::make_unique<DefaultCullStrategy>();
}

BlockRenderData FaceCuller::CullFaces(
    const RegionData &region,
    int x, int y, int z,
    const BlockRenderData &renderData,
    const std::string &blockName
) const
{
    return m_Strategy->CullFaces(region, x, y, z, renderData, blockName);
}
