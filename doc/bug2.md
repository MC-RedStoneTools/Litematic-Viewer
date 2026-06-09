# Bug 2: 逐方块绘制时同种方块顶点交错导致纹理错误

## 问题描述
正常纹理模式下，地狱门方块显示为黑曜石纹理，红石面(+X方向)的方块纹理也显示不正确。面方向调试模式(按键1)下红色面看不到任何内容。

## 根本原因
`CubeRenderer::Init()` 和 `CubeRendererNoCull::Init()` 中使用 `drawStart[blockIdx]` 和 `drawCount[blockIdx]` 构建绘制命令，但顶点数据是按 x,y,z 遍历顺序写入的，不同方块类型的顶点在 VBO 中交错分布。

`glDrawArrays(GL_TRIANGLES, drawStart, drawCount)` 绘制的是**连续**的顶点范围，但同种方块的顶点并不连续，导致绘制范围覆盖了其他方块类型的顶点。

### 错误逻辑示意
```
VBO中顶点顺序（按x,y,z遍历）：
  obsidian(0,0,0), obsidian(1,0,0), ..., obsidian(0,1,0),
  nether_portal(1,1,0), nether_portal(2,1,0),   ← 夹在中间
  obsidian(3,1,0), ...

drawStart[obsidian] = 0
drawCount[obsidian] = 所有obsidian顶点数（包括后面交错的）

glDrawArrays(GL_TRIANGLES, 0, drawCount[obsidian])
→ 绘制范围从顶点0到总obsidian数，中间包含了nether_portal的顶点
→ nether_portal顶点被错误绑定为黑曜石纹理
```

### 错误代码
```cpp
// drawStart只记录第一个出现位置
if (drawStart.find(blockIdx) == drawStart.end())
{
    drawStart[blockIdx] = vertIdx;
    drawCount[blockIdx] = 0;
}
// drawCount累加所有同类方块的顶点数
drawCount[blockIdx] += 6;
```

## 影响
- 同种方块类型的顶点在VBO中不连续，但绘制命令假设连续
- 导致纹理绑定错误：地狱门方块显示黑曜石纹理
- 面方向调试模式下部分面无法显示

## 修复方案
使用按方块类型分离的顶点数组，确保每种方块的顶点在VBO中连续存储：

```cpp
// 按方块类型分离顶点数据
std::map<int, std::vector<float>> perBlockVerts;

for (x, y, z) {
    perBlockVerts[blockIdx].push_back(...);
}

// 合并时确保同种方块连续
std::vector<float> vertices;
for (auto &[idx, verts] : perBlockVerts) {
    int firstVert = vertices.size() / 8;
    vertices.insert(vertices.end(), verts.begin(), verts.end());
    m_DrawCalls.push_back({idx, firstVert, (int)verts.size() / 8});
}
```

## 涉及文件
- `src/render/CubeRenderer.cpp` — Init()方法，修复drawStart/drawCount逻辑
- `src/render/CubeRendererNoCull.cpp` — Init()方法，同样的修复

## 状态
已修复
