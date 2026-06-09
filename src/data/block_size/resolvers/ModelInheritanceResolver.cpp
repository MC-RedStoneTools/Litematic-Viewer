#include "ModelInheritanceResolver.h"
#include "../../../utils/Log.h"
#include "../../../utils/JsonUtils.h"
#include "../data/BlockSizeState.h"

#include <nlohmann/json.hpp>

static LogSource gLog("Inheritance");
#include <filesystem>
#include <queue>

using json = nlohmann::json;
namespace fs = std::filesystem;

// 从方块名提取模型文件名
// "minecraft:lantern" → "lantern"
// "minecraft:block/lantern" → "lantern"
std::string ModelInheritanceResolver::ExtractModelName(const std::string &blockName)
{
    std::string name = blockName;

    // 去掉minecraft:前缀
    auto colonPos = name.find(':');
    if (colonPos != std::string::npos)
        name = name.substr(colonPos + 1);

    // 处理block/xxx格式
    auto slashPos = name.find('/');
    if (slashPos != std::string::npos)
        name = name.substr(slashPos + 1);

    return name;
}

// 递归加载模型文件及其所有父模型
void ModelInheritanceResolver::LoadModelWithParents(
    const std::string &modelName,
    const std::string &modelsDir,
    std::map<std::string, BlockSizeInfo> &loadedModels,
    std::set<std::string> &visited)
{
    // 已访问过，跳过
    if (visited.count(modelName)) return;
    visited.insert(modelName);

    // 构建文件路径
    std::string filePath = modelsDir + "\\" + modelName + ".json";
    if (!fs::exists(filePath))
    {
        gLog.Info("模型文件不存在: %s", filePath.c_str());
        return;
    }

    // 使用公共函数加载JSON
    auto jOpt = LoadJsonFile(filePath);
    if (!jOpt)
    {
        gLog.Error("JSON加载失败: %s", filePath.c_str());
        return;
    }
    json &j = *jOpt;

    // 创建BlockSizeInfo
    BlockSizeInfo info;
    info.blockName = modelName;  // 存储模型名

    // 解析parent（如果有）
    std::string parentModelName;
    if (j.contains("parent") && j["parent"].is_string())
    {
        std::string parentName = j["parent"].get<std::string>();
        // 递归加载父模型
        parentModelName = ExtractModelName(parentName);
        if (!parentModelName.empty() && parentModelName != modelName)
        {
            LoadModelWithParents(parentModelName, modelsDir, loadedModels, visited);
        }
    }

    // 解析elements
    BlockSizeParser::ParseElements(&j, info.elements);

    // 解析textures纹理变量映射
    if (j.contains("textures") && j["textures"].is_object())
    {
        for (auto &[key, val] : j["textures"].items())
        {
            if (val.is_string())
                info.textureVars["#" + key] = val.get<std::string>();
        }
    }

    // 确定状态
    info.state = DetermineState(modelName, info.elements);

    // 存储父模型名（用于继承链解析）
    if (!parentModelName.empty())
    {
        info.blockName = parentModelName;  // 临时存储父模型名
    }

    // 存储模型
    loadedModels[modelName] = std::move(info);

    // 调试：输出模型信息
    int facesWithUV = 0;
    int facesEnabled = 0;
    for (auto &elem : loadedModels[modelName].elements)
    {
        for (int i = 0; i < 6; i++)
        {
            if (elem.faceEnabled[i]) facesEnabled++;
            if (elem.faceUV[i].hasUV) facesWithUV++;
        }
    }
    gLog.Info("加载模型: %s, elements=%d, facesEnabled=%d, facesWithUV=%d",
        modelName.c_str(), (int)loadedModels[modelName].elements.size(), facesEnabled, facesWithUV);
}

