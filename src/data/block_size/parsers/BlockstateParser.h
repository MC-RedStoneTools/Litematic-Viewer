#pragma once

#include <string>
#include <map>
#include <vector>

// Blockstate变体：一个变体包含模型引用和旋转信息
struct BlockstateVariant
{
    std::string key;             // 变体键，如 "facing=east,half=bottom"
    std::string model;           // 模型路径，如 "block/end_rod"
    float x = 0;                 // X轴旋转（度）
    float y = 0;                 // Y轴旋转（度）
    bool uvLock = false;         // UV锁定
    int weight = 1;              // 权重（用于随机变体）
};

// Blockstate条件：用于multipart格式的条件匹配
struct BlockstateCondition
{
    std::map<std::string, std::string> properties;  // 属性条件
};

// Blockstate条目：条件+应用
struct BlockstateApply
{
    BlockstateCondition when;    // 条件
    BlockstateVariant apply;     // 应用的变体
};

// Blockstate解析结果
struct BlockstateData
{
    std::string blockName;       // 方块名称
    std::vector<BlockstateVariant> variants;  // 变体列表（variants格式）
    std::vector<BlockstateApply> multipart;   // 多部分列表（multipart格式）
    bool isMultipart = false;    // 是否为multipart格式
};

// Blockstate解析器：解析blockstate JSON文件
// 职责：读取blockstate文件、解析variants/multipart格式、建立方块名→变体映射
class BlockstateParser
{
public:
    // 解析单个blockstate文件
    static bool ParseFile(const std::string &filePath, BlockstateData &out);

    // 解析目录下所有blockstate文件
    static std::map<std::string, BlockstateData> ParseDirectory(const std::string &dirPath);

    // 根据方块属性查找匹配的变体
    static const BlockstateVariant* FindVariant(
        const BlockstateData &data,
        const std::map<std::string, std::string> &properties
    );
};
