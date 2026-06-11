#pragma once

#include <filesystem>
#include <string>

namespace PathUtils
{

inline std::string GetExeDir(const char *argv0)
{
    return std::filesystem::path(argv0).parent_path().string();
}

inline std::string Join(const std::string &base, std::initializer_list<const char *> parts)
{
    std::filesystem::path p(base);
    for (const char *part : parts)
        p /= part;
    return p.string();
}

} // namespace PathUtils
