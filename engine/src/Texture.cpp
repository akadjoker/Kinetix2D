#include "k2d/Texture.h"
#include "k2d/FileBuffer.h"

#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace k2d
{

    Texture::Texture()
        : mId(0), mWidth(0), mHeight(0)
    {
    }

    Texture::~Texture()
    {
        Release();
    }

    void Texture::Release()
    {
        if (mId)
        {
            glDeleteTextures(1, &mId);
            mId = 0;
        }
        mWidth = 0;
        mHeight = 0;
    }

    bool Texture::Create(int width, int height, const unsigned char *rgba, bool nearest, bool repeat)
    {
        if (width <= 0 || height <= 0)
            return false;

        Release();

        unsigned int id = 0;
        glGenTextures(1, &id);
        glBindTexture(GL_TEXTURE_2D, id);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

        int filter = nearest ? GL_NEAREST : GL_LINEAR;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

        int wrap = repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

        glBindTexture(GL_TEXTURE_2D, 0);

        mId = id;
        mWidth = width;
        mHeight = height;
        return true;
    }

    bool Texture::Load(const char *path, bool nearest, bool repeat)
    {
        FileBuffer file;
        if (!file.Load(path, false))
            return false;

        int w = 0;
        int h = 0;
        int channels = 0;
        unsigned char *pixels = stbi_load_from_memory(file.Data(), (int)file.Size(), &w, &h, &channels, 4);
        if (!pixels)
            return false;

        bool ok = Create(w, h, pixels, nearest, repeat);

        stbi_image_free(pixels);
        return ok;
    }

    void Texture::Bind(int unit) const
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, mId);
    }

}