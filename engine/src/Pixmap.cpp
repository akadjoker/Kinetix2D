#include "k2d/Pixmap.h"

#include "k2d/Assets.h"

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
