#pragma once

#include <memory>
#include <string>
#include "../data/loader/BlockDecoder.h"
#include "../data/block_size/parsers/BlockSizeParser.h"
#include "../data/block_size/mappers/SizeToModelMapper.h"

// 面剔除策略接口
class IFaceCullStrategy
{
public:
    virtual ~IFaceCullStrategy() = default;

    // 对指定方块进行面剔除，返回剔除后的渲染数据
    virtual BlockRenderData CullFaces(
        const RegionData &region,
        int x, int y, int z,
        const BlockRenderData &renderData,
        const std::string &blockName
    ) const = 0;
};

// 默认剔除策略：完整方块应用面剔除
class DefaultCullStrategy : public IFaceCullStrategy
{
public:
    BlockRenderData CullFaces(
        const RegionData &region,
        int x, int y, int z,
        const BlockRenderData &renderData,
        const std::string &blockName
    ) const override;
};

// 无剔除策略：直接返回原始数据
class NoCullStrategy : public IFaceCullStrategy
{
public:
    BlockRenderData CullFaces(
        const RegionData &region,
        int x, int y, int z,
        const BlockRenderData &renderData,
        const std::string &blockName
    ) const override;
};

// 面剔除器：管理剔除策略
class FaceCuller
{
public:
    FaceCuller(std::unique_ptr<IFaceCullStrategy> strategy = nullptr);

    // 设置剔除策略
    void SetStrategy(std::unique_ptr<IFaceCullStrategy> strategy);

    // 执行面剔除
    BlockRenderData CullFaces(
        const RegionData &region,
        int x, int y, int z,
        const BlockRenderData &renderData,
        const std::string &blockName
    ) const;

private:
    std::unique_ptr<IFaceCullStrategy> m_Strategy;
};
