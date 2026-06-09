#pragma once

#include <string>
#include "loader/BlockDecoder.h"

// 方块类型判断工具类
// 职责：判断方块是否为空气、透明方块等，用于面剔除和渲染决策
class BlockTypeClassifier
{
public:
    // 判断指定位置是否为空气（包括cave_air、void_air）
    static bool IsAir(const RegionData &region, int x, int y, int z);

    // 判断方块是否为透明方块（空气、玻璃、水、树叶等）
    static bool IsTransparentBlock(const std::string &name);

    // 判断是否应该剔除指定方向的面
    // nx/ny/nz 为相邻方块坐标
    static bool ShouldCullFace(const RegionData &region, int x, int y, int z, int nx, int ny, int nz);
};
