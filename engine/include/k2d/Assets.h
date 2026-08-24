#pragma once

#include "k2d/Shader.h"
#include "k2d/Texture.h"

#include <ct/hashmap.hpp>
#include <ct/string.hpp>

#include <cstddef>

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
        Texture *LoadTexture(const char *name, const char *path,
                            bool nearest = true, bool repeat = false);
        Texture *LoadTextureMemory(const char *name, const unsigned char *data, std::size_t size,
                                   bool nearest = true, bool repeat = false);

        Shader *GetShader(const char *name);
        Texture *GetTexture(const char *name);

        const char *FindTextureName(const Texture *texture) const;

        void Clear();

        // Preferred camelCase API. PascalCase names above remain compatible.
        Shader *loadShader(const char *name, const char *vsSrc, const char *fsSrc) { return LoadShader(name, vsSrc, fsSrc); }
        Shader *loadShaderFiles(const char *name, const char *vsPath, const char *fsPath) { return LoadShaderFiles(name, vsPath, fsPath); }
        Texture *createTexture(const char *name, int w, int h, const unsigned char *rgba,
                               bool nearest = true, bool repeat = false)
        { return CreateTexture(name, w, h, rgba, nearest, repeat); }
        Texture *loadTexture(const char *name, const char *path, bool nearest = true, bool repeat = false)
        { return LoadTexture(name, path, nearest, repeat); }
        Texture *loadTextureMemory(const char *name, const unsigned char *data, std::size_t size,
                                   bool nearest = true, bool repeat = false)
        { return LoadTextureMemory(name, data, size, nearest, repeat); }
        Shader *shader(const char *name) { return GetShader(name); }
        Texture *texture(const char *name) { return GetTexture(name); }
        const char *findTextureName(const Texture *texture) const { return FindTextureName(texture); }
        void clear() { Clear(); }

    private:
        ct::HashMap<ct::String, Shader *> mShaders;
        ct::HashMap<ct::String, Texture *> mTextures;
    };

}
