#include "Shader.h"
#include "../../utils/Log.h"

static LogSource gLog("Shader");

bool Shader::Init(const std::string &vertexSrc, const std::string &fragmentSrc)
{
    // 编译顶点着色器和片段着色器
    GLuint vert = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint frag = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!vert || !frag) return false;

    // 创建程序并链接
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    // 检查链接是否成功
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char logBuf[512];
        glGetProgramInfoLog(program, 512, nullptr, logBuf);
        gLog.Error("着色器链接失败: %s", logBuf);
    }

    // 链接后可以删除着色器对象
    glDeleteShader(vert);
    glDeleteShader(frag);

    if (success)
        m_Program.Reset(program);
    else
        glDeleteProgram(program);

    return success != 0;
}

void Shader::Use() const
{
    glUseProgram(m_Program);
}

void Shader::SetMat4(const char *name, const float *value) const
{
    // 设置4x4矩阵uniform
    glUniformMatrix4fv(glGetUniformLocation(m_Program, name), 1, GL_FALSE, value);
}

GLuint Shader::CompileShader(GLenum type, const std::string &source)
{
    GLuint shader = glCreateShader(type);
    const char *src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    // 检查编译是否成功
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char logBuf[512];
        glGetShaderInfoLog(shader, 512, nullptr, logBuf);
        gLog.Error("着色器编译失败 (%s): %s", type == GL_VERTEX_SHADER ? "顶点" : "片段", logBuf);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
