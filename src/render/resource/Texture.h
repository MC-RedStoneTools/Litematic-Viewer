#pragma once

#include <string>
#include <map>
#include <glad/gl.h>

// 纹理管理类：加载方块纹理图片并缓存
class TextureManager
{
public:
    // 加载纹理图片目录（扫描目录下所有png）
    bool LoadTexturesFromDir(const std::string &dirPath);

    // 获取方块纹理ID，找不到返回0
    GLuint GetBlockTexture(const std::string &blockName) const;

    // 清理所有纹理
    void Destroy();

private:
    // 方块名 → OpenGL纹理ID
    std::map<std::string, GLuint> m_Textures;

    // 从文件加载单个纹理
    GLuint LoadTexture(const std::string &filePath);
};
