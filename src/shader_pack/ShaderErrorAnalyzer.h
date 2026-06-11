#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>

// 着色器错误分析器
// 收集、分类、分析所有编译错误，提供修复建议
class ShaderErrorAnalyzer
{
public:
    // 错误类型枚举
    enum class ErrorType
    {
        SyntaxError,        // 语法错误
        UndefinedVariable,  // 未定义变量
        UndefinedFunction,  // 未定义函数
        TypeMismatch,       // 类型不匹配
        TextureError,       // 纹理相关错误
        VersionError,       // 版本兼容错误
        Other               // 其他错误
    };

    // 单个错误记录
    struct ErrorRecord
    {
        std::string passName;       // Pass 名称
        std::string errorMessage;   // 错误信息
        ErrorType type;             // 错误类型
        int lineNumber;             // 行号（如果可用）
        std::string suggestion;     // 修复建议
    };

    // 错误统计
    struct ErrorStats
    {
        int totalErrors = 0;
        int syntaxErrors = 0;
        int undefinedVars = 0;
        int undefinedFuncs = 0;
        int typeErrors = 0;
        int textureErrors = 0;
        int versionErrors = 0;
        int otherErrors = 0;
        std::map<std::string, int> passErrorCounts;  // 每个 Pass 的错误数
    };

    // 添加错误记录
    void AddError(const std::string &passName, const std::string &errorMessage);

    // 分析所有错误
    void AnalyzeErrors();

    // 生成错误报告
    std::string GenerateReport() const;

    // 获取统计信息
    const ErrorStats& GetStats() const { return m_Stats; }

    // 获取所有错误记录
    const std::vector<ErrorRecord>& GetErrors() const { return m_Errors; }

    // 获取高频错误（出现次数 > threshold）
    std::vector<ErrorRecord> GetFrequentErrors(int threshold = 3) const;

    // 获取可自动修复的错误
    std::vector<ErrorRecord> GetAutoFixableErrors() const;

    // 清空所有记录
    void Clear();

    // 检查是否有错误
    bool HasErrors() const { return !m_Errors.empty(); }

private:
    // 分类错误类型
    ErrorType ClassifyError(const std::string &errorMessage) const;

    // 提取行号
    int ExtractLineNumber(const std::string &errorMessage) const;

    // 生成修复建议
    std::string GenerateSuggestion(const ErrorRecord &error) const;

    // 判断是否可自动修复
    bool IsAutoFixable(const ErrorRecord &error) const;

    std::vector<ErrorRecord> m_Errors;
    ErrorStats m_Stats;
    std::map<std::string, int> m_ErrorFrequency;  // 错误信息出现频率
};
