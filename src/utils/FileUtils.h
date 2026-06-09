#pragma once

#include <string>
#include <filesystem>
#include <functional>

namespace fs = std::filesystem;

// 遍历目录中指定扩展名的文件，对每个文件调用callback
// callback签名：void(const std::string &filePath, const std::string &stemName)
// 返回匹配扩展名的文件数量
template<typename Callback>
inline int ForEachFileInDir(const std::string &dirPath, const std::string &ext, Callback callback)
{
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath))
        return 0;

    int count = 0;
    for (auto &entry : fs::directory_iterator(dirPath))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ext) continue;

        // 文件名去掉扩展名
        std::string stem = entry.path().stem().string();
        callback(entry.path().string(), stem);
        count++;
    }
    return count;
}
