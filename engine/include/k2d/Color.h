#pragma once

namespace k2d
{

    struct Color
    {
        float r, g, b, a;

        Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
        explicit Color(float grey) : r(grey), g(grey), b(grey), a(grey) {}
        Color(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
        Color(unsigned int packed)
            : r(((packed) & 0xFF) / 255.0f),
              g(((packed >> 8) & 0xFF) / 255.0f),
              b(((packed >> 16) & 0xFF) / 255.0f),
              a(((packed >> 24) & 0xFF) / 255.0f)
        {
        }

        static Color FromBytes(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
        {
            return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
        }

        unsigned int Packed() const
        {
            unsigned char br = (unsigned char)(r * 255.0f + 0.5f);
            unsigned char bg = (unsigned char)(g * 255.0f + 0.5f);
            unsigned char bb = (unsigned char)(b * 255.0f + 0.5f);
            unsigned char ba = (unsigned char)(a * 255.0f + 0.5f);
            return (unsigned int)br | ((unsigned int)bg << 8) | ((unsigned int)bb << 16) | ((unsigned int)ba << 24);
        }

        explicit operator unsigned int() const { return Packed(); }

        static Color White() { return Color(1.0f, 1.0f, 1.0f, 1.0f); }
        static Color Black() { return Color(0.0f, 0.0f, 0.0f, 1.0f); }
        static Color Transparent() { return Color(0.0f, 0.0f, 0.0f, 0.0f); }
        static Color Red() { return Color(1.0f, 0.0f, 0.0f, 1.0f); }
        static Color Green() { return Color(0.0f, 1.0f, 0.0f, 1.0f); }
        static Color Blue() { return Color(0.0f, 0.0f, 1.0f, 1.0f); }
        static Color Yellow() { return Color(1.0f, 1.0f, 0.0f, 1.0f); }

        static Color Lerp(const Color &x, const Color &y, float t)
        {
            return Color(x.r + (y.r - x.r) * t, x.g + (y.g - x.g) * t,
                         x.b + (y.b - x.b) * t, x.a + (y.a - x.a) * t);
        }

        Color operator*(float s) const { return Color(r * s, g * s, b * s, a * s); }
        Color operator*(const Color &o) const { return Color(r * o.r, g * o.g, b * o.b, a * o.a); }
        Color operator+(const Color &o) const { return Color(r + o.r, g + o.g, b + o.b, a + o.a); }
        Color operator-(const Color &o) const { return Color(r - o.r, g - o.g, b - o.b, a - o.a); }
        Color &operator*=(float s) { r *= s; g *= s; b *= s; a *= s; return *this; }
        bool operator==(const Color &o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
        bool operator!=(const Color &o) const { return !(*this == o); }
    };

}
