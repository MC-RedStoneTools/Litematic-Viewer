#include "SizeToModelMapper.h"
#include "../../../utils/Log.h"

#include <cmath>

static LogSource gLog("SizeMapper");

// 默认完整方块尺寸
static const SizeData FULL_BLOCK_SIZE = {{0, 0, 0}, {16, 16, 16}};

// 对顶点施加Element旋转（绕origin按axis旋转angle度）
static void ApplyElementRotation(float &vx, float &vy, float &vz,
                                  const ElementRotation &rot)
{
    // 平移到旋转中心
    float dx = vx - rot.origin[0];
    float dy = vy - rot.origin[1];
    float dz = vz - rot.origin[2];

    float rad = rot.angle * 3.14159265358979f / 180.0f;
    float c = std::cos(rad), s = std::sin(rad);
    float nx, ny, nz;

    // 根据旋转轴应用旋转矩阵
    switch (rot.axis)
    {
    case 'x': // 绕X轴：Y和Z变化
        nx = dx;
        ny = dy * c - dz * s;
        nz = dy * s + dz * c;
        break;
    case 'y': // 绕Y轴：X和Z变化
        nx = dx * c + dz * s;
        ny = dy;
        nz = -dx * s + dz * c;
        break;
    case 'z': // 绕Z轴：X和Y变化
        nx = dx * c - dy * s;
        ny = dx * s + dy * c;
        nz = dz;
        break;
    default:
        nx = dx; ny = dy; nz = dz;
    }

    // rescale：按1/cos(angle)缩放（补偿旋转导致的尺寸损失）
    if (rot.rescale)
    {
        float scale = 1.0f / std::cos(rad);
        nx *= scale;
        ny *= scale;
        nz *= scale;
    }

    // 平移回原坐标系
    vx = nx + rot.origin[0];
    vy = ny + rot.origin[1];
    vz = nz + rot.origin[2];
}

