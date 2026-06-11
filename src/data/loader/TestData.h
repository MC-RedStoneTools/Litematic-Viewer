#pragma once

#include "BlockDecoder.h"
#include <vector>

// 硬编码测试数据：5x5 地狱门框架（--test 模式）
inline void LoadHardcodedTestData(std::vector<RegionData> &regions)
{
    regions.clear();

    RegionData rd;
    rd.sizeX = 5;
    rd.sizeY = 5;
    rd.sizeZ = 1;

    rd.palette = {
        {"minecraft:air", {}},
        {"minecraft:obsidian", {}},
        {"minecraft:netherrack", {}},
    };

    rd.blocks.assign(static_cast<size_t>(rd.sizeX * rd.sizeY * rd.sizeZ), 0);

    auto setBlock = [&](int x, int y, int z, uint16_t paletteIdx)
    {
        int idx = RegionData::CalcBlockIndex(x, y, z, rd.sizeX, rd.sizeY, rd.sizeZ);
        rd.blocks[idx] = paletteIdx;
    };

    for (int y = 0; y < rd.sizeY; y++)
        for (int x = 0; x < rd.sizeX; x++)
        {
            if (x == 0 || x == rd.sizeX - 1 || y == 0 || y == rd.sizeY - 1)
                setBlock(x, y, 0, 1);
            else
                setBlock(x, y, 0, 2);
        }

    regions.push_back(std::move(rd));
}
