#pragma once

#include <string>
#include <map>
#include <vector>
#include <set>
#include "../parsers/BlockSizeParser.h"

// 模型继承链解析器
// 职责：根据方块列表加载模型文件，解析完整的继承链
class ModelInheritanceResolver
{
public:
    // 根据方块名称列表，加载并解析完整的模型继承链
    // blockNames: 从litematic palette中提取的方块名列表（如 "minecraft:lantern"）
    // modelsDir: 模型JSON文件目录
    // 返回: 方块名 → BlockSizeInfo 的映射
    static std::map<std::string, BlockSizeInfo> Resolve(
        const std::vector<std::string> &blockNames,
        const std::string &modelsDir
    );

private:
    // 从方块名提取模型文件名（去掉minecraft:前缀）
    static std::string ExtractModelName(const std::string &blockName);

    // 加载单个模型文件及其所有父模型（递归）
    static void LoadModelWithParents(
        const std::string &modelName,
        const std::string &modelsDir,
        std::map<std::string, BlockSizeInfo> &loadedModels,
        std::set<std::string> &visited
    );

    // 拓扑排序后执行继承解析
    static void ResolveInheritanceChain(
        std::map<std::string, BlockSizeInfo> &models
    );
};