// 为单个element生成面顶点
// 流程：计算顶点位置→施加Element旋转→FaceBakery公式计算UV→面UV旋转→自定义UV重映射
void SizeToModelMapper::AddElementVertices(std::vector<MeshVertex> &vertices,
                                            int x, int y, int z, int faceIdx,
                                            int offsetX, int offsetY, int offsetZ,
                                            const SizeData &size)
{
    // element坐标（Minecraft [0,16] 范围）
    float fx = size.from[0], fy = size.from[1], fz = size.from[2];
    float tx = size.to[0],   ty = size.to[1],   tz = size.to[2];

    // 6个面各4个顶点的原始位置（CCW绕序，从面外侧看）
    // 顶点顺序：左上→右上→右下→左下（在面的投影平面上）
    struct RawVert { float px, py, pz; };
    RawVert faceVerts[6][4] = {
        // +X (EAST): 面在tx处，4个角点
        {{tx,fy,tz}, {tx,fy,fz}, {tx,ty,fz}, {tx,ty,tz}},
        // -X (WEST): 面在fx处
        {{fx,fy,fz}, {fx,fy,tz}, {fx,ty,tz}, {fx,ty,fz}},
        // +Y (UP): 面在ty处
        {{fx,ty,tz}, {tx,ty,tz}, {tx,ty,fz}, {fx,ty,fz}},
        // -Y (DOWN): 面在fy处
        {{fx,fy,fz}, {tx,fy,fz}, {tx,fy,tz}, {fx,fy,tz}},
        // +Z (SOUTH): 面在tz处
        {{fx,fy,tz}, {tx,fy,tz}, {tx,ty,tz}, {fx,ty,tz}},
        // -Z (NORTH): 面在fz处
        {{tx,fy,fz}, {fx,fy,fz}, {fx,ty,fz}, {tx,ty,fz}},
    };

    // 施加Element旋转到当前面的4个顶点
    if (size.rotation.hasRotation)
    {
        for (int i = 0; i < 4; i++)
        {
            ApplyElementRotation(faceVerts[faceIdx][i].px,
                                 faceVerts[faceIdx][i].py,
                                 faceVerts[faceIdx][i].pz,
                                 size.rotation);
        }
    }

    // 用FaceBakery公式计算每个顶点的MC UV（基于旋转后的坐标）
    // 每个面的UV轴：
    //   EAST(+X): u=16-Z, v=16-Y
    //   WEST(-X): u=Z,   v=16-Y
    //   UP(+Y):   u=X,   v=Z
    //   DOWN(-Y): u=X,   v=16-Z
    //   SOUTH(+Z): u=X,  v=16-Y
    //   NORTH(-Z): u=16-X, v=16-Y
    struct VertUV { float mc_u, mc_v; };
    VertUV uv[4];
    for (int i = 0; i < 4; i++)
    {
        float vx = faceVerts[faceIdx][i].px;
        float vy = faceVerts[faceIdx][i].py;
        float vz = faceVerts[faceIdx][i].pz;

        switch (faceIdx)
        {
        case 0: uv[i] = {16.0f - vz, 16.0f - vy}; break;  // EAST
        case 1: uv[i] = {vz,          16.0f - vy}; break;  // WEST
        case 2: uv[i] = {vx,          vz};          break;  // UP
        case 3: uv[i] = {vx,          16.0f - vz}; break;  // DOWN
        case 4: uv[i] = {vx,          16.0f - vy}; break;  // SOUTH
        case 5: uv[i] = {16.0f - vx,  16.0f - vy}; break;  // NORTH
        }
    }

    // 面UV旋转：对UV值进行顺时针轮换置换
    // rotation=90 → shift=1，每个顶点取前一个顶点的UV
    int faceRot = size.faceUV[faceIdx].rotation;
    if (faceRot != 0)
    {
        int shift = ((faceRot / 90) % 4 + 4) % 4;
        VertUV saved[4] = {uv[0], uv[1], uv[2], uv[3]};
        for (int i = 0; i < 4; i++)
        {
            int src = (i - shift + 4) % 4;
            uv[i] = saved[src];
        }
    }

    // 自定义UV重映射：将自动UV范围线性映射到自定义UV矩形
    if (size.faceUV[faceIdx].hasUV)
    {
        float cu1 = size.faceUV[faceIdx].u1, cv1 = size.faceUV[faceIdx].v1;
        float cu2 = size.faceUV[faceIdx].u2, cv2 = size.faceUV[faceIdx].v2;

        // 计算当前面的自动UV范围
        float au1 = uv[0].mc_u, au2 = au1;
        float av1 = uv[0].mc_v, av2 = av1;
        for (int i = 1; i < 4; i++)
        {
            if (uv[i].mc_u < au1) au1 = uv[i].mc_u;
            if (uv[i].mc_u > au2) au2 = uv[i].mc_u;
            if (uv[i].mc_v < av1) av1 = uv[i].mc_v;
            if (uv[i].mc_v > av2) av2 = uv[i].mc_v;
        }

        // 线性重映射：auto范围 → custom范围
        float du = au2 - au1, dv = av2 - av1;
        for (int i = 0; i < 4; i++)
        {
            uv[i].mc_u = (du > 0) ? cu1 + (uv[i].mc_u - au1) / du * (cu2 - cu1) : cu1;
            uv[i].mc_v = (dv > 0) ? cv1 + (uv[i].mc_v - av1) / dv * (cv2 - cv1) : cv1;
        }
    }

    // 半纹素内缩：MC UV [u1,v1]-[u2,v2] 覆盖纹素 (u1,v1) 到 (u2-1,v2-1)
    // 直接用 mc_u/16 映射到纹素边界，GL_NEAREST可能在边界采到相邻透明纹素
    // 内缩0.5后映射到纹素中心，确保采样正确
    {
        float minU = uv[0].mc_u, maxU = uv[0].mc_u;
        float minV = uv[0].mc_v, maxV = uv[0].mc_v;
        for (int i = 1; i < 4; i++)
        {
            if (uv[i].mc_u < minU) minU = uv[i].mc_u;
            if (uv[i].mc_u > maxU) maxU = uv[i].mc_u;
            if (uv[i].mc_v < minV) minV = uv[i].mc_v;
            if (uv[i].mc_v > maxV) maxV = uv[i].mc_v;
        }
        float du = maxU - minU, dv = maxV - minV;
        // 将UV范围从 [min,max] 内缩到 [min+0.5, max-0.5]，映射到纹素中心
        if (du > 1.0f)
        {
            for (int i = 0; i < 4; i++)
                uv[i].mc_u = minU + 0.5f + (uv[i].mc_u - minU) / du * (du - 1.0f);
        }
        if (dv > 1.0f)
        {
            for (int i = 0; i < 4; i++)
                uv[i].mc_v = minV + 0.5f + (uv[i].mc_v - minV) / dv * (dv - 1.0f);
        }
    }

    // 发射两个三角形：(0,1,2) 和 (0,2,3)，CCW绕序
    float ox = (float)(offsetX + x);
    float oy = (float)(offsetY + y);
    float oz = (float)(offsetZ + z);
    static const int order[] = {0, 1, 2, 0, 2, 3};

    for (int i = 0; i < 6; i++)
    {
        int vi = order[i];
        MeshVertex vert;
        vert.x = ox + faceVerts[faceIdx][vi].px / 16.0f;
        vert.y = oy + faceVerts[faceIdx][vi].py / 16.0f;
        vert.z = oz + faceVerts[faceIdx][vi].pz / 16.0f;
        // MC v坐标：v=0在纹理顶部，v=16在纹理底部
        // OpenGL纹理：stb_image数据第一行=图像顶部，glTexImage2D第一行对应v=0
        // 因此v=0→图像顶部，与MC的v=0→顶部一致，无需翻转
        vert.u = uv[vi].mc_u / 16.0f;
        vert.v = uv[vi].mc_v / 16.0f;
        vert.r = FACE_COLORS[faceIdx][0];
        vert.g = FACE_COLORS[faceIdx][1];
        vert.b = FACE_COLORS[faceIdx][2];
        vertices.push_back(vert);
    }
}