// 拓扑排序后执行继承解析
void ModelInheritanceResolver::ResolveInheritanceChain(
    std::map<std::string, BlockSizeInfo> &models)
{
    // 构建依赖图：子模型 → 父模型
    std::map<std::string, std::string> parentMap;  // 子 → 父
    for (auto &[name, info] : models)
    {
        if (!info.blockName.empty() && info.blockName != name)
        {
            parentMap[name] = info.blockName;
        }
    }

    // 拓扑排序：确保父模型先被处理
    // 使用BFS（Kahn算法）
    std::map<std::string, int> inDegree;
    std::map<std::string, std::vector<std::string>> children;

    // 初始化
    for (auto &[name, info] : models)
    {
        inDegree[name] = 0;
    }

    // 构建子→父的依赖关系
    for (auto &[child, parent] : parentMap)
    {
        if (models.count(parent))
        {
            children[parent].push_back(child);
            inDegree[child]++;
        }
    }

    // BFS拓扑排序
    std::queue<std::string> queue;
    for (auto &[name, degree] : inDegree)
    {
        if (degree == 0) queue.push(name);
    }

    std::vector<std::string> sorted;
    while (!queue.empty())
    {
        std::string current = queue.front();
        queue.pop();
        sorted.push_back(current);

        for (auto &child : children[current])
        {
            inDegree[child]--;
            if (inDegree[child] == 0)
            {
                queue.push(child);
            }
        }
    }

    // 按拓扑序执行继承
    for (auto &name : sorted)
    {
        auto it = parentMap.find(name);
        if (it != parentMap.end())
        {
            auto childIt = models.find(name);
            auto parentIt = models.find(it->second);
            if (childIt != models.end() && parentIt != models.end())
            {
                // 如果子模型没有elements，从父模型继承
                if (childIt->second.elements.empty() && !parentIt->second.elements.empty())
                {
                    childIt->second.elements = parentIt->second.elements;
                    childIt->second.state = DetermineState(name, childIt->second.elements);
                    gLog.Info("%s -> %s, 继承 %d 个elements",
            name.c_str(), it->second.c_str(), (int)childIt->second.elements.size());

                    // 调试：输出继承后的faceUV信息
                    int facesWithUV = 0;
                    int facesEnabled = 0;
                    for (auto &elem : childIt->second.elements)
                    {
                        for (int i = 0; i < 6; i++)
                        {
                            if (elem.faceEnabled[i]) facesEnabled++;
                            if (elem.faceUV[i].hasUV)
                            {
                                facesWithUV++;
                                if (facesWithUV <= 5)
                                {
                                    gLog.Info("faceUV[%d]: hasUV=true, u1=%.1f, v1=%.1f, u2=%.1f, v2=%.1f",
                                        i, elem.faceUV[i].u1, elem.faceUV[i].v1, elem.faceUV[i].u2, elem.faceUV[i].v2);
                                }
                            }
                        }
                    }
                    gLog.Info("%s 继承后: facesEnabled=%d, facesWithUV=%d", name.c_str(), facesEnabled, facesWithUV);
                }

                // 继承父模型的纹理变量（子模型已有的优先）
                for (auto &[k, v] : parentIt->second.textureVars)
                {
                    if (childIt->second.textureVars.find(k) == childIt->second.textureVars.end())
                        childIt->second.textureVars[k] = v;
                }
            }
        }
    }
}

// 根据方块名称列表，加载并解析完整的模型继承链
std::map<std::string, BlockSizeInfo> ModelInheritanceResolver::Resolve(
    const std::vector<std::string> &blockNames,
    const std::string &modelsDir)
{
    std::map<std::string, BlockSizeInfo> loadedModels;
    std::set<std::string> visited;

    // 第一步：收集所有需要的模型名称
    std::set<std::string> neededModels;
    for (auto &blockName : blockNames)
    {
        std::string modelName = ExtractModelName(blockName);
        if (!modelName.empty())
        {
            neededModels.insert(modelName);
        }
    }

    gLog.Info("需要加载 %d 个模型", (int)neededModels.size());

    // 第二步：递归加载每个模型及其父模型
    for (auto &modelName : neededModels)
    {
        LoadModelWithParents(modelName, modelsDir, loadedModels, visited);
    }

    gLog.Info("共加载 %d 个模型文件", (int)loadedModels.size());

    // 第三步：拓扑排序后执行继承
    ResolveInheritanceChain(loadedModels);

    // 第四步：解析叶子模型的faceTexture #var引用链
    // 构建模板模型集合：被其他模型作为parent引用的模型
    // 模板模型的纹理变量由子模型提供，不需要单独解析
    std::set<std::string> templateModels;
    for (auto &[name, info] : loadedModels)
    {
        // blockName存储了父模型名（LoadModelWithParents中设置）
        // 如果blockName不等于自身名称，说明它是某个模型的子模型
        // 那么blockName指向的父模型就是模板
        if (!info.blockName.empty() && info.blockName != name)
        {
            templateModels.insert(info.blockName);
        }
    }

    for (auto &[name, info] : loadedModels)
    {
        // 跳过模板模型：它们的元素会被子模型继承，子模型会正确解析纹理变量
        if (templateModels.count(name)) continue;

        for (auto &elem : info.elements)
        {
            for (int f = 0; f < 6; f++)
            {
                std::string ref = elem.faceTexture[f];
                if (ref.empty()) continue;

                // 最多解析5层引用链
                for (int depth = 0; depth < 5; depth++)
                {
                    if (ref.empty() || ref[0] != '#') break;
                    auto it = info.textureVars.find(ref);
                    if (it == info.textureVars.end())
                    {
                        gLog.Info("纹理变量未找到: %s (模型: %s)", ref.c_str(), name.c_str());
                        break;
                    }
                    ref = it->second;
                }
                elem.faceTexture[f] = ref;
            }
        }
    }

    // 第五步：将结果转换为方块名格式（添加minecraft:前缀）
    std::map<std::string, BlockSizeInfo> result;
    for (auto &[modelName, info] : loadedModels)
    {
        // 存储原始模型名（不带前缀）
        result[modelName] = std::move(info);

        // 也存储带minecraft:前缀的版本
        std::string namespacedName = "minecraft:" + modelName;
        if (result.find(namespacedName) == result.end())
        {
            result[namespacedName] = result[modelName];
        }
    }

    gLog.Info("最终返回 %d 个模型映射", (int)result.size());
    return result;
}
