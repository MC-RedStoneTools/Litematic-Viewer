#pragma once

#include <string>
#include <vector>

#include "BlockDecoder.h"

// 加载litematic文件，解码所有Region的方块数据
bool LoadLitematic(const std::string &filePath, std::vector<RegionData> &regions);