// 为单个element生成线框（支持Element旋转）
void SizeToModelMapper::AddElementWireframe(std::vector<float> &wireVerts,
                                             int x, int y, int z, int faceIdx,
                                             int offsetX, int offsetY, int offsetZ,
                                             const SizeData &size)
{
    float fx = size.from[0], fy = size.from[1], fz = size.from[2];
    float tx = size.to[0],   ty = size.to[1],   tz = size.to[2];
    float cx = (float)(offsetX + x), cy = (float)(offsetY + y), cz = (float)(offsetZ + z);

    // 6个面的4个角点（MC坐标）
    float px[4], py[4], pz[4];
    switch (faceIdx)
    {
    case 0: px[0]=tx;py[0]=fy;pz[0]=tz; px[1]=tx;py[1]=ty;pz[1]=tz;
            px[2]=tx;py[2]=ty;pz[2]=fz; px[3]=tx;py[3]=fy;pz[3]=fz; break;
    case 1: px[0]=fx;py[0]=fy;pz[0]=fz; px[1]=fx;py[1]=fy;pz[1]=tz;
            px[2]=fx;py[2]=ty;pz[2]=tz; px[3]=fx;py[3]=ty;pz[3]=fz; break;
    case 2: px[0]=fx;py[0]=ty;pz[0]=tz; px[1]=tx;py[1]=ty;pz[1]=tz;
            px[2]=tx;py[2]=ty;pz[2]=fz; px[3]=fx;py[3]=ty;pz[3]=fz; break;
    case 3: px[0]=fx;py[0]=fy;pz[0]=fz; px[1]=tx;py[1]=fy;pz[1]=fz;
            px[2]=tx;py[2]=fy;pz[2]=tz; px[3]=fx;py[3]=fy;pz[3]=tz; break;
    case 4: px[0]=fx;py[0]=fy;pz[0]=tz; px[1]=tx;py[1]=fy;pz[1]=tz;
            px[2]=tx;py[2]=ty;pz[2]=tz; px[3]=fx;py[3]=ty;pz[3]=tz; break;
    case 5: px[0]=tx;py[0]=fy;pz[0]=fz; px[1]=fx;py[1]=fy;pz[1]=fz;
            px[2]=fx;py[2]=ty;pz[2]=fz; px[3]=tx;py[3]=ty;pz[3]=fz; break;
    default: return;
    }

    // 施加Element旋转
    if (size.rotation.hasRotation)
    {
        for (int i = 0; i < 4; i++)
            ApplyElementRotation(px[i], py[i], pz[i], size.rotation);
    }

    // 归一化到[0,1]并加上偏移
    for (int e = 0; e < 4; e++)
    {
        px[e] = cx + px[e] / 16.0f;
        py[e] = cy + py[e] / 16.0f;
        pz[e] = cz + pz[e] / 16.0f;
    }

    for (int e = 0; e < 4; e++)
    {
        int e2 = (e + 1) % 4;
        wireVerts.push_back(px[e]); wireVerts.push_back(py[e]); wireVerts.push_back(pz[e]);
        wireVerts.push_back(1.0f); wireVerts.push_back(1.0f); wireVerts.push_back(1.0f);
        wireVerts.push_back(px[e2]); wireVerts.push_back(py[e2]); wireVerts.push_back(pz[e2]);
        wireVerts.push_back(1.0f); wireVerts.push_back(1.0f); wireVerts.push_back(1.0f);
    }
}

// 为完整方块生成渲染数据
BlockRenderData SizeToModelMapper::GenerateFullBlock(int x, int y, int z,
                                                      int offsetX, int offsetY, int offsetZ)
{
    BlockRenderData data;
    data.state = BlockSizeState::FullBlock;

    for (int f = 0; f < 6; f++)
    {
        FaceBatch batch;
        batch.textureName = "";  // 完整方块没有faceTexture，由外部设置默认纹理
        AddElementVertices(batch.vertices, x, y, z, f, offsetX, offsetY, offsetZ, FULL_BLOCK_SIZE);
        data.faceBatches.push_back(std::move(batch));
        AddElementWireframe(data.wireVertices, x, y, z, f, offsetX, offsetY, offsetZ, FULL_BLOCK_SIZE);
    }

    return data;
}

