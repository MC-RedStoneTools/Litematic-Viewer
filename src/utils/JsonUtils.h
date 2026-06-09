#pragma once

#include <string>
#include <optional>
#include <fstream>
#include <nlohmann/json.hpp>

// JSON文件加载工具：统一处理文件打开、解析和异常捕获
inline std::optional<nlohmann::json> LoadJsonFile(const std::string &filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
        return std::nullopt;

    try {
        nlohmann::json j;
        file >> j;
        return j;
    }
    catch (...) {
        return std::nullopt;
    }
}
