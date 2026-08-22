#include "k2d/Assets.h"

#include "k2d/FileSystem.h"

namespace k2d
{

    Assets::Assets()
    {
    }

    Assets::~Assets()
    {
        Clear();
    }

    Shader *Assets::LoadShader(const char *name, const char *vsSrc, const char *fsSrc)
    {
        Shader *shader = new Shader();
        if (!shader->CompileSource(vsSrc, fsSrc))
        {
            delete shader;
            return nullptr;
        }

        ct::String key(name);
        Shader **existing = mShaders.find(key);
        if (existing)
        {
            delete *existing;
        }
        mShaders.put(key, shader);
        return shader;
    }

    Shader *Assets::LoadShaderFiles(const char *name, const char *vsPath, const char *fsPath)
    {
        ct::String vsResolved;
        if (!FileSystem::Instance().Resolve(vsPath, vsResolved))
            vsResolved = vsPath;

        ct::String fsResolved;
        if (!FileSystem::Instance().Resolve(fsPath, fsResolved))
            fsResolved = fsPath;

        Shader *shader = new Shader();
        if (!shader->CompileFiles(vsResolved.c_str(), fsResolved.c_str()))
        {
            delete shader;
            return nullptr;
        }

        ct::String key(name);
        Shader **existing = mShaders.find(key);
        if (existing)
        {
            delete *existing;
        }
        mShaders.put(key, shader);
        return shader;
    }

    Texture *Assets::CreateTexture(const char *name, int w, int h, const unsigned char *rgba,
                                   bool nearest, bool repeat)
    {
        Texture *texture = new Texture();
        if (!texture->Create(w, h, rgba, nearest, repeat))
        {
            delete texture;
            return nullptr;
        }

        ct::String key(name);
        Texture **existing = mTextures.find(key);
        if (existing)
        {
            delete *existing;
        }
        mTextures.put(key, texture);
        return texture;
    }

    Texture *Assets::LoadTexture(const char *name, const char *path)
    {
        ct::String resolved;
        if (!FileSystem::Instance().Resolve(path, resolved))
            resolved = path;

        Texture *texture = new Texture();
        if (!texture->Load(resolved.c_str()))
        {
            delete texture;
            return nullptr;
        }

        ct::String key(name);
        Texture **existing = mTextures.find(key);
        if (existing)
        {
            delete *existing;
        }
        mTextures.put(key, texture);
        return texture;
    }

    Shader *Assets::GetShader(const char *name)
    {
        ct::String key(name);
        Shader **found = mShaders.find(key);
        return found ? *found : nullptr;
    }

    Texture *Assets::GetTexture(const char *name)
    {
        ct::String key(name);
        Texture **found = mTextures.find(key);
        return found ? *found : nullptr;
    }

    void Assets::Clear()
    {
        for (auto &entry : mShaders)
        {
            delete entry.value;
        }
        mShaders.clear();

        for (auto &entry : mTextures)
        {
            delete entry.value;
        }
        mTextures.clear();
    }

}
