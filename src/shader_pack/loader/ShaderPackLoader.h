#pragma once

#include "../data/ShaderPackData.h"
#include <vector>
#include <string>
#include <map>
#include <cstdint>

// 光影包加载器：从 ZIP 文件或目录加载光影包
class ShaderPackLoader
{
public:
    // 从 ZIP 文件加载光影包
    bool LoadFromFile(const std::string &zipPath, ShaderPackData &out);

    // 从目录加载光影包（调试用）
    bool LoadFromDirectory(const std::string &dirPath, ShaderPackData &out);

private:
    // 文件路径 → 内容的映射表（用于 #include 解析）
    using FileMap = std::map<std::string, std::string>;

    // 解析 shaders.properties 配置
    bool ParseProperties(const std::string &content, ShaderPackConfig &config);

    // 从 ZIP 读取文件内容到字节缓冲区
    bool ReadFileFromZip(void *zipHandle, const std::string &path,
                         std::vector<uint8_t> &outData);

    // 扫描并加载所有着色器 Pass
    void ScanShaderPasses(void *zipHandle, ShaderPackData &pack);

    // 将文件路径映射到 Pass 名称，返回是否为已知着色器文件
    bool MapPathToPassName(const std::string &path, std::string &outPassName,
                           bool &isVertex);

    // 构建文件映射表（从 ZIP 所有条目）
    void BuildFileMap(void *zipHandle, FileMap &outMap);

    // 递归展开 #include 指令
    static std::string ResolveIncludes(const std::string &source,
                                        const FileMap &fileMap,
                                        int depth = 0);

    // 规范化路径（统一斜杠、去掉前导 /）
    static std::string NormalizePath(const std::string &path);
};
