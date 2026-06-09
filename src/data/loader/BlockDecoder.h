#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <map>

// 方块信息
struct BlockInfo
{
    std::string name;                              // 如 "minecraft:stone"
    std::map<std::string, std::string> properties; // 如 facing=east
};

// Region解码结果
struct RegionData
{
    int sizeX, sizeY, sizeZ;
    int offsetX = 0, offsetY = 0, offsetZ = 0;  // 在litematic中的位置偏移
    std::vector<BlockInfo> palette;    // 方块调色板
    std::vector<uint16_t> blocks;      // 一维数组模拟3D，值为palette索引
    // 存储顺序：X最快变化，Y次之，Z最慢

    // 计算3D坐标对应的线性索引
    // 公式：x + y * sizeX + z * sizeX * sizeY
    static constexpr int CalcBlockIndex(int x, int y, int z, int sizeX, int sizeY)
    {
        return x + y * sizeX + z * sizeX * sizeY;
    }

    // 通过3D坐标获取palette索引
    uint16_t GetBlockIndex(int x, int y, int z) const
    {
        return blocks[CalcBlockIndex(x, y, z, sizeX, sizeY)];
    }

    // 通过3D坐标获取方块信息
    const BlockInfo& GetBlock(int x, int y, int z) const
    {
        return palette[GetBlockIndex(x, y, z)];
    }
};

// 计算所有Region的包围盒边界
inline void CalcRegionsBoundingBox(const std::vector<RegionData> &regions,
    int &minX, int &minY, int &minZ, int &maxX, int &maxY, int &maxZ)
{
    minX = minY = minZ = 0;
    maxX = maxY = maxZ = 0;

    for (auto &rd : regions)
    {
        minX = std::min(minX, rd.offsetX);
        minY = std::min(minY, rd.offsetY);
        minZ = std::min(minZ, rd.offsetZ);
        maxX = std::max(maxX, rd.offsetX + rd.sizeX);
        maxY = std::max(maxY, rd.offsetY + rd.sizeY);
        maxZ = std::max(maxZ, rd.offsetZ + rd.sizeZ);
    }
}

// 从LongArray解码BlockStates为一维数组
bool DecodeBlockStates(
    const int64_t *longArray, size_t longCount,
    int sizeX, int sizeY, int sizeZ,
    size_t paletteSize,
    std::vector<uint16_t> &outBlocks
);
