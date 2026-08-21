#pragma once

#include "k2d/Shader.h"
#include "k2d/Texture.h"

#include <ct/hashmap.hpp>
#include <ct/string.hpp>

namespace k2d
{

    class Assets
    {
    public:
        Assets();
        ~Assets();

        Assets(const Assets &) = delete;
        Assets &operator=(const Assets &) = delete;

        Shader *LoadShader(const char *name, const char *vsSrc, const char *fsSrc);
        Shader *LoadShaderFiles(const char *name, const char *vsPath, const char *fsPath);
        Texture *CreateTexture(const char *name, int w, int h, const unsigned char *rgba,
                               bool nearest = true, bool repeat = false);
        Texture *LoadTexture(const char *name, const char *path);

        Shader *GetShader(const char *name);
        Texture *GetTexture(const char *name);

        void Clear();

    private:
        ct::HashMap<ct::String, Shader *> mShaders;
        ct::HashMap<ct::String, Texture *> mTextures;
    };

}
