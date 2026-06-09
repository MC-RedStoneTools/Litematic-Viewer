# Litematica Preview

Minecraft Litematica 投影文件的 3D 预览工具，使用 C++20 和 OpenGL 实现。

## 功能

- 加载 `.litematic` 文件并解析 NBT 数据
- 支持 Minecraft 方块模型 JSON 解析和继承链解析
- 支持方块纹理映射（包括自定义 UV 坐标）
- 支持非完整方块渲染（楼梯、台阶、门等）
- 支持半透明方块渲染（传送门、水、玻璃等）
- 支持面剔除优化
- 支持鼠标旋转/缩放摄像机
- 支持配置文件自定义渲染选项

## 依赖

| 库 | 用途 |
|---|---|
| [GLFW 3.4](https://www.glfw.org/) | 窗口管理和 OpenGL 上下文 |
| [GLAD](https://glad.dav1d.de/) | OpenGL 函数加载器 |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON 解析 |
| [zlib](https://zlib.net/) | 数据解压 |
| [xxhash](https://github.com/Cyan4973/xxHash) | 哈希计算 |
| [stb_image](https://github.com/nothings/stb) | 图片加载 |
| [nbt_cpp](https://github.com/Spocky42/nbt_cpp) | NBT 数据解析 |

GLFW 通过 CMake FetchContent 自动下载，其他依赖包含在 `extern/` 目录中。

## 构建

### 环境要求

- CMake 3.14+
- 支持 C++20 的编译器（GCC 10+、Clang 12+、MSVC 2019+）

### 构建步骤

```bash
# 克隆仓库
git clone https://github.com/your-username/litematica-preview.git
cd litematica-preview

# 创建构建目录
mkdir build && cd build

# 配置
cmake ..

# 编译
cmake --build . --config Release
```

编译产物位于 `build/bin/LitematicaPreview.exe`。

### MinGW 构建

```bash
mkdir build_mingw && cd build_mingw
cmake -G "MinGW Makefiles" ..
cmake --build . --config Release
```

## 使用

```bash
# 加载投影文件
LitematicaPreview.exe <文件.litematic>

# 使用硬编码测试数据
LitematicaPreview.exe --test

# 无剔除模式
LitematicaPreview.exe --nocull

# 调试模式（面方向着色）
LitematicaPreview.exe --debug
```

### 操作方式

| 操作 | 功能 |
|------|------|
| 鼠标左键拖拽 | 旋转摄像机 |
| 鼠标滚轮 | 缩放 |
| ESC | 退出 |

### 配置文件

编辑 `config.json` 自定义渲染选项：

```json
{
    "剔除透明方块": false
}
```

## 项目结构

```
src/
├── main.cpp                    # 程序入口
├── app/                        # 应用程序生命周期
├── pipeline/                   # 数据处理流水线框架
├── stage/                      # 处理阶段（加载、网格生成）
├── data/                       # 数据层
│   ├── loader/                 # 文件加载器
│   ├── block_size/             # 方块尺寸系统
│   │   ├── parsers/            # JSON 解析器
│   │   ├── mappers/            # 尺寸→模型映射
│   │   └── resolvers/          # 模型继承链解析
│   └── BlockTypeClassifier.*   # 方块类型分类器
├── render/                     # 渲染层
│   ├── core/                   # 渲染器核心
│   ├── scene/                  # 场景（摄像机）
│   ├── resource/               # 资源（纹理）
│   └── platform/               # 平台（窗口、渲染循环）
└── utils/                      # 通用工具
models/                         # 方块模型 JSON 资源
```

## 渲染流程

```
.litematic 文件
    ↓ LoadStage
RegionData（方块数据）
    ↓ MeshStage
MeshData（顶点/三角形）
    ↓ Renderer
OpenGL 渲染
```

## 许可证

本项目使用 MIT 许可证，详见 [LICENSE](LICENSE)。
