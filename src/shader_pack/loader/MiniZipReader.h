#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>

// 轻量级 ZIP 读取器：基于 zlib inflate，仅支持读取
class MiniZipReader
{
public:
    // ZIP 文件条目信息
    struct Entry
    {
        std::string name;           // 文件路径
        uint32_t compressedSize;    // 压缩后大小
        uint32_t uncompressedSize;  // 原始大小
        uint32_t offset;            // 本地文件头偏移
        uint16_t compressionMethod; // 压缩方式（0=存储, 8=DEFLATE）
    };

    MiniZipReader() = default;
    ~MiniZipReader();

    // 禁止复制
    MiniZipReader(const MiniZipReader &) = delete;
    MiniZipReader &operator=(const MiniZipReader &) = delete;

    // 打开 ZIP 文件并解析中央目录
    bool Open(const std::string &path);

    // 关闭文件
    void Close();

    // 获取所有条目
    const std::vector<Entry> &GetEntries() const { return m_Entries; }

    // 读取指定条目的解压内容
    bool ReadFile(const Entry &entry, std::vector<uint8_t> &outData);

    // 根据路径查找条目（自动处理路径分隔符）
    const Entry *FindEntry(const std::string &path) const;

    // 是否已打开
    bool IsOpen() const { return m_File != nullptr; }

private:
    FILE *m_File = nullptr;
    std::vector<Entry> m_Entries;
    uint32_t m_CDOffset = 0;    // 中央目录偏移
    uint32_t m_CDSize = 0;      // 中央目录大小

    // 查找 EOCD 签名
    bool FindEOCD();

    // 解析中央目录
    bool ParseCentralDirectory();

    // 使用 zlib inflate 解压 DEFLATE 数据
    static bool DecompressDeflate(const Entry &entry, std::vector<uint8_t> &outData,
                                   const std::vector<uint8_t> &compressedData);
};
