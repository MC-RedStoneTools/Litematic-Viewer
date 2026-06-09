#include "Texture.h"
#include "../../utils/Log.h"
#include "../../utils/FileUtils.h"
#include "../../utils/StringUtils.h"
#include <filesystem>
#include <vector>
#include <iostream>

static LogSource gLog("Texture");

// 在一个cpp中定义STB_IMAGE_IMPLEMENTATION以启用stb_image实现
#define STB_IMAGE_IMPLEMENTATION
#include "extern/stb/stb_image.h"

namespace fs = std::filesystem;

bool TextureManager::LoadTexturesFromDir(const std::string &dirPath)
{
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath))
    {
        gLog.Error("纹理目录不存在: %s", dirPath.c_str());
        return false;
    }

    int count = 0;
    ForEachFileInDir(dirPath, ".png", [&](const std::string &filePath, const std::string &stem) {
        // 文件名去掉.png就是方块名，加上minecraft:前缀匹配palette
        std::string blockName = "minecraft:" + stem;
        GLuint tex = LoadTexture(filePath);
        if (tex)
        {
            m_Textures[blockName] = tex;
            count++;
        }
    });
    gLog.Info("已加载 %d 个方块纹理", count);

    // 别名映射：方块名→已有纹理名
    static const std::pair<const char*, const char*> aliases[] = {
        {"minecraft:redstone_wall_torch", "minecraft:redstone_torch"},
        {"minecraft:redstone_floor_torch", "minecraft:redstone_torch"},
        {"minecraft:soul_wall_torch", "minecraft:soul_torch"},
        {"minecraft:oak_pressure_plate", "minecraft:oak_planks"},
        {"minecraft:birch_pressure_plate", "minecraft:birch_planks"},
        {"minecraft:spruce_pressure_plate", "minecraft:spruce_planks"},
        {"minecraft:stone_pressure_plate", "minecraft:stone"},
        {"minecraft:heavy_weighted_pressure_plate", "minecraft:iron_block"},
        {"minecraft:light_weighted_pressure_plate", "minecraft:gold_block"},
        {"minecraft:dispenser", "minecraft:dispenser_front"},
        {"minecraft:dropper", "minecraft:dropper_front"},
        {"minecraft:observer", "minecraft:observer_front"},
        {"minecraft:hopper", "minecraft:hopper_outside"},
        {"minecraft:furnace", "minecraft:furnace_front"},
        {"minecraft:blast_furnace", "minecraft:blast_furnace_front"},
        {"minecraft:smoker", "minecraft:smoker_front"},
        {"minecraft:quartz_block", "minecraft:quartz_block_side"},
        {"minecraft:smooth_quartz", "minecraft:quartz_block_bottom"},
        {"minecraft:cartography_table", "minecraft:cartography_table_top"},
        {"minecraft:smithing_table", "minecraft:smithing_table_front"},
        {"minecraft:loom", "minecraft:loom_front"},
        {"minecraft:barrel", "minecraft:barrel_side"},
        {"minecraft:lectern", "minecraft:lectern_front"},
        {"minecraft:grindstone", "minecraft:grindstone_side"},
        {"minecraft:stonecutter", "minecraft:stonecutter_front"},
        {"minecraft:crafting_table", "minecraft:crafting_table_front"},
        {"minecraft:composter", "minecraft:composter_side"},
        {"minecraft:beehive", "minecraft:beehive_end"},
        {"minecraft:bee_nest", "minecraft:bee_nest_front"},
        {"minecraft:bookshelf", "minecraft:oak_planks"},
    };
    for (auto &[alias, target] : aliases)
    {
        if (m_Textures.find(alias) == m_Textures.end())
        {
            auto it = m_Textures.find(target);
            if (it != m_Textures.end())
                m_Textures[alias] = it->second; // 共享同一个纹理ID
        }
    }

    return count > 0;
}

GLuint TextureManager::GetBlockTexture(const std::string &blockName) const
{
    // 精确查找
    auto it = m_Textures.find(blockName);
    if (it != m_Textures.end()) return it->second;

    // 提取短名（去掉minecraft:前缀）
    std::string shortName = StripNamespace(blockName);

    // 处理 "block/xxx" 格式（模型纹理变量解析后的格式）
    // "block/lantern" → 提取 "lantern"，然后查找 "minecraft:lantern"
    if (shortName.size() > 6 && shortName.substr(0, 6) == "block/")
    {
        std::string bare = shortName.substr(6);
        std::string candidate = "minecraft:" + bare;
        it = m_Textures.find(candidate);
        if (it != m_Textures.end()) return it->second;
    }

    // 尝试常见后缀组合
    static const char *suffixes[] = {
        "_side", "_top", "_front", "_outside", "_end", "_planks",
        "_wall", "_floor", "_ceiling", "_on", "_off",
    };
    for (auto &suf : suffixes)
    {
        std::string candidate = "minecraft:" + shortName + suf;
        it = m_Textures.find(candidate);
        if (it != m_Textures.end()) return it->second;
    }

    // 去掉_block后缀重试
    if (shortName.size() > 6 && shortName.substr(shortName.size() - 6) == "_block")
    {
        std::string bare = shortName.substr(0, shortName.size() - 6);
        std::string candidate = "minecraft:" + bare;
        it = m_Textures.find(candidate);
        if (it != m_Textures.end()) return it->second;
    }

    // 模糊匹配：去掉_wall/_floor等后缀
    std::string stripped = "minecraft:" + shortName;
    static const char *removeSuffixes[] = {"_wall", "_floor", "_ceiling"};
    for (auto &suf : removeSuffixes)
    {
        auto pos = stripped.find(suf);
        if (pos != std::string::npos)
        {
            stripped.erase(pos, strlen(suf));
            it = m_Textures.find(stripped);
            if (it != m_Textures.end()) return it->second;
        }
    }

    return 0;
}

void TextureManager::Destroy()
{
    for (auto &[name, tex] : m_Textures)
        glDeleteTextures(1, &tex);
    m_Textures.clear();
}

GLuint TextureManager::LoadTexture(const std::string &filePath)
{
    // stbi加载图片，强制RGBA 4通道
    int width, height, channels;
    unsigned char *data = stbi_load(filePath.c_str(), &width, &height, &channels, 4);
    if (!data)
    {
        gLog.Warn("纹理加载失败: %s", filePath.c_str());
        return 0;
    }

    // 检查是否有mcmeta文件（动画纹理）
    // 动画纹理垂直堆叠多帧，只使用第一帧（16×16区域）
    int uploadHeight = height;
    unsigned char *uploadData = data;
    std::vector<unsigned char> croppedData;

    if (height > width)
    {
        // 动画纹理：高度大于宽度，只取第一帧
        uploadHeight = width;
        size_t frameSize = width * uploadHeight * 4;
        croppedData.resize(frameSize);
        memcpy(croppedData.data(), data, frameSize);
        uploadData = croppedData.data();
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // 上传纹理数据（RGBA），动画纹理只上传第一帧
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, uploadHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, uploadData);

    // 设置纹理参数：最近邻采样（像素风格），边缘重复
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);
    return texture;
}
