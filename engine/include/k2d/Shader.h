#pragma once

namespace k2d
{

    class Shader
    {
    public:
        Shader();
        ~Shader();

        Shader(const Shader &) = delete;
        Shader &operator=(const Shader &) = delete;

        bool CompileSource(const char *vsSrc, const char *fsSrc);
        bool CompileFiles(const char *vsPath, const char *fsPath);

        void Bind() const;
        static void Unbind();

        int UniformLocation(const char *name) const;
        void SetInt(const char *name, int value);
        void SetFloat(const char *name, float value);
        void SetVec2(const char *name, float x, float y);
        void SetVec3(const char *name, float x, float y, float z);
        void SetVec4(const char *name, float x, float y, float z, float w);
        void SetMat4(const char *name, const float *m16);

        unsigned int Id() const { return mProgram; }

        void Release();

    private:
        unsigned int mProgram;
    };

}
