#pragma once

#include <string>

// 从命名空间ID中提取短名："minecraft:stone" -> "stone"
// 如果没有冒号，返回原字符串
inline std::string StripNamespace(const std::string &namespacedName)
{
    auto colonPos = namespacedName.find(':');
    if (colonPos != std::string::npos)
        return namespacedName.substr(colonPos + 1);
    return namespacedName;
}
