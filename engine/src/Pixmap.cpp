#include "k2d/Pixmap.h"

#include "k2d/Assets.h"
#include "k2d/FileBuffer.h"

#include <stb_image.h>
#include <stb_image_write.h>

#include <cmath>
#include <cstring>

namespace k2d
{
    Pixmap::Pixmap() : mWidth(0), mHeight(0), mPixels(nullptr) {}

    Pixmap::Pixmap(int width, int height) : mWidth(0), mHeight(0), mPixels(nullptr)
    {
        Create(width, height);
    }

    Pixmap::~Pixmap()
    {
        delete[] mPixels;
    }

    bool Pixmap::Create(int width, int height)
    {
        if (width <= 0 || height <= 0)
            return false;
        unsigned char *pixels = new unsigned char[(size_t)width * (size_t)height * 4u];
        std::memset(pixels, 0, (size_t)width * (size_t)height * 4u);
        delete[] mPixels;
        mPixels = pixels;
        mWidth = width;
        mHeight = height;
        return true;
    }

    bool Pixmap::Load(const char *path)
    {
        FileBuffer buffer;
        if (!buffer.Load(path, false))
            return false;

        int w = 0;
        int h = 0;
        int channels = 0;
        unsigned char *pixels = stbi_load_from_memory(buffer.Data(), (int)buffer.Size(), &w, &h, &channels, 4);
        if (!pixels)
            return false;

        const bool ok = Create(w, h);
        if (ok)
            std::memcpy(mPixels, pixels, (size_t)w * (size_t)h * 4u);
        stbi_image_free(pixels);
        return ok;
    }

    bool Pixmap::Save(const char *path) const
    {
        if (!mPixels || mWidth <= 0 || mHeight <= 0 || !path)
            return false;
        return stbi_write_png(path, mWidth, mHeight, 4, mPixels, mWidth * 4) != 0;
    }

    Pixmap *Pixmap::GenerateNormalMap(float strength) const
    {
        if (!mPixels || mWidth <= 0 || mHeight <= 0)
            return nullptr;

        Pixmap *result = new Pixmap();
        if (!result->Create(mWidth, mHeight))
        {
            delete result;
            return nullptr;
        }

        const auto heightAt = [this](int x, int y) -> float
        {
            x = x < 0 ? 0 : (x >= mWidth ? mWidth - 1 : x);
            y = y < 0 ? 0 : (y >= mHeight ? mHeight - 1 : y);
            const unsigned char *pixel = mPixels + ((size_t)y * (size_t)mWidth + (size_t)x) * 4u;
            return (pixel[0] * 0.299f + pixel[1] * 0.587f + pixel[2] * 0.114f) / 255.0f;
        };

        for (int y = 0; y < mHeight; ++y)
        {
            for (int x = 0; x < mWidth; ++x)
            {
                const float tl = heightAt(x - 1, y - 1);
                const float t = heightAt(x, y - 1);
                const float tr = heightAt(x + 1, y - 1);
                const float l = heightAt(x - 1, y);
                const float r = heightAt(x + 1, y);
                const float bl = heightAt(x - 1, y + 1);
                const float b = heightAt(x, y + 1);
                const float br = heightAt(x + 1, y + 1);

                const float dx = (tr + 2.0f * r + br) - (tl + 2.0f * l + bl);
                const float dy = (bl + 2.0f * b + br) - (tl + 2.0f * t + tr);

                float nx = -dx * strength;
                float ny = -dy * strength;
                float nz = 1.0f;
                const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (length > 0.0001f)
                {
                    nx /= length;
                    ny /= length;
                    nz /= length;
                }

                result->SetPixel(x, y, (unsigned char)((nx * 0.5f + 0.5f) * 255.0f),
                                 (unsigned char)((ny * 0.5f + 0.5f) * 255.0f),
                                 (unsigned char)((nz * 0.5f + 0.5f) * 255.0f), 255);
            }
        }
        return result;
    }

    void Pixmap::SetPixel(int x, int y, unsigned char r, unsigned char g,
                          unsigned char b, unsigned char a)
    {
        if (!mPixels || x < 0 || y < 0 || x >= mWidth || y >= mHeight)
            return;
        unsigned char *pixel = mPixels + ((size_t)y * (size_t)mWidth + (size_t)x) * 4u;
        pixel[0] = r; pixel[1] = g; pixel[2] = b; pixel[3] = a;
    }

    void Pixmap::Clear(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        if (!mPixels)
            return;
        for (int y = 0; y < mHeight; ++y)
            for (int x = 0; x < mWidth; ++x)
                SetPixel(x, y, r, g, b, a);
    }

    void Pixmap::FillRect(int x, int y, int width, int height,
                          unsigned char r, unsigned char g, unsigned char b,
                          unsigned char a)
    {
        for (int py = y; py < y + height; ++py)
            for (int px = x; px < x + width; ++px)
                SetPixel(px, py, r, g, b, a);
    }

    void Pixmap::FillCircle(int centerX, int centerY, int radius,
                            unsigned char r, unsigned char g, unsigned char b,
                            unsigned char a)
    {
        if (radius < 0)
            return;
        int radiusSquared = radius * radius;
        for (int y = centerY - radius; y <= centerY + radius; ++y)
            for (int x = centerX - radius; x <= centerX + radius; ++x)
                if ((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY) <= radiusSquared)
                    SetPixel(x, y, r, g, b, a);
    }

    Texture *Pixmap::CreateTexture(Assets &assets, const char *name,
                                   bool nearest, bool repeat) const
    {
        if (!mPixels || !name)
            return nullptr;
        return assets.CreateTexture(name, mWidth, mHeight, mPixels, nearest, repeat);
    }
}