#pragma once

#include "../data/BlockSizeState.h"
#include "../../MeshData.h"
#include <vector>
#include <map>

// 使用统一的Vertex结构体（定义在MeshData.h中）
using MeshVertex = Vertex;

// 单个面的顶点批次（记录该批顶点使用的纹理）
struct FaceBatch
{
    std::string textureName;          // 该面使用的纹理名（已解析）
    std::vector<MeshVertex> vertices; // 该面的顶点
};

// 单个方块的渲染数据
struct BlockRenderData
{
    std::vector<FaceBatch> faceBatches;  // 按面分组的顶点批次（每面可能不同纹理）
    std::vector<float> wireVertices;     // 线框顶点
    BlockSizeState state;                // 方块状态
};

// 尺寸映射组件：将BlockSizeInfo转换为可渲染的网格数据
// 职责：根据状态生成对应的顶点数据
class SizeToModelMapper
{
public:
    // 根据方块状态生成渲染数据
    static BlockRenderData GenerateRenderData(const BlockSizeInfo &sizeInfo, int x, int y, int z,
                                               int offsetX, int offsetY, int offsetZ);

    // 为完整方块生成渲染数据
    static BlockRenderData GenerateFullBlock(int x, int y, int z,
                                              int offsetX, int offsetY, int offsetZ);

    // 为特殊尺寸方块生成渲染数据
    static BlockRenderData GenerateSpecialSize(const BlockSizeInfo &sizeInfo, int x, int y, int z,
                                                int offsetX, int offsetY, int offsetZ);

    // 为非形状方块生成渲染数据（空或占位）
    static BlockRenderData GenerateNonShape(int x, int y, int z,
                                             int offsetX, int offsetY, int offsetZ);

    // 对所有顶点施加模型级旋转（绕方块中心8,8,8）
    static void ApplyModelRotation(BlockRenderData &data, float rotX, float rotY,
                                    int x, int y, int z, int offsetX, int offsetY, int offsetZ);

private:
    // 为单个element生成面顶点
    static void AddElementVertices(std::vector<MeshVertex> &vertices,
                                    int x, int y, int z, int faceIdx,
                                    int offsetX, int offsetY, int offsetZ,
                                    const SizeData &size);

    // 为单个element生成线框
    static void AddElementWireframe(std::vector<float> &wireVerts,
                                     int x, int y, int z, int faceIdx,
                                     int offsetX, int offsetY, int offsetZ,
                                     const SizeData &size);

    // 面颜色定义
    static constexpr float FACE_COLORS[6][3] = {
        {1.0f, 0.3f, 0.3f},  // +X 红
        {0.3f, 1.0f, 1.0f},  // -X 青
        {0.3f, 1.0f, 0.3f},  // +Y 绿
        {1.0f, 0.3f, 1.0f},  // -Y 品红
        {0.3f, 0.3f, 1.0f},  // +Z 蓝
        {1.0f, 1.0f, 0.3f},  // -Z 黄
    };
};
