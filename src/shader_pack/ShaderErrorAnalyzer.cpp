#include "ShaderErrorAnalyzer.h"
#include <sstream>
#include <regex>
#include <algorithm>

void ShaderErrorAnalyzer::AddError(const std::string &passName, const std::string &errorMessage)
{
    ErrorRecord record;
    record.passName = passName;
    record.errorMessage = errorMessage;
    record.type = ClassifyError(errorMessage);
    record.lineNumber = ExtractLineNumber(errorMessage);
    record.suggestion = GenerateSuggestion(record);

    m_Errors.push_back(record);

    // 统计错误频率
    m_ErrorFrequency[errorMessage]++;
}

void ShaderErrorAnalyzer::AnalyzeErrors()
{
    m_Stats = ErrorStats();
    m_Stats.totalErrors = m_Errors.size();

    for (const auto &error : m_Errors)
    {
        // 按类型统计
        switch (error.type)
        {
            case ErrorType::SyntaxError:    m_Stats.syntaxErrors++; break;
            case ErrorType::UndefinedVariable: m_Stats.undefinedVars++; break;
            case ErrorType::UndefinedFunction: m_Stats.undefinedFuncs++; break;
            case ErrorType::TypeMismatch:   m_Stats.typeErrors++; break;
            case ErrorType::TextureError:   m_Stats.textureErrors++; break;
            case ErrorType::VersionError:   m_Stats.versionErrors++; break;
            case ErrorType::Other:          m_Stats.otherErrors++; break;
        }

        // 按 Pass 统计
        m_Stats.passErrorCounts[error.passName]++;
    }
}

std::string ShaderErrorAnalyzer::GenerateReport() const
{
    std::ostringstream oss;

    oss << "\n";
    oss << "======================================================================\n";
    oss << "                    着色器错误分析报告\n";
    oss << "======================================================================\n\n";

    // 统计概览
    oss << "【统计概览】\n";
    oss << "  总错误数: " << m_Stats.totalErrors << "\n";
    oss << "  语法错误: " << m_Stats.syntaxErrors << "\n";
    oss << "  未定义变量: " << m_Stats.undefinedVars << "\n";
    oss << "  未定义函数: " << m_Stats.undefinedFuncs << "\n";
    oss << "  类型错误: " << m_Stats.typeErrors << "\n";
    oss << "  纹理错误: " << m_Stats.textureErrors << "\n";
    oss << "  版本错误: " << m_Stats.versionErrors << "\n";
    oss << "  其他错误: " << m_Stats.otherErrors << "\n\n";

    // 高频错误
    oss << "【高频错误 Top 10】\n";
    std::vector<std::pair<std::string, int>> sortedErrors(m_ErrorFrequency.begin(), m_ErrorFrequency.end());
    std::sort(sortedErrors.begin(), sortedErrors.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });

    int count = 0;
    for (const auto &[msg, freq] : sortedErrors)
    {
        if (count >= 10) break;
        oss << "  [" << freq << "次] " << msg << "\n";
        count++;
    }
    oss << "\n";

    // 按 Pass 分组的错误
    oss << "【按 Pass 分组的错误】\n";
    std::map<std::string, std::vector<const ErrorRecord*>> passErrors;
    for (const auto &error : m_Errors)
        passErrors[error.passName].push_back(&error);

    for (const auto &[pass, errors] : passErrors)
    {
        oss << "  " << pass << " (" << errors.size() << " 个错误):\n";
        for (const auto *error : errors)
        {
            oss << "    - " << error->errorMessage << "\n";
            if (!error->suggestion.empty())
                oss << "      建议: " << error->suggestion << "\n";
        }
    }
    oss << "\n";

    // 可自动修复的错误
    oss << "【可自动修复的错误】\n";
    auto fixable = GetAutoFixableErrors();
    if (fixable.empty())
    {
        oss << "  无\n";
    }
    else
    {
        for (const auto &error : fixable)
        {
            oss << "  " << error.passName << ": " << error.errorMessage << "\n";
            oss << "    修复: " << error.suggestion << "\n";
        }
    }
    oss << "\n";

    oss << "======================================================================\n";

    return oss.str();
}

std::vector<ShaderErrorAnalyzer::ErrorRecord> ShaderErrorAnalyzer::GetFrequentErrors(int threshold) const
{
    std::vector<ErrorRecord> result;
    for (const auto &error : m_Errors)
    {
        auto it = m_ErrorFrequency.find(error.errorMessage);
        if (it != m_ErrorFrequency.end() && it->second >= threshold)
            result.push_back(error);
    }
    return result;
}

