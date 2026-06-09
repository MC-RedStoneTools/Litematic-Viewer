# 模型体积尺寸解析与展示分析

## 1. 尺寸解析流程

### 1.1 NBT结构解析

在 `src/data/loader/LitematicLoader.cpp` 中，从 `.litematic` 文件的 NBT 结构中提取尺寸：

```cpp
// 从NBT的Compound中读取Size
auto *pSize = region.HasCompound(MU8STR("Size"));
auto *sx = pSize->HasInt(MU8STR("x"));
auto *sy = pSize->HasInt(MU8STR("y"));
auto *sz = pSize->HasInt(MU8STR("z"));

// Size可能为负数，取绝对值作为实际尺寸
out.sizeX = *sx > 0 ? *sx : -*sx;
out.sizeY = *sy > 0 ? *sy : -*sy;
out.sizeZ = *sz > 0 ? *sz : -*sz;
```

### 1.2 数据存储结构

尺寸信息存储在 `RegionData` 结构体中（`src/data/loader/BlockDecoder.h`）：

```cpp
struct RegionData {
    int sizeX, sizeY, sizeZ;                        // 体积尺寸
    int offsetX = 0, offsetY = 0, offsetZ = 0;      // 位置偏移
    std::vector<BlockInfo> palette;                  // 方块调色板
    std::vector<uint16_t> blocks;                    // 方块索引数组
};
```

---

## 2. 尺寸展示方式

### 2.1 控制台日志输出

尺寸信息通过 `std::cout` 和 `Log()` 函数在控制台中输出：

**a) 文件加载时** — `src/data/loader/LitematicLoader.cpp:158-161`
```cpp
std::cout << "Region '" << name << "': "
          << rd.sizeX << "x" << rd.sizeY << "x" << rd.sizeZ
          << ", " << rd.palette.size() << "种方块, "
          << rd.blocks.size() << "个方块数据" << std::endl;
```

**b) 加载阶段处理时** — `src/stage/LoadStage.cpp:25`
```cpp
Log("Region尺寸: %dx%dx%d", rd.sizeX, rd.sizeY, rd.sizeZ);
```

**c) 网格生成阶段** — `src/stage/MeshStage.cpp:210-212`
```cpp
Log("Region %d: %dx%dx%d offset(%d,%d,%d)",
    (int)r, region.sizeX, region.sizeY, region.sizeZ,
    region.offsetX, region.offsetY, region.offsetZ);
```

### 2.2 渲染层面的间接运用

尺寸并非直接以UI文字展示，而是间接影响渲染：

- **包围盒中心计算** — `src/render/RenderLoop.cpp:96-120`：遍历所有 Region，用 `offset` 和 `size` 计算整体包围盒中心，作为相机观察目标点。
- **网格生成** — `src/stage/MeshStage.cpp:132-134`：用 `sizeX/sizeY/sizeZ` 作为三重循环的边界，遍历每个方块位置生成顶点。

---

## 3. 总结流程图

```
.litematic文件
    ↓
NBT解压解析
    ↓
读取Size(x,y,z) → RegionData存储
    ↓
控制台输出: "Region尺寸: 64x64x64"
控制台输出: "Region 'xxx': 64x64x64, N种方块"
    ↓
MeshStage: 用sizeX/Y/Z遍历生成网格
    ↓
RenderLoop: 用offset+size计算包围盒中心 → 相机目标
```

---

## 4. 当前状态

- **无GUI层面的尺寸展示**（如窗口标题栏或ImGui面板）
- 窗口标题固定为 `"Litematica Preview"`（`src/render/RenderLoop.cpp:127`）
- 尺寸信息仅通过控制台日志输出

---

## 5. 待办事项

- [ ] 在窗口标题栏显示模型尺寸信息
- [ ] 添加ImGui面板展示详细的尺寸和方块统计
- [ ] 支持多Region场景下的总尺寸计算和展示
- [ ] 将尺寸信息持久化到配置文件中
