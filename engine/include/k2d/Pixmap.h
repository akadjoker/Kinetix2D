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
        bool Load(const char *path);
        bool Save(const char *path) const;
        Pixmap *GenerateNormalMap(float strength = 2.0f) const;
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

        // Preferred camelCase API. PascalCase names above remain compatible.
        bool create(int width, int height) { return Create(width, height); }
        bool load(const char *path) { return Load(path); }
        bool save(const char *path) const { return Save(path); }
        Pixmap *generateNormalMap(float strength = 2.0f) const { return GenerateNormalMap(strength); }
        void clear(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) { Clear(r, g, b, a); }
        void setPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
        { SetPixel(x, y, r, g, b, a); }
        void fillRect(int x, int y, int width, int height, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
        { FillRect(x, y, width, height, r, g, b, a); }
        void fillCircle(int centerX, int centerY, int radius, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
        { FillCircle(centerX, centerY, radius, r, g, b, a); }
        int width() const { return Width(); }
        int height() const { return Height(); }
        const unsigned char *pixels() const { return Pixels(); }
        unsigned char *pixels() { return Pixels(); }

    private:
        int mWidth;
        int mHeight;
        unsigned char *mPixels;
    };
}
