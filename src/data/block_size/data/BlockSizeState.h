#pragma once

#include <array>
#include <vector>
#include <string>
#include <map>

// 方块尺寸状态枚举
enum class BlockSizeState
{
    FullBlock,      // 完整方块：1x1x1，如石头、泥土
    SpecialSize,    // 特殊尺寸：非标准尺寸，如台阶、楼梯、末地烛
    NonShape        // 非形状尺寸：无标准几何体，如告示牌、花盆
};

// 面索引枚举
enum class FaceIndex : int
{
    East = 0,   // +X
    West = 1,   // -X
    Up = 2,     // +Y
    Down = 3,   // -Y
    South = 4,  // +Z
    North = 5,  // -Z
    Count = 6
};

// Element旋转数据（参照Minecraft BlockElementRotation）
struct ElementRotation
{
    std::array<float, 3> origin = {8, 8, 8};  // 旋转中心
    char axis = 'y';                            // 旋转轴: 'x', 'y', 'z'
    float angle = 0;                            // 旋转角度（度），范围[-45, 45]
    bool rescale = false;                       // 旋转后是否缩放
    bool hasRotation = false;                   // 是否有旋转数据
};

// 单个面的UV数据 [u1, v1, u2, v2]，范围[0,16]
struct FaceUV
{
    float u1 = 0, v1 = 0, u2 = 16, v2 = 16;  // 默认覆盖整个面
    bool hasUV = false;  // 是否有自定义UV
    int rotation = 0;    // 面UV旋转：0, 90, 180, 270（顺时针）
};

// 尺寸数据（从JSON解析出的原始数据）
struct SizeData
{
    std::array<float, 3> from = {0, 0, 0};   // 起始坐标 [0,16]
    std::array<float, 3> to = {16, 16, 16};  // 结束坐标 [0,16]
    FaceUV faceUV[6];  // 6个面的UV数据
    bool faceEnabled[6] = {false};  // 6个面是否启用（在JSON中定义）
    std::string faceTexture[6];  // 6个面的纹理变量引用（如 "#side", "#top"）
    ElementRotation rotation;  // Element级旋转
};

// 单个方块的完整尺寸信息
struct BlockSizeInfo
{
    BlockSizeState state = BlockSizeState::FullBlock;
    std::vector<SizeData> elements;  // 所有元素的尺寸数据
    std::string blockName;           // 方块名称
    std::map<std::string, std::string> textureVars;  // 纹理变量映射（如 "#side" → "minecraft:block/oak_planks"）
    float modelRotX = 0;             // Blockstate变体的X轴旋转（度）
    float modelRotY = 0;             // Blockstate变体的Y轴旋转（度）

    // 判断是否为完整方块尺寸
    bool IsFullBlockSize() const
    {
        if (elements.size() != 1) return false;
        auto &e = elements[0];
        return e.from[0] == 0 && e.from[1] == 0 && e.from[2] == 0 &&
               e.to[0] == 16 && e.to[1] == 16 && e.to[2] == 16;
    }

    // 是否有模型级旋转
    bool HasModelRotation() const { return modelRotX != 0 || modelRotY != 0; }
};

// 状态转换函数
inline BlockSizeState DetermineState(const std::string &blockName, const std::vector<SizeData> &elements)
{
    // 无元素数据 -> 非形状
    if (elements.empty())
        return BlockSizeState::NonShape;

    // 单个元素且为完整尺寸 -> 完整方块
    if (elements.size() == 1)
    {
        auto &e = elements[0];
        if (e.from[0] == 0 && e.from[1] == 0 && e.from[2] == 0 &&
            e.to[0] == 16 && e.to[1] == 16 && e.to[2] == 16)
            return BlockSizeState::FullBlock;
    }

    // 其他情况 -> 特殊尺寸
    return BlockSizeState::SpecialSize;
}

// 获取状态名称（用于日志）
inline const char* GetStateName(BlockSizeState state)
{
    switch (state)
    {
    case BlockSizeState::FullBlock:   return "完整方块";
    case BlockSizeState::SpecialSize: return "特殊尺寸";
    case BlockSizeState::NonShape:    return "非形状";
    default: return "未知";
    }
}
