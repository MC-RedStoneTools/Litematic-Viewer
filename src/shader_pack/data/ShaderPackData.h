#pragma once

#include <string>
#include <map>
#include <vector>

// 着色器 Pass 类型
enum class ShaderPassType
{
    Shadow,         // 阴影 Pass
    GBuffer,        // 几何缓冲 Pass（地形、实体等）
    Composite,      // 后处理 Pass
    Final,          // 最终输出 Pass
};

// 单个着色器 Pass 的源码
struct ShaderPassSource
{
    std::string vertexSource;       // 顶点着色器源码
    std::string fragmentSource;     // 片段着色器源码
    bool hasVertex = false;         // 是否有顶点着色器
    bool hasFragment = false;       // 是否有片段着色器
};

// 光影包配置（shaders.properties）
struct ShaderPackConfig
{
    int shadowMapResolution = 1024;     // 阴影贴图分辨率
    float shadowDistance = 128.0f;      // 阴影距离
    bool hasShadow = false;             // 是否有阴影
    bool hasComposite = false;          // 是否有后处理
    int noiseTextureResolution = 256;   // 噪声纹理分辨率
    std::map<std::string, std::string> customProperties;  // 自定义属性
};

// 光影包数据（加载后的原始数据）
struct ShaderPackData
{
    std::string packName;                           // 光影包名称
    ShaderPackConfig config;                        // 配置
    std::map<std::string, ShaderPassSource> passes; // Pass 名称 → 源码
    std::map<std::string, std::string> fileMap;     // 文件路径 → 内容（用于 #include 解析）

    // 获取指定 Pass
    const ShaderPassSource* GetPass(const std::string &name) const
    {
        auto it = passes.find(name);
        return (it != passes.end()) ? &it->second : nullptr;
    }
};
