#include "k2d/Shader.h"
#include "k2d/FileBuffer.h"
#include "k2d/OpenGL.h"

#include <cstdio>
#include <cstdlib>

namespace k2d
{

static unsigned int CompileStage(unsigned int type, const char* src)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        int logLen = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 0)
        {
            char* log = static_cast<char*>(std::malloc(logLen));
            glGetShaderInfoLog(shader, logLen, nullptr, log);
            std::printf("Shader compile error: %s\n", log);
            std::free(log);
        }
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

Shader::Shader() : mProgram(0)
{
}

Shader::~Shader()
{
    Release();
}

void Shader::Release()
{
    if (mProgram)
    {
        glDeleteProgram(mProgram);
        mProgram = 0;
    }
}

bool Shader::CompileSource(const char* vsSrc, const char* fsSrc)
{
    unsigned int vs = CompileStage(GL_VERTEX_SHADER, vsSrc);
    if (!vs)
        return false;

    unsigned int fs = CompileStage(GL_FRAGMENT_SHADER, fsSrc);
    if (!fs)
    {
        glDeleteShader(vs);
        return false;
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    int ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        int logLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 0)
        {
            char* log = static_cast<char*>(std::malloc(logLen));
            glGetProgramInfoLog(program, logLen, nullptr, log);
            std::printf("Shader link error: %s\n", log);
            std::free(log);
        }
        glDeleteProgram(program);
        return false;
    }

    Release();
    mProgram = program;
    return true;
}

bool Shader::CompileFiles(const char* vsPath, const char* fsPath)
{
    FileBuffer vsBuf;
    FileBuffer fsBuf;

    if (!vsBuf.Load(vsPath, true))
        return false;

    if (!fsBuf.Load(fsPath, true))
        return false;

    return CompileSource(vsBuf.Text(), fsBuf.Text());
}

void Shader::Bind() const
{
    glUseProgram(mProgram);
}

void Shader::Unbind()
{
    glUseProgram(0);
}

int Shader::UniformLocation(const char* name) const
{
    return glGetUniformLocation(mProgram, name);
}

void Shader::SetInt(const char* name, int value)
{
    glUniform1i(UniformLocation(name), value);
}

void Shader::SetFloat(const char* name, float value)
{
    glUniform1f(UniformLocation(name), value);
}

void Shader::SetVec2(const char* name, float x, float y)
{
    glUniform2f(UniformLocation(name), x, y);
}

void Shader::SetVec3(const char* name, float x, float y, float z)
{
    glUniform3f(UniformLocation(name), x, y, z);
}

void Shader::SetVec4(const char* name, float x, float y, float z, float w)
{
    glUniform4f(UniformLocation(name), x, y, z, w);
}

void Shader::SetMat4(const char* name, const float* m16)
{
    glUniformMatrix4fv(UniformLocation(name), 1, GL_FALSE, m16);
}

} // namespace k2d
