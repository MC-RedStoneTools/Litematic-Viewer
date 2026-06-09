#include "BlockSizeParser.h"
#include "../../../utils/Log.h"
#include "../../../utils/JsonUtils.h"
#include "../../../utils/FileUtils.h"

#include <nlohmann/json.hpp>

static LogSource gLog("BlockSize");
#include <iostream>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

// 面名称到索引的映射
static int GetFaceIndex(const std::string &faceName)
{
    if (faceName == "east")  return 0;
    if (faceName == "west")  return 1;
    if (faceName == "up")    return 2;
    if (faceName == "down")  return 3;
    if (faceName == "south") return 4;
    if (faceName == "north") return 5;
    return -1;
}

// 从JSON中解析elements数组
void BlockSizeParser::ParseElements(const void *jsonRoot, std::vector<SizeData> &elements)
{
    const json &j = *static_cast<const json*>(jsonRoot);

    if (!j.contains("elements") || !j["elements"].is_array())
        return;

    for (auto &elem : j["elements"])
    {
        SizeData sd;

        if (elem.contains("from") && elem["from"].is_array())
        {
            auto &arr = elem["from"];
            if (arr.size() >= 3)
            {
                sd.from[0] = arr[0].get<float>();
                sd.from[1] = arr[1].get<float>();
                sd.from[2] = arr[2].get<float>();
            }
        }

        if (elem.contains("to") && elem["to"].is_array())
        {
            auto &arr = elem["to"];
            if (arr.size() >= 3)
            {
                sd.to[0] = arr[0].get<float>();
                sd.to[1] = arr[1].get<float>();
                sd.to[2] = arr[2].get<float>();
            }
        }

        // 解析faces中的UV坐标、rotation和纹理引用
        if (elem.contains("faces") && elem["faces"].is_object())
        {
            for (auto &[faceName, faceData] : elem["faces"].items())
            {
                int faceIdx = GetFaceIndex(faceName);
                if (faceIdx < 0) continue;

                // 标记面已启用
                sd.faceEnabled[faceIdx] = true;

                // 解析纹理引用（如 "#side", "#top"）
                if (faceData.contains("texture") && faceData["texture"].is_string())
                {
                    sd.faceTexture[faceIdx] = faceData["texture"].get<std::string>();
                }

                // 解析自定义UV [u1, v1, u2, v2]
                if (faceData.contains("uv") && faceData["uv"].is_array())
                {
                    auto &uvArr = faceData["uv"];
                    if (uvArr.size() >= 4)
                    {
                        sd.faceUV[faceIdx].u1 = uvArr[0].get<float>();
                        sd.faceUV[faceIdx].v1 = uvArr[1].get<float>();
                        sd.faceUV[faceIdx].u2 = uvArr[2].get<float>();
                        sd.faceUV[faceIdx].v2 = uvArr[3].get<float>();
                        sd.faceUV[faceIdx].hasUV = true;
                    }
                }

                // 解析面UV旋转（0/90/180/270顺时针）
                if (faceData.contains("rotation") && faceData["rotation"].is_number())
                {
                    sd.faceUV[faceIdx].rotation = faceData["rotation"].get<int>();
                }
            }
        }

        // 解析Element级旋转
        if (elem.contains("rotation") && elem["rotation"].is_object())
        {
            auto &rot = elem["rotation"];
            if (rot.contains("origin") && rot["origin"].is_array())
            {
                auto &arr = rot["origin"];
                if (arr.size() >= 3)
                {
                    sd.rotation.origin[0] = arr[0].get<float>();
                    sd.rotation.origin[1] = arr[1].get<float>();
                    sd.rotation.origin[2] = arr[2].get<float>();
                }
            }
            if (rot.contains("axis") && rot["axis"].is_string())
            {
                std::string axisStr = rot["axis"].get<std::string>();
                if (!axisStr.empty()) sd.rotation.axis = axisStr[0];
            }
            if (rot.contains("angle") && rot["angle"].is_number())
            {
                sd.rotation.angle = rot["angle"].get<float>();
            }
            if (rot.contains("rescale") && rot["rescale"].is_boolean())
            {
                sd.rotation.rescale = rot["rescale"].get<bool>();
            }
            sd.rotation.hasRotation = true;
        }

        elements.push_back(sd);
    }
}

bool BlockSizeParser::ParseFile(const std::string &filePath, BlockSizeInfo &out)
{
    // 使用公共函数加载JSON
    auto jOpt = LoadJsonFile(filePath);
    if (!jOpt)
    {
        gLog.Warn("JSON加载失败: %s", filePath.c_str());
        return false;
    }
    json &j = *jOpt;

    // 读取parent
    if (j.contains("parent") && j["parent"].is_string())
    {
        // 暂存parent信息（用blockName字段临时存储）
        out.blockName = j["parent"].get<std::string>();
    }

    // 解析textures纹理变量映射
    if (j.contains("textures") && j["textures"].is_object())
    {
        for (auto &[key, val] : j["textures"].items())
        {
            if (val.is_string())
                out.textureVars["#" + key] = val.get<std::string>();
        }
    }

    // 解析elements
    ParseElements(&j, out.elements);

    // 确定状态
    out.state = DetermineState(out.blockName, out.elements);

    return true;
}
