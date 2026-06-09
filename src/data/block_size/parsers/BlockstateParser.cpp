#include "BlockstateParser.h"
#include "../../../utils/Log.h"
#include "../../../utils/JsonUtils.h"
#include "../../../utils/FileUtils.h"

#include <nlohmann/json.hpp>

static LogSource gLog("Blockstate");
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

// 解析单个变体对象（包含model/x/y/uvlock/weight）
static BlockstateVariant ParseVariant(const json &j)
{
    BlockstateVariant v;
    if (j.contains("model") && j["model"].is_string())
        v.model = j["model"].get<std::string>();
    if (j.contains("x") && j["x"].is_number())
        v.x = j["x"].get<float>();
    if (j.contains("y") && j["y"].is_number())
        v.y = j["y"].get<float>();
    if (j.contains("uvlock") && j["uvlock"].is_boolean())
        v.uvLock = j["uvlock"].get<bool>();
    if (j.contains("weight") && j["weight"].is_number())
        v.weight = j["weight"].get<int>();
    return v;
}

bool BlockstateParser::ParseFile(const std::string &filePath, BlockstateData &out)
{
    // 使用公共函数加载JSON
    auto jOpt = LoadJsonFile(filePath);
    if (!jOpt)
    {
        gLog.Warn("JSON加载失败: %s", filePath.c_str());
        return false;
    }
    json &j = *jOpt;

    // 解析variants格式
    if (j.contains("variants") && j["variants"].is_object())
    {
        out.isMultipart = false;
        // 统一处理单个变体或变体数组
        auto processVariant = [&](const std::string &key, const json &item) {
            if (item.is_object())
            {
                BlockstateVariant v = ParseVariant(item);
                v.key = key;
                out.variants.push_back(std::move(v));
            }
        };

        for (auto &[key, value] : j["variants"].items())
        {
            if (value.is_array())
                for (auto &item : value)
                    processVariant(key, item);
            else
                processVariant(key, value);
        }
    }
    // 解析multipart格式
    else if (j.contains("multipart") && j["multipart"].is_array())
    {
        out.isMultipart = true;
        for (auto &part : j["multipart"])
        {
            BlockstateApply apply;

            // 解析when条件
            if (part.contains("when") && part["when"].is_object())
            {
                for (auto &[propName, propValue] : part["when"].items())
                {
                    if (propValue.is_string())
                        apply.when.properties[propName] = propValue.get<std::string>();
                }
            }

            // 解析apply
            if (part.contains("apply") && part["apply"].is_object())
            {
                apply.apply = ParseVariant(part["apply"]);
            }

            out.multipart.push_back(std::move(apply));
        }
    }

    return true;
}

std::map<std::string, BlockstateData> BlockstateParser::ParseDirectory(const std::string &dirPath)
{
    std::map<std::string, BlockstateData> blockstates;

    if (!fs::exists(dirPath))
    {
        gLog.Error("Blockstate目录不存在: %s", dirPath.c_str());
        return blockstates;
    }

    int loaded = ForEachFileInDir(dirPath, ".json", [&](const std::string &filePath, const std::string &name) {
        BlockstateData data;
        data.blockName = name;
        if (ParseFile(filePath, data))
            blockstates[name] = std::move(data);
    });

    gLog.Info("解析了 %d 个blockstate文件", loaded);
    return blockstates;
}

const BlockstateVariant* BlockstateParser::FindVariant(
    const BlockstateData &data,
    const std::map<std::string, std::string> &properties)
{
    if (data.isMultipart)
    {
        // multipart格式：返回第一个匹配的apply
        for (auto &entry : data.multipart)
        {
            bool match = true;
            for (auto &[key, value] : entry.when.properties)
            {
                auto it = properties.find(key);
                if (it == properties.end() || it->second != value)
                {
                    match = false;
                    break;
                }
            }
            if (match)
                return &entry.apply;
        }
    }
    else
    {
        // variants格式：匹配属性字符串
        // 构建属性字符串，如 "facing=east,half=bottom,shape=straight"
        std::string propsStr;
        for (auto &[key, value] : properties)
        {
            if (!propsStr.empty()) propsStr += ",";
            propsStr += key + "=" + value;
        }

        // 查找匹配的变体
        for (auto &variant : data.variants)
        {
            if (variant.key == propsStr)
                return &variant;
        }
    }

    return nullptr;
}
