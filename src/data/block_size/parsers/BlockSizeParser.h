#pragma once

#include "../data/BlockSizeState.h"
#include <string>
#include <map>

// JSON解析组件：将模型JSON文件解析为BlockSizeInfo
// 职责：读取文件、解析JSON、提取from/to坐标、确定状态
// 注意：继承链解析由 ModelInheritanceResolver 负责
class BlockSizeParser
{
public:
    // 解析单个模型文件
    static bool ParseFile(const std::string &filePath, BlockSizeInfo &out);

    // 解析elements数组（供外部继承解析器使用）
    static void ParseElements(const void *jsonRoot, std::vector<SizeData> &elements);
};
