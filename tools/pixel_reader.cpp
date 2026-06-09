// 独立图片像素阅读器 - 命令行工具
// 用法: pixel_reader.exe <图片路径> [--range x1 y1 x2 y2] [--output file.csv]
// 编译: g++ -std=c++17 -I. -o tools\pixel_reader.exe tools\pixel_reader.cpp -O2

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "../extern/stb/stb_image.h"

struct PixelColor
{
    unsigned char r, g, b, a;
};

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cout << "用法: pixel_reader.exe <图片路径> [选项]\n"
                  << "选项:\n"
                  << "  --range x1 y1 x2 y2   只输出指定区域 (左上角x1,y1 到 右下角x2,y2)\n"
                  << "  --output <file.csv>   输出到CSV文件\n"
                  << "  --info                只输出图片基本信息\n"
                  << "  --hex                 用十六进制输出颜色值\n"
                  << "示例:\n"
                  << "  pixel_reader.exe soul_lantern.png\n"
                  << "  pixel_reader.exe soul_lantern.png --range 0 0 16 16\n"
                  << "  pixel_reader.exe soul_lantern.png --output pixels.csv\n";
        return 0;
    }

    std::string filePath = argv[1];
    int rangeX1 = -1, rangeY1 = -1, rangeX2 = -1, rangeY2 = -1;
    std::string outputFile;
    bool infoOnly = false;
    bool hexMode = false;

    for (int i = 2; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--range" && i + 4 < argc)
        {
            rangeX1 = std::stoi(argv[++i]);
            rangeY1 = std::stoi(argv[++i]);
            rangeX2 = std::stoi(argv[++i]);
            rangeY2 = std::stoi(argv[++i]);
        }
        else if (arg == "--output" && i + 1 < argc)
        {
            outputFile = argv[++i];
        }
        else if (arg == "--info")
        {
            infoOnly = true;
        }
        else if (arg == "--hex")
        {
            hexMode = true;
        }
    }

    // 加载图片
    int width, height, channels;
    unsigned char *data = stbi_load(filePath.c_str(), &width, &height, &channels, 4);
    if (!data)
    {
        std::cerr << "错误: 无法加载图片 " << filePath << std::endl;
        return 1;
    }

    std::cout << "图片: " << filePath << std::endl;
    std::cout << "尺寸: " << width << " x " << height << std::endl;
    std::cout << "通道: " << channels << " (已转为RGBA)" << std::endl;

    if (infoOnly)
    {
        stbi_image_free(data);
        return 0;
    }

    // 确定输出范围
    int x1 = 0, y1 = 0, x2 = width, y2 = height;
    if (rangeX1 >= 0)
    {
        x1 = std::max(0, rangeX1);
        y1 = std::max(0, rangeY1);
        x2 = std::min(width, rangeX2);
        y2 = std::min(height, rangeY2);
        std::cout << "输出区域: (" << x1 << "," << y1 << ") -> (" << x2 << "," << y2 << ")" << std::endl;
    }

    // 构建像素二维数组
    std::vector<std::vector<PixelColor>> pixels(y2 - y1, std::vector<PixelColor>(x2 - x1));
    for (int y = y1; y < y2; y++)
    {
        for (int x = x1; x < x2; x++)
        {
            int idx = (y * width + x) * 4;
            pixels[y - y1][x - x1] = {data[idx], data[idx + 1], data[idx + 2], data[idx + 3]};
        }
    }

    stbi_image_free(data);

    // 输出到文件或控制台
    if (!outputFile.empty())
    {
        std::ofstream ofs(outputFile);
        ofs << "x,y,r,g,b,a" << std::endl;
        for (int y = 0; y < (int)pixels.size(); y++)
        {
            for (int x = 0; x < (int)pixels[y].size(); x++)
            {
                const auto &p = pixels[y][x];
                ofs << (x + x1) << "," << (y + y1) << ","
                    << (int)p.r << "," << (int)p.g << "," << (int)p.b << "," << (int)p.a << std::endl;
            }
        }
        std::cout << "已输出到: " << outputFile << " (" << pixels.size() * pixels[0].size() << " 像素)" << std::endl;
    }
    else
    {
        std::cout << "\n像素数据 (行优先 pixels[y][x]):" << std::endl;
        for (int y = 0; y < (int)pixels.size(); y++)
        {
            std::cout << "y=" << (y + y1) << ": ";
            for (int x = 0; x < (int)pixels[y].size(); x++)
            {
                const auto &p = pixels[y][x];
                if (hexMode)
                    printf("#%02X%02X%02X%02X ", p.r, p.g, p.b, p.a);
                else
                    printf("(%3d,%3d,%3d,%3d) ", p.r, p.g, p.b, p.a);
            }
            std::cout << std::endl;
        }
    }

    return 0;
}
