#include "MiniZipReader.h"
#include <cstring>
#include <algorithm>
#include <zlib.h>

MiniZipReader::~MiniZipReader()
{
    Close();
}

// 打开 ZIP 文件并解析中央目录
bool MiniZipReader::Open(const std::string &path)
{
    Close();

    m_File = fopen(path.c_str(), "rb");
    if (!m_File)
    {
        // 尝试反斜杠路径
        std::string altPath = path;
        for (auto &c : altPath) if (c == '/') c = '\\';
        m_File = fopen(altPath.c_str(), "rb");
    }
    if (!m_File)
        return false;

    // 查找 EOCD（End of Central Directory）签名
    if (!FindEOCD())
    {
        Close();
        return false;
    }

    // 解析中央目录
    if (!ParseCentralDirectory())
    {
        Close();
        return false;
    }

    return true;
}

// 关闭文件
void MiniZipReader::Close()
{
    if (m_File)
    {
        fclose(m_File);
        m_File = nullptr;
    }
    m_Entries.clear();
}

// 读取指定条目的解压内容
bool MiniZipReader::ReadFile(const Entry &entry, std::vector<uint8_t> &outData)
{
    if (!m_File)
        return false;

    outData.resize(entry.uncompressedSize);

    // 定位到本地文件头
    fseek(m_File, static_cast<long>(entry.offset), SEEK_SET);

    // 读取本地文件头（30 字节固定头）
    uint8_t header[30];
    if (fread(header, 1, 30, m_File) != 30)
        return false;

    // 验证本地文件头签名
    if (header[0] != 0x50 || header[1] != 0x4B || header[2] != 0x03 || header[3] != 0x04)
        return false;

    // 获取文件名长度和额外字段长度
    uint16_t nameLen = header[26] | (header[27] << 8);
    uint16_t extraLen = header[28] | (header[29] << 8);

    // 跳过文件名和额外字段，到达数据区
    fseek(m_File, static_cast<long>(entry.offset + 30 + nameLen + extraLen), SEEK_SET);

    if (entry.compressionMethod == 0)
    {
        // 存储方式：直接读取
        if (fread(outData.data(), 1, entry.uncompressedSize, m_File) != entry.uncompressedSize)
            return false;
    }
    else if (entry.compressionMethod == 8)
    {
        // DEFLATE 方式：使用 zlib 解压
        // 读取压缩数据
        std::vector<uint8_t> compressedData(entry.compressedSize);
        if (fread(compressedData.data(), 1, entry.compressedSize, m_File) != entry.compressedSize)
            return false;
        return DecompressDeflate(entry, outData, compressedData);
    }
    else
    {
        return false; // 不支持的压缩方式
    }

    return true;
}

// 根据路径查找条目（自动处理路径分隔符）
const MiniZipReader::Entry *MiniZipReader::FindEntry(const std::string &path) const
{
    // 统一路径分隔符为正斜杠
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    for (const auto &e : m_Entries)
    {
        std::string entryName = e.name;
        std::replace(entryName.begin(), entryName.end(), '\\', '/');
        if (entryName == normalized)
            return &e;
    }
    return nullptr;
}

// 查找 EOCD 签名
bool MiniZipReader::FindEOCD()
{
    // EOCD 签名：50 4B 05 06
    // 从文件末尾向前搜索（最大搜索 65535 + 22 字节）
    fseek(m_File, 0, SEEK_END);
    long fileSize = ftell(m_File);
    fprintf(stderr, "[MiniZip] fileSize=%ld\n", fileSize);

    if (fileSize < 22)
        return false;

    // 最大搜索范围
    long searchStart = fileSize - 65557;
    if (searchStart < 0)
        searchStart = 0;

    // 从后向前搜索 EOCD 签名
    for (long pos = fileSize - 22; pos >= searchStart; pos--)
    {
        fseek(m_File, pos, SEEK_SET);
        uint8_t sig[4];
        if (fread(sig, 1, 4, m_File) != 4)
            return false;

        if (sig[0] == 0x50 && sig[1] == 0x4B && sig[2] == 0x05 && sig[3] == 0x06)
        {
            fprintf(stderr, "[MiniZip] EOCD found at offset=%ld\n", pos);
            // 读取 EOCD 记录
            // EOCD 结构: sig(4) + diskNum(2) + cdDisk(2) + entriesDisk(2) + entries(2) + cdSize(4) + cdOffset(4) + commentLen(2)
            uint8_t buf[4];

            // 中央目录大小 (offset +12)
            fseek(m_File, pos + 12, SEEK_SET);
            if (fread(buf, 1, 4, m_File) != 4) return false;
            m_CDSize = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);

            // 中央目录偏移 (offset +16)
            if (fread(buf, 1, 4, m_File) != 4) return false;
            m_CDOffset = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);

            fprintf(stderr, "[MiniZip] EOCD: CD offset=%u, CD size=%u\n", m_CDOffset, m_CDSize);
            return true;
        }
    }

    fprintf(stderr, "[MiniZip] EOCD not found!\n");
    return false;
}

