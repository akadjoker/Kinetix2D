#pragma once

namespace k2d
{
    class Assets;
    class Texture;

    class Pixmap
    {
    public:
        Pixmap();
        Pixmap(int width, int height);
        ~Pixmap();

        Pixmap(const Pixmap &) = delete;
        Pixmap &operator=(const Pixmap &) = delete;

        bool Create(int width, int height);
        void Clear(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
        void SetPixel(int x, int y, unsigned char r, unsigned char g,
                      unsigned char b, unsigned char a = 255);
        void FillRect(int x, int y, int width, int height,
                      unsigned char r, unsigned char g, unsigned char b,
                      unsigned char a = 255);
        void FillCircle(int centerX, int centerY, int radius,
                        unsigned char r, unsigned char g, unsigned char b,
                        unsigned char a = 255);

        Texture *CreateTexture(Assets &assets, const char *name,
                               bool nearest = true, bool repeat = false) const;

        int Width() const { return mWidth; }
        int Height() const { return mHeight; }
        const unsigned char *Pixels() const { return mPixels; }
        unsigned char *Pixels() { return mPixels; }

    private:
        int mWidth;
        int mHeight;
        unsigned char *mPixels;
    };
}
