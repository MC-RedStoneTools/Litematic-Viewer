#include "BlockDecoder.h"
#include "../../utils/Log.h"
#include <iostream>

static LogSource gLog("BlockDecoder");

// 计算存储一个索引需要的最小位数
// Minecraft/litematica格式最小使用2位（paletteSize>1时）
static int CalcBitsPerEntry(size_t paletteSize)
{
    if (paletteSize <= 1) return 0;
    // ceil(log2(paletteSize))
    int bits = 0;
    size_t v = paletteSize - 1;
    while (v > 0) { v >>= 1; bits++; }
    // Minecraft格式最小为2位
    return bits < 2 ? 2 : bits;
}


bool DecodeBlockStates(
    const int64_t *longArray, size_t longCount,
    int sizeX, int sizeY, int sizeZ,
    size_t paletteSize,
    std::vector<uint16_t> &outBlocks)
{
    int totalBlocks = sizeX * sizeY * sizeZ;
    if (totalBlocks <= 0 || paletteSize == 0)
    {
        gLog.Warn("无效参数: totalBlocks=%d, paletteSize=%zu", totalBlocks, paletteSize);
        return false;
    }

    outBlocks.resize(totalBlocks);

    // paletteSize=1时，所有方块都是同一种（索引0），无需解析BlockStates
    if (paletteSize <= 1)
    {
        std::fill(outBlocks.begin(), outBlocks.end(), 0);
        return true;
    }

    int bitsPerEntry = CalcBitsPerEntry(paletteSize);
    // 每个long能存储的索引数量
    int entriesPerLong = 64 / bitsPerEntry;
    // 每个long中未使用的位数（高位填充）
    int paddingBits = 64 - (entriesPerLong * bitsPerEntry);

    gLog.Debug("解码BlockStates: bitsPerEntry=%d, entriesPerLong=%d, longCount=%zu, totalBlocks=%d",
             bitsPerEntry, entriesPerLong, longCount, totalBlocks);

    // 位掩码，用于提取bitsPerEntry位
    uint64_t mask = (1ULL << bitsPerEntry) - 1;

    // 调试：打印前几个long的值
    for (size_t i = 0; i < std::min((size_t)3, longCount); i++)
    {
        uint64_t v = static_cast<uint64_t>(longArray[i]);
        gLog.Debug("  longArray[%zu] = 0x%llx", i, (unsigned long long)v);
    }

    int blockIndex = 0;
    for (size_t i = 0; i < longCount && blockIndex < totalBlocks; i++)
    {
        // 字节序已由底层读取器处理，直接使用
        uint64_t value = static_cast<uint64_t>(longArray[i]);

        // 从低位到高位逐个提取索引
        for (int j = 0; j < entriesPerLong && blockIndex < totalBlocks; j++)
        {
            uint16_t idx = static_cast<uint16_t>((value >> (j * bitsPerEntry)) & mask);
            outBlocks[blockIndex++] = idx;
        }
    }

    if (blockIndex != totalBlocks)
    {
        gLog.Warn("解码不完整: 期望%d个方块，实际%d个", totalBlocks, blockIndex);
        return false;
    }
    return true;
}