// 解析中央目录
bool MiniZipReader::ParseCentralDirectory()
{
    fprintf(stderr, "[MiniZip] CD offset=%u, size=%u\n", m_CDOffset, m_CDSize);
    fseek(m_File, static_cast<long>(m_CDOffset), SEEK_SET);
    uint32_t pos = m_CDOffset;
    uint32_t endPos = m_CDOffset + m_CDSize;

    while (pos < endPos)
    {
        // 读取中央目录文件头签名
        uint8_t sig[4];
        if (fread(sig, 1, 4, m_File) != 4)
        {
            fprintf(stderr, "[MiniZip] failed to read sig at pos=%u\n", pos);
            break;
        }

        // 验证中央目录签名：50 4B 01 02
        if (sig[0] != 0x50 || sig[1] != 0x4B || sig[2] != 0x01 || sig[3] != 0x02)
        {
            fprintf(stderr, "[MiniZip] bad sig at pos=%u: %02X %02X %02X %02X\n",
                    pos, sig[0], sig[1], sig[2], sig[3]);
            break;
        }

        // 读取固定字段（42 字节头部，签名后的数据部分）
        // 布局: verMade(2) verNeed(2) flags(2) method(2) time(2) date(2)
        //       crc(4) compSize(4) uncompSize(4) nameLen(2) extraLen(2)
        //       commentLen(2) diskStart(2) intAttr(2) extAttr(4) offset(4)
        uint8_t header[42];
        if (fread(header, 1, 42, m_File) != 42)
        {
            fprintf(stderr, "[MiniZip] failed to read header at pos=%u\n", pos);
            break;
        }

        Entry entry;
        entry.compressionMethod = header[6] | (header[7] << 8);
        entry.compressedSize = header[16] | (header[17] << 8) | (header[18] << 16) | (header[19] << 24);
        entry.uncompressedSize = header[20] | (header[21] << 8) | (header[22] << 16) | (header[23] << 24);

        uint16_t nameLen = header[24] | (header[25] << 8);
        uint16_t extraLen = header[26] | (header[27] << 8);
        uint16_t commentLen = header[28] | (header[29] << 8);

        // 本地文件头偏移
        entry.offset = header[38] | (header[39] << 8) | (header[40] << 16) | (header[41] << 24);

        // 读取文件名
        std::vector<char> nameBuf(nameLen + 1, 0);
        if (fread(nameBuf.data(), 1, nameLen, m_File) != nameLen)
        {
            fprintf(stderr, "[MiniZip] failed to read name at pos=%u, nameLen=%u\n", pos, nameLen);
            break;
        }
        entry.name = std::string(nameBuf.data(), nameLen);

        // 跳过额外字段和注释
        fseek(m_File, static_cast<long>(extraLen + commentLen), SEEK_CUR);

        // 保存所有条目（由调用方负责过滤）
        m_Entries.push_back(std::move(entry));
        fprintf(stderr, "[MiniZip] entry: %s\n", entry.name.c_str());

        pos += 4 + 42 + nameLen + extraLen + commentLen;  // 4 = signature size, 42 = header size
    }

    fprintf(stderr, "[MiniZip] total entries: %zu\n", m_Entries.size());
    return true; // 中央目录解析成功（条目可能为空）
}

// 使用 zlib inflate 解压 DEFLATE 数据
bool MiniZipReader::DecompressDeflate(const Entry &entry, std::vector<uint8_t> &outData,
                                       const std::vector<uint8_t> &compressedData)
{
    // 初始化 zlib 流（raw deflate，windowBits = -15）
    z_stream stream;
    memset(&stream, 0, sizeof(stream));
    stream.next_in = const_cast<uint8_t*>(compressedData.data());
    stream.avail_in = entry.compressedSize;
    stream.next_out = outData.data();
    stream.avail_out = entry.uncompressedSize;

    // -15 表示 raw deflate（无 zlib 头）
    int ret = inflateInit2(&stream, -15);
    if (ret != Z_OK)
        return false;

    ret = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    return ret == Z_STREAM_END;
}
