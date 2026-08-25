#include "k2d/Assets.h"

#include "k2d/FileBuffer.h"
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

Shader* Assets::LoadShader(const char* name, const char* vsSrc, const char* fsSrc)
{
    Shader* shader = new Shader();
    if (!shader->CompileSource(vsSrc, fsSrc))
    {
        delete shader;
        return nullptr;
    }

    ct::String key(name);
    Shader** existing = mShaders.find(key);
    if (existing)
    {
        delete *existing;
    }
    mShaders.put(key, shader);
    return shader;
}

Shader* Assets::LoadShaderFiles(const char* name, const char* vsPath, const char* fsPath)
{
    FileBuffer vertex;
    FileBuffer fragment;
    if (!FileSystem::Instance().LoadFile(vsPath, vertex, true) ||
        !FileSystem::Instance().LoadFile(fsPath, fragment, true))
        return nullptr;

    Shader* shader = new Shader();
    if (!shader->CompileSource(vertex.Text(), fragment.Text()))
    {
        delete shader;
        return nullptr;
    }

    ct::String key(name);
    Shader** existing = mShaders.find(key);
    if (existing)
    {
        delete *existing;
    }
    mShaders.put(key, shader);
    return shader;
}

Texture* Assets::CreateTexture(const char* name, int w, int h, const unsigned char* rgba, bool nearest, bool repeat)
{
    Texture* texture = new Texture();
    if (!texture->Create(w, h, rgba, nearest, repeat))
    {
        delete texture;
        return nullptr;
    }

    ct::String key(name);
    Texture** existing = mTextures.find(key);
    if (existing)
    {
        delete *existing;
    }
    mTextures.put(key, texture);
    return texture;
}

Texture* Assets::LoadTexture(const char* name, const char* path, bool nearest, bool repeat)
{
    FileBuffer image;
    if (!FileSystem::Instance().LoadFile(path, image))
        return nullptr;

    Texture* texture = new Texture();
    if (!texture->LoadMemory(image.Data(), image.Size(), nearest, repeat))
    {
        delete texture;
        return nullptr;
    }

    ct::String key(name);
    Texture** existing = mTextures.find(key);
    if (existing)
    {
        delete *existing;
    }
    mTextures.put(key, texture);
    return texture;
}

Texture* Assets::LoadTextureMemory(const char* name, const unsigned char* data, std::size_t size, bool nearest,
                                   bool repeat)
{
    if (!name || !data || size == 0)
        return nullptr;
    Texture* texture = new Texture();
    if (!texture->LoadMemory(data, size, nearest, repeat))
    {
        delete texture;
        return nullptr;
    }

    ct::String key(name);
    Texture** existing = mTextures.find(key);
    if (existing)
        delete *existing;
    mTextures.put(key, texture);
    return texture;
}

Shader* Assets::GetShader(const char* name)
{
    ct::String key(name);
    Shader** found = mShaders.find(key);
    return found ? *found : nullptr;
}

Texture* Assets::GetTexture(const char* name)
{
    ct::String key(name);
    Texture** found = mTextures.find(key);
    return found ? *found : nullptr;
}

const char* Assets::FindTextureName(const Texture* texture) const
{
    if (!texture)
        return nullptr;

    auto& textures = const_cast<ct::HashMap<ct::String, Texture*>&>(mTextures);
    for (auto& entry : textures)
    {
        if (entry.value == texture)
            return entry.key.c_str();
    }
    return nullptr;
}

void Assets::Clear()
{
    for (auto& entry : mShaders)
    {
        delete entry.value;
    }
    mShaders.clear();

    for (auto& entry : mTextures)
    {
        delete entry.value;
    }
    mTextures.clear();
}

} // namespace k2d
