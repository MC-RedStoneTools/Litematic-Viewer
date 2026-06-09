#pragma once

#include <vector>
#include <string>
#include <cstdint>

// === 顶点格式定义 ===

// 三角形顶点：位置(3) + UV(2) + 颜色(3) = 8个float
struct Vertex
{
    float x, y, z;      // 位置
    float u, v;         // 纹理坐标
    float r, g, b;      // 颜色（调试用）
};

// 线框顶点：位置(3) + 颜色(3) = 6个float
struct WireVertex
{
    float x, y, z;      // 位置
    float r, g, b;      // 颜色
};

// 顶点格式常量
constexpr int VERTEX_FLOAT_COUNT = sizeof(Vertex) / sizeof(float);           // 8
constexpr int WIRE_VERTEX_FLOAT_COUNT = sizeof(WireVertex) / sizeof(float);  // 6

// 方块绘制命令：记录每种纹理的顶点范围（纯数据，不含渲染策略）
struct BlockDrawCall
{
    std::string textureName;  // 纹理名（用于查找纹理，空表示使用方块默认纹理）
    int blockIndex;           // 方块在palette中的索引（回退时使用）
    int firstVertex;          // 第一个顶点索引
    int vertexCount;          // 顶点数量
    bool isTransparent = false; // 是否半透明（决定渲染pass）
};

// 网格数据：MeshStage生成，Renderer消费
struct MeshData
{
    std::vector<float> vertices;               // 三角形顶点数据
    std::vector<BlockDrawCall> drawCalls;       // 按方块类型分组的绘制命令
    std::vector<std::string> blockNames;        // palette索引→方块名

    std::vector<float> wireVertices;            // 线框顶点数据

    int GetVertexCount() const { return static_cast<int>(vertices.size()) / VERTEX_FLOAT_COUNT; }
    int GetTriangleCount() const { return GetVertexCount() / 3; }
    int GetWireVertexCount() const { return static_cast<int>(wireVertices.size()) / WIRE_VERTEX_FLOAT_COUNT; }
    bool IsEmpty() const { return vertices.empty(); }
};
