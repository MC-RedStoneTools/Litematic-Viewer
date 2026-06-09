#include "Pipeline.h"

// 阶段注册表（静态存储）
StageDesc Pipeline::s_Table[static_cast<int>(StageType::Count)] = {};
