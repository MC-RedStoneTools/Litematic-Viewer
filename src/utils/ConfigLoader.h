#pragma once

#include <string>
#include <filesystem>
#include "JsonUtils.h"
#include "Log.h"

// 光影包应用配置（用于 config.json）
struct ShaderPackAppConfig
{
    bool enabled = false;               // 是否启用光影包
    std::string path;                   // 光影包路径（ZIP文件或目录）
    int shadowResolution = 1024;        // 阴影贴图分辨率
    float shadowDistance = 128.0f;      // 阴影距离
};

// 配置结构
struct AppConfig
{
    bool cullTransparentBlocks = false;  // 剔除透明方块
    ShaderPackAppConfig shaderPack;      // 光影包配置
};

// 加载配置文件
inline AppConfig LoadConfig(const std::string &exeDir)
{
    AppConfig config;
    std::string configPath = (std::filesystem::path(exeDir) / "config.json").string();

    auto json = LoadJsonFile(configPath);
    if (!json)
    {
        Log("[Config] 配置文件未找到，使用默认配置");
        return config;
    }

    if (json->contains("剔除透明方块"))
        config.cullTransparentBlocks = (*json)["剔除透明方块"].get<bool>();

    // 加载光影包配置
    if (json->contains("shaderPack"))
    {
        auto &sp = (*json)["shaderPack"];
        if (sp.contains("enabled"))
            config.shaderPack.enabled = sp["enabled"].get<bool>();
        if (sp.contains("path"))
            config.shaderPack.path = sp["path"].get<std::string>();
        if (sp.contains("shadowResolution"))
            config.shaderPack.shadowResolution = sp["shadowResolution"].get<int>();
        if (sp.contains("shadowDistance"))
            config.shaderPack.shadowDistance = sp["shadowDistance"].get<float>();
    }

    Log("[Config] 剔除透明方块: %s", config.cullTransparentBlocks ? "是" : "否");
    if (config.shaderPack.enabled)
        Log("[Config] 光影包: %s", config.shaderPack.path.c_str());
    return config;
}