// 为特殊尺寸方块生成渲染数据
BlockRenderData SizeToModelMapper::GenerateSpecialSize(const BlockSizeInfo &sizeInfo, int x, int y, int z,
                                                        int offsetX, int offsetY, int offsetZ)
{
    BlockRenderData data;
    data.state = BlockSizeState::SpecialSize;

    for (size_t ei = 0; ei < sizeInfo.elements.size(); ei++)
    {
        auto &elem = sizeInfo.elements[ei];

        for (int f = 0; f < 6; f++)
        {
            // 只为启用的面生成顶点
            if (!elem.faceEnabled[f]) continue;

            // 使用FaceBatch分组，每个面一个批次
            FaceBatch batch;
            batch.textureName = elem.faceTexture[f].empty() ? "" : elem.faceTexture[f];
            AddElementVertices(batch.vertices, x, y, z, f, offsetX, offsetY, offsetZ, elem);
            data.faceBatches.push_back(std::move(batch));
            AddElementWireframe(data.wireVertices, x, y, z, f, offsetX, offsetY, offsetZ, elem);
        }
    }

    return data;
}

// 为非形状方块生成渲染数据
BlockRenderData SizeToModelMapper::GenerateNonShape(int x, int y, int z,
                                                     int offsetX, int offsetY, int offsetZ)
{
    BlockRenderData data;
    data.state = BlockSizeState::NonShape;
    // 非形状方块不生成任何顶点
    return data;
}

// 对所有顶点施加模型级旋转（绕方块中心 [x+0.5, y+0.5, z+0.5]）
// rotX: 绕X轴旋转角度（度），rotY: 绕Y轴旋转角度（度）
void SizeToModelMapper::ApplyModelRotation(BlockRenderData &data, float rotX, float rotY,
                                            int x, int y, int z,
                                            int offsetX, int offsetY, int offsetZ)
{
    if (rotX == 0 && rotY == 0) return;

    // 旋转中心：方块的世界坐标中心
    float cx = (float)(offsetX + x) + 0.5f;
    float cy = (float)(offsetY + y) + 0.5f;
    float cz = (float)(offsetZ + z) + 0.5f;

    float radX = rotX * 3.14159265358979f / 180.0f;
    float radY = rotY * 3.14159265358979f / 180.0f;
    float cosX = std::cos(radX), sinX = std::sin(radX);
    float cosY = std::cos(radY), sinY = std::sin(radY);

    // 对所有FaceBatch中的顶点施加旋转
    for (auto &batch : data.faceBatches)
    for (auto &v : batch.vertices)
    {
        float dx = v.x - cx;
        float dy = v.y - cy;
        float dz = v.z - cz;

        // 先绕X轴旋转（Y和Z变化）
        float ny = dy * cosX - dz * sinX;
        float nz = dy * sinX + dz * cosX;
        dy = ny; dz = nz;

        // 再绕Y轴旋转（X和Z变化）
        float nx = dx * cosY + dz * sinY;
        nz = -dx * sinY + dz * cosY;

        v.x = nx + cx;
        v.y = dy + cy;
        v.z = nz + cz;
    }

    // 对线框顶点施加旋转
    for (size_t i = 0; i < data.wireVertices.size(); i += 6)
    {
        float dx = data.wireVertices[i] - cx;
        float dy = data.wireVertices[i + 1] - cy;
        float dz = data.wireVertices[i + 2] - cz;

        float ny = dy * cosX - dz * sinX;
        float nz = dy * sinX + dz * cosX;
        dy = ny; dz = nz;

        float nx = dx * cosY + dz * sinY;
        nz = -dx * sinY + dz * cosY;

        data.wireVertices[i] = nx + cx;
        data.wireVertices[i + 1] = dy + cy;
        data.wireVertices[i + 2] = nz + cz;
    }
}

// 根据方块状态生成渲染数据
BlockRenderData SizeToModelMapper::GenerateRenderData(const BlockSizeInfo &sizeInfo, int x, int y, int z,
                                                       int offsetX, int offsetY, int offsetZ)
{
    switch (sizeInfo.state)
    {
    case BlockSizeState::FullBlock:
        return GenerateFullBlock(x, y, z, offsetX, offsetY, offsetZ);

    case BlockSizeState::SpecialSize:
        return GenerateSpecialSize(sizeInfo, x, y, z, offsetX, offsetY, offsetZ);

    case BlockSizeState::NonShape:
        return GenerateNonShape(x, y, z, offsetX, offsetY, offsetZ);

    default:
        return GenerateFullBlock(x, y, z, offsetX, offsetY, offsetZ);
    }
}
