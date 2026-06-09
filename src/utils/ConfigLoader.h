#pragma once

#include <string>
#include "JsonUtils.h"
#include "Log.h"

// 配置结构
struct AppConfig
{
    bool cullTransparentBlocks = false;  // 剔除透明方块
};

// 加载配置文件
inline AppConfig LoadConfig(const std::string &exeDir)
{
    AppConfig config;
    std::string configPath = exeDir + "..\\..\\config.json";

    auto json = LoadJsonFile(configPath);
    if (!json)
    {
        Log("[Config] 配置文件未找到，使用默认配置");
        return config;
    }

    if (json->contains("剔除透明方块"))
        config.cullTransparentBlocks = (*json)["剔除透明方块"].get<bool>();

    Log("[Config] 剔除透明方块: %s", config.cullTransparentBlocks ? "是" : "否");
    return config;
}
