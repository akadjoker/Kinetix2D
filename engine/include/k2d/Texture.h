#pragma once

namespace k2d
{

    class Texture
    {
    public:
        Texture();
        ~Texture();

        Texture(const Texture &) = delete;
        Texture &operator=(const Texture &) = delete;

        bool Create(int width, int height, const unsigned char *rgba, bool nearest = true, bool repeat = false);
        bool Load(const char *path, bool nearest = true, bool repeat = false);

        void Bind(int unit = 0) const;
        void Release();

        int Width() const { return mWidth; }
        int Height() const { return mHeight; }
        unsigned int Id() const { return mId; }

    private:
        unsigned int mId;
        int mWidth;
        int mHeight;
    };

}