std::vector<ShaderErrorAnalyzer::ErrorRecord> ShaderErrorAnalyzer::GetAutoFixableErrors() const
{
    std::vector<ErrorRecord> result;
    for (const auto &error : m_Errors)
    {
        if (IsAutoFixable(error))
            result.push_back(error);
    }
    return result;
}

void ShaderErrorAnalyzer::Clear()
{
    m_Errors.clear();
    m_Stats = ErrorStats();
    m_ErrorFrequency.clear();
}

ShaderErrorAnalyzer::ErrorType ShaderErrorAnalyzer::ClassifyError(const std::string &errorMessage) const
{
    // 语法错误
    if (errorMessage.find("syntax error") != std::string::npos ||
        errorMessage.find("unexpected") != std::string::npos ||
        errorMessage.find("expected") != std::string::npos)
        return ErrorType::SyntaxError;

    // 未定义变量
    if (errorMessage.find("undefined variable") != std::string::npos ||
        errorMessage.find("undeclared identifier") != std::string::npos)
        return ErrorType::UndefinedVariable;

    // 未定义函数
    if (errorMessage.find("undefined function") != std::string::npos ||
        errorMessage.find("no matching function") != std::string::npos)
        return ErrorType::UndefinedFunction;

    // 类型错误
    if (errorMessage.find("cannot convert") != std::string::npos ||
        errorMessage.find("type mismatch") != std::string::npos ||
        errorMessage.find("incompatible") != std::string::npos)
        return ErrorType::TypeMismatch;

    // 纹理错误
    if (errorMessage.find("sampler") != std::string::npos ||
        errorMessage.find("texture") != std::string::npos)
        return ErrorType::TextureError;

    // 版本错误
    if (errorMessage.find("version") != std::string::npos ||
        errorMessage.find("#version") != std::string::npos)
        return ErrorType::VersionError;

    return ErrorType::Other;
}

int ShaderErrorAnalyzer::ExtractLineNumber(const std::string &errorMessage) const
{
    // 尝试提取行号，格式如 "0(123) : error"
    std::regex lineRegex(R"(\d+\((\d+)\))");
    std::smatch match;
    if (std::regex_search(errorMessage, match, lineRegex))
        return std::stoi(match[1].str());
    return -1;
}

std::string ShaderErrorAnalyzer::GenerateSuggestion(const ErrorRecord &error) const
{
    switch (error.type)
    {
        case ErrorType::SyntaxError:
        {
            if (error.errorMessage.find("for") != std::string::npos)
                return "检查 for 循环语法，可能缺少括号或分号";
            if (error.errorMessage.find("if") != std::string::npos)
                return "检查 if 语句语法";
            return "检查代码语法，注意括号匹配和语句结束符";
        }

        case ErrorType::UndefinedVariable:
        {
            // 提取变量名
            std::regex varRegex(R"(undefined variable \"?(\w+)\"?)");
            std::smatch match;
            if (std::regex_search(error.errorMessage, match, varRegex))
            {
                std::string varName = match[1].str();
                return "添加变量声明: " + varName;
            }
            return "添加缺失的变量声明";
        }

        case ErrorType::UndefinedFunction:
        {
            std::regex funcRegex(R"(undefined function \"?(\w+)\"?)");
            std::smatch match;
            if (std::regex_search(error.errorMessage, match, funcRegex))
            {
                std::string funcName = match[1].str();
                return "添加函数实现: " + funcName;
            }
            return "添加缺失的函数实现";
        }

        case ErrorType::TypeMismatch:
            return "检查类型转换，确保赋值类型匹配";

        case ErrorType::TextureError:
            return "检查纹理采样器绑定和纹理单元配置";

        case ErrorType::VersionError:
            return "检查 GLSL 版本声明，确保与 OpenGL 版本兼容";

        default:
            return "";
    }
}

bool ShaderErrorAnalyzer::IsAutoFixable(const ErrorRecord &error) const
{
    // 未定义变量通常可以通过添加声明来修复
    if (error.type == ErrorType::UndefinedVariable)
        return true;

    // 某些未定义函数可以通过添加 stub 来修复
    if (error.type == ErrorType::UndefinedFunction)
    {
        // 检查是否是常见的 Iris 函数
        std::regex funcRegex(R"(undefined function \"?(\w+)\"?)");
        std::smatch match;
        if (std::regex_search(error.errorMessage, match, funcRegex))
        {
            std::string funcName = match[1].str();
            // 这些函数可以自动生成 stub
            static const std::set<std::string> autoFixableFuncs = {
                "clamp01", "max0", "min0", "GetSunVector", "GetMoonVector",
                "GetLightMapCoordinates", "screenToView", "viewToScreen"
            };
            return autoFixableFuncs.count(funcName) > 0;
        }
    }

    return false;
}
