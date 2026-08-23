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

    k2d::Pixmap saved(4, 4);
    saved.Clear(200, 150, 50, 255);
    saved.SetPixel(2, 2, 10, 20, 30, 255);
    const char *savePath = "/tmp/k2d_pixmap_test.png";
    const bool savedOk = saved.Save(savePath);

    k2d::Pixmap loaded;
    const bool loadedOk = savedOk && loaded.Load(savePath) &&
                          loaded.Width() == 4 && loaded.Height() == 4 &&
                          Pixel(loaded, 0, 0, 200, 150, 50, 255) &&
                          Pixel(loaded, 2, 2, 10, 20, 30, 255);

    k2d::Pixmap source(16, 16);
    source.Clear(0, 0, 0, 255);
    source.FillRect(8, 0, 8, 16, 255, 255, 255, 255);
    k2d::Pixmap *normalMap = source.GenerateNormalMap(2.0f);
    bool flatOk = false;
    bool edgeOk = false;
    if (normalMap)
    {
        const unsigned char *flat = normalMap->Pixels() + ((8 * 16 + 2) * 4);
        flatOk = flat[0] > 120 && flat[0] < 136 && flat[1] > 120 && flat[1] < 136 && flat[2] > 200;

        const unsigned char *edge = normalMap->Pixels() + ((8 * 16 + 8) * 4);
        int redDiff = static_cast<int>(edge[0]) - 128;
        redDiff = redDiff < 0 ? -redDiff : redDiff;
        edgeOk = redDiff > 20;

        delete normalMap;
    }
    const bool normalMapOk = normalMap != nullptr;

    std::printf("pixmap: clear=%s pixel=%s rect=%s circle=%s save=%s load=%s normalmap=%s "
               "normal_flat=%s normal_edge=%s\n",
                clear ? "pass" : "fail", pixel ? "pass" : "fail",
                rect ? "pass" : "fail", circle ? "pass" : "fail",
                savedOk ? "pass" : "fail", loadedOk ? "pass" : "fail",
                normalMapOk ? "pass" : "fail", flatOk ? "pass" : "fail", edgeOk ? "pass" : "fail");
    return clear && pixel && rect && circle && savedOk && loadedOk && normalMapOk && flatOk && edgeOk
               ? 0
               : 1;
}