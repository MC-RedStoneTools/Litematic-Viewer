#pragma once

#include <string>
#include <map>

// 着色器预处理器：将 GLSL 120/130 语法转换为 330+
class ShaderPreprocessor
{
public:
    // 处理着色器源码（isVertex: true=顶点着色器, false=片段着色器）
    // passName: pass 名称，用于注入对应的阶段宏（如 gbuffers_terrain → GBUFFERS_TERRAIN）
    static std::string Process(const std::string &source, bool isVertex,
                                const std::map<std::string, std::string> &fileMap = {},
                                const std::string &passName = "");

private:
    // 递归展开 #include 指令
    static std::string ResolveIncludes(const std::string &source,
                                        const std::map<std::string, std::string> &fileMap,
                                        int depth);

    // 添加 #version 声明（如果没有）
    static std::string AddVersionHeader(const std::string &source);

    // 替换关键字：attribute → in, varying → out/in
    static std::string ReplaceKeywords(const std::string &source, bool isVertex);

    // 替换纹理函数：texture2D → texture
    static std::string ReplaceTextureFunctions(const std::string &source);

    // 替换片段输出：gl_FragData[N] → out 变量
    static std::string ReplaceFragDataOutput(const std::string &source);

    // 添加自定义宏定义
    static std::string AddCustomDefines(const std::string &source, bool isVertex,
                                         const std::string &passName = "");

    // 添加 Iris 标准 Uniform 声明（插入到全局作用域）
    static std::string AddIrisUniforms(const std::string &source, bool isVertex);

    // 规范化路径（统一斜杠、去掉前导 /）
    static std::string NormalizePath(const std::string &path);

    // 查找全局作用域的插入点（在 #version 和所有 #pragma/extension 之后，第一个函数/变量声明之前）
    static size_t FindGlobalInsertPoint(const std::string &source);
};
