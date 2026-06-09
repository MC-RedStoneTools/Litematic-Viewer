# Bug 1: 硬编码测试数据索引公式错误

## 问题描述
硬编码地狱门测试结构时，渲染显示为S型扭曲形状，而非正确的门型框架。

## 根本原因
`test_regions.h` 中 `SetBlock` 使用的索引公式与 `RegionData::GetBlockIndex` 不一致。

### 错误代码
```cpp
// test_regions.h 中的 SetBlock
rd.blocks[y + z * rd.sizeY + x * rd.sizeY * rd.sizeZ] = paletteIdx;
```

### 正确代码
```cpp
// RegionData::GetBlockIndex 中的公式
blocks[x + z * sizeX + y * sizeX * sizeZ]
```

## 影响
- 方块坐标映射错误，导致渲染结构扭曲
- 数据写入和读取使用不同的索引公式

## 修复方案
将 `test_regions.h` 中的索引公式改为与 `RegionData::GetBlockIndex` 一致：

```cpp
// 修复后的 SetBlock
rd.blocks[x + z * rd.sizeX + y * rd.sizeX * rd.sizeZ] = paletteIdx;
```

## 涉及文件
- `src/test_regions.h`
- `src/BlockDecoder.h` (RegionData::GetBlockIndex)

## 状态
已修复
