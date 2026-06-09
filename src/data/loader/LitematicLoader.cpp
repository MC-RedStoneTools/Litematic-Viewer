#include "LitematicLoader.h"
#include "../../utils/Log.h"

#include <vector>

static LogSource gLog("Litematic");
#include <string>
#include <cstdint>
#include <fstream>

// NBT解析库
#include "NBT_All.hpp"

// 手动读取文件字节流（避免std::filesystem中文路径问题）
static bool ReadFileBytes(const std::string &filePath, std::vector<uint8_t> &out)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        gLog.Error("无法打开文件: %s", filePath.c_str());
        return false;
    }
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(out.data()), size);
    if (!file.good())
    {
        gLog.Error("文件读取失败: %s", filePath.c_str());
        return false;
    }
    return true;
}

// 从NBT的Compound中读取单个Region并解码
static bool LoadRegion(NBT_Type::Compound &region, RegionData &out)
{
    // 读取Size
    auto *pSize = region.HasCompound(MU8STR("Size"));
    if (!pSize)
    {
        gLog.Warn("Region缺少Size字段");
        return false;
    }

    auto *sx = pSize->HasInt(MU8STR("x"));
    auto *sy = pSize->HasInt(MU8STR("y"));
    auto *sz = pSize->HasInt(MU8STR("z"));
    if (!sx || !sy || !sz)
    {
        gLog.Warn("Region Size字段不完整");
        return false;
    }

    // Size可能为负数，取绝对值作为实际尺寸
    out.sizeX = *sx > 0 ? *sx : -*sx;
    out.sizeY = *sy > 0 ? *sy : -*sy;
    out.sizeZ = *sz > 0 ? *sz : -*sz;

    // 读取Position（Region在litematic中的偏移）
    auto *pPos = region.HasCompound(MU8STR("Position"));
    if (pPos)
    {
        auto *px = pPos->HasInt(MU8STR("x"));
        auto *py = pPos->HasInt(MU8STR("y"));
        auto *pz = pPos->HasInt(MU8STR("z"));
        if (px) out.offsetX = *px;
        if (py) out.offsetY = *py;
        if (pz) out.offsetZ = *pz;
    }

    // 读取Palette
    auto *pPalette = region.HasList(MU8STR("BlockStatePalette"));
    if (!pPalette)
    {
        gLog.Warn("Region缺少BlockStatePalette");
        return false;
    }

    out.palette.clear();
    for (auto &entry : *pPalette)
    {
        BlockInfo info;
        auto &blockCpd = entry.GetCompound();
        auto *pName = blockCpd.HasString(MU8STR("Name"));
        if (pName)
            info.name = std::string(pName->begin(), pName->end());

        // 读取Properties
        auto *pProps = blockCpd.HasCompound(MU8STR("Properties"));
        if (pProps)
        {
            for (auto &[key, val] : *pProps)
            {
                if (val.IsString())
                {
                    auto &s = val.GetString();
                    info.properties[std::string(key.begin(), key.end())] =
                        std::string(s.begin(), s.end());
                }
            }
        }
        out.palette.push_back(std::move(info));
    }

    // 解码BlockStates
    auto *pBlockStates = region.HasLongArray(MU8STR("BlockStates"));
    if (!pBlockStates)
    {
        gLog.Warn("Region缺少BlockStates");
        return false;
    }

    return DecodeBlockStates(
        pBlockStates->data(), pBlockStates->size(),
        out.sizeX, out.sizeY, out.sizeZ,
        out.palette.size(),
        out.blocks
    );
}

bool LoadLitematic(const std::string &filePath, std::vector<RegionData> &regions)
{
    regions.clear();

    // 1. 读取文件
    std::vector<uint8_t> fileStream{};
    if (!ReadFileBytes(filePath, fileStream))
    {
        gLog.Error("无法读取文件: %s", filePath.c_str());
        return false;
    }
    gLog.Info("文件大小: %zu 字节", fileStream.size());

    // 2. 解压
    std::vector<uint8_t> nbtStream{};
    if (!NBT_IO::DecompressDataNoThrow(nbtStream, fileStream))
        nbtStream = std::move(fileStream);
    else
        gLog.Info("解压后大小: %zu 字节", nbtStream.size());

    // 3. 解析NBT
    NBT_Type::Compound rootCompound{};
    if (!NBT_Reader::ReadNBT(nbtStream, 0, rootCompound))
    {
        gLog.Error("NBT解析失败");
        return false;
    }

    // 4. 获取根节点
    auto *pRoot = rootCompound.HasCompound(MU8STR(""));
    if (!pRoot)
    {
        gLog.Error("找不到根Compound");
        return false;
    }
    auto &data = *pRoot;

    // 5. 读取元数据
    auto *pMetadata = data.HasCompound(MU8STR("Metadata"));
    if (pMetadata)
    {
        auto *pName = pMetadata->HasString(MU8STR("Name"));
        auto *pTotalBlocks = pMetadata->HasInt(MU8STR("TotalBlocks"));
        if (pName) gLog.Info("名称: %s", std::string(pName->begin(), pName->end()).c_str());
        if (pTotalBlocks) gLog.Info("总方块数: %d", *pTotalBlocks);
    }

    // 6. 遍历Regions并解码
    auto *pRegions = data.HasCompound(MU8STR("Regions"));
    if (!pRegions)
    {
        gLog.Error("找不到Regions");
        return false;
    }

    for (auto &[regionName, regionNode] : *pRegions)
    {
        auto &regionCpd = regionNode.GetCompound();
        RegionData rd;

        if (LoadRegion(regionCpd, rd))
        {
            std::string name(regionName.begin(), regionName.end());
            gLog.Info("Region '%s': %dx%dx%d, %zu种方块, %zu个方块数据",
                name.c_str(), rd.sizeX, rd.sizeY, rd.sizeZ,
                rd.palette.size(), rd.blocks.size());
            regions.push_back(std::move(rd));
        }
        else
        {
            gLog.Error("Region解码失败");
        }
    }

    return !regions.empty();
}
