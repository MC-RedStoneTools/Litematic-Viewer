#include "BlockTypeClassifier.h"

bool BlockTypeClassifier::IsAir(const RegionData &region, int x, int y, int z)
{
    if (x < 0 || x >= region.sizeX || y < 0 || y >= region.sizeY || z < 0 || z >= region.sizeZ)
        return true;
    const auto &name = region.GetBlock(x, y, z).name;
    return name == "minecraft:air" || name == "minecraft:cave_air" || name == "minecraft:void_air";
}

bool BlockTypeClassifier::IsTransparentBlock(const std::string &name)
{
    if (name == "minecraft:air" || name == "minecraft:cave_air" || name == "minecraft:void_air")
        return true;
    if (name == "minecraft:end_portal"
        || name == "minecraft:nether_portal"
        || name == "minecraft:water" || name == "minecraft:lava"
        || name == "minecraft:glass" || name.find("_stained_glass") != std::string::npos
        || name.find("leaves") != std::string::npos
        || name.find("sapling") != std::string::npos)
        return true;
    return false;
}

bool BlockTypeClassifier::ShouldCullFace(const RegionData &region, int x, int y, int z, int nx, int ny, int nz)
{
    if (nx < 0 || nx >= region.sizeX || ny < 0 || ny >= region.sizeY || nz < 0 || nz >= region.sizeZ)
        return false;
    return !IsTransparentBlock(region.GetBlock(nx, ny, nz).name);
}
