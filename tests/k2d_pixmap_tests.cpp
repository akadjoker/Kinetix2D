#include <k2d/Pixmap.h>

#include <cstdio>

static bool Pixel(const k2d::Pixmap &pixmap, int x, int y,
                  unsigned char r, unsigned char g,
                  unsigned char b, unsigned char a)
{
    const unsigned char *p = pixmap.Pixels() + ((y * pixmap.Width() + x) * 4);
    return p[0] == r && p[1] == g && p[2] == b && p[3] == a;
}

int main()
{
    k2d::Pixmap pixmap(8, 8);
    pixmap.Clear(1, 2, 3, 4);
    bool clear = Pixel(pixmap, 0, 0, 1, 2, 3, 4) && Pixel(pixmap, 7, 7, 1, 2, 3, 4);

    pixmap.SetPixel(3, 3, 10, 20, 30, 40);
    bool pixel = Pixel(pixmap, 3, 3, 10, 20, 30, 40);

    pixmap.FillRect(1, 1, 2, 3, 50, 60, 70, 80);
    bool rect = Pixel(pixmap, 1, 1, 50, 60, 70, 80) &&
                Pixel(pixmap, 2, 3, 50, 60, 70, 80) &&
                Pixel(pixmap, 4, 4, 1, 2, 3, 4);

    pixmap.Clear(0, 0, 0, 0);
    pixmap.FillCircle(4, 4, 2, 100, 110, 120, 130);
    bool circle = Pixel(pixmap, 4, 4, 100, 110, 120, 130) &&
                  Pixel(pixmap, 0, 0, 0, 0, 0, 0);

    std::printf("pixmap: clear=%s pixel=%s rect=%s circle=%s\n",
                clear ? "pass" : "fail", pixel ? "pass" : "fail",
                rect ? "pass" : "fail", circle ? "pass" : "fail");
    return clear && pixel && rect && circle ? 0 : 1;
}
