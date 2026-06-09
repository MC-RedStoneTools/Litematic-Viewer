#pragma once

#include <glad/gl.h>

// OpenGL 资源 RAII 包装：自动管理生命周期
namespace GLResource
{
    // VAO 包装
    class VertexArray
    {
    public:
        VertexArray() { glGenVertexArrays(1, &m_Id); }
        ~VertexArray() { if (m_Id) glDeleteVertexArrays(1, &m_Id); }

        // 禁止复制，允许移动
        VertexArray(const VertexArray &) = delete;
        VertexArray &operator=(const VertexArray &) = delete;
        VertexArray(VertexArray &&other) noexcept : m_Id(other.m_Id) { other.m_Id = 0; }
        VertexArray &operator=(VertexArray &&other) noexcept
        {
            if (this != &other)
            {
                if (m_Id) glDeleteVertexArrays(1, &m_Id);
                m_Id = other.m_Id;
                other.m_Id = 0;
            }
            return *this;
        }

        GLuint Get() const { return m_Id; }
        operator GLuint() const { return m_Id; }

    private:
        GLuint m_Id = 0;
    };

    // VBO 包装
    class Buffer
    {
    public:
        Buffer() { glGenBuffers(1, &m_Id); }
        ~Buffer() { if (m_Id) glDeleteBuffers(1, &m_Id); }

        Buffer(const Buffer &) = delete;
        Buffer &operator=(const Buffer &) = delete;
        Buffer(Buffer &&other) noexcept : m_Id(other.m_Id) { other.m_Id = 0; }
        Buffer &operator=(Buffer &&other) noexcept
        {
            if (this != &other)
            {
                if (m_Id) glDeleteBuffers(1, &m_Id);
                m_Id = other.m_Id;
                other.m_Id = 0;
            }
            return *this;
        }

        GLuint Get() const { return m_Id; }
        operator GLuint() const { return m_Id; }

    private:
        GLuint m_Id = 0;
    };

    // Shader Program 包装
    class Program
    {
    public:
        Program() = default;
        ~Program() { if (m_Id) glDeleteProgram(m_Id); }

        Program(const Program &) = delete;
        Program &operator=(const Program &) = delete;
        Program(Program &&other) noexcept : m_Id(other.m_Id) { other.m_Id = 0; }
        Program &operator=(Program &&other) noexcept
        {
            if (this != &other)
            {
                if (m_Id) glDeleteProgram(m_Id);
                m_Id = other.m_Id;
                other.m_Id = 0;
            }
            return *this;
        }

        void Reset(GLuint id) { if (m_Id) glDeleteProgram(m_Id); m_Id = id; }
        GLuint Get() const { return m_Id; }
        operator GLuint() const { return m_Id; }

    private:
        GLuint m_Id = 0;
    };

    // Texture 包装
    class Texture
    {
    public:
        Texture() = default;
        ~Texture() { if (m_Id) glDeleteTextures(1, &m_Id); }

        Texture(const Texture &) = delete;
        Texture &operator=(const Texture &) = delete;
        Texture(Texture &&other) noexcept : m_Id(other.m_Id) { other.m_Id = 0; }
        Texture &operator=(Texture &&other) noexcept
        {
            if (this != &other)
            {
                if (m_Id) glDeleteTextures(1, &m_Id);
                m_Id = other.m_Id;
                other.m_Id = 0;
            }
            return *this;
        }

        void Reset(GLuint id) { if (m_Id) glDeleteTextures(1, &m_Id); m_Id = id; }
        GLuint Get() const { return m_Id; }
        operator GLuint() const { return m_Id; }

    private:
        GLuint m_Id = 0;
    };
}
