#include <kx/kx.h>

#include <cstdio>
#include <cmath>
#include <cstring>

namespace
{

    int gPass = 0;
    int gFail = 0;

    void Check(bool cond, const char *name)
    {
        if (cond)
        {
            ++gPass;
            std::printf("[PASS] %s\n", name);
        }
        else
        {
            ++gFail;
            std::printf("[FAIL] %s\n", name);
        }
    }

    void FillCircle(unsigned char *pix, int w, int h, float cx, float cy, float radius)
    {
        std::memset(pix, 0, (size_t)(w * h));
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                float dx = (float)x + 0.5f - cx;
                float dy = (float)y + 0.5f - cy;
                if (dx * dx + dy * dy <= radius * radius)
                    pix[y * w + x] = 255;
            }
        }
    }

    void FillRing(unsigned char *pix, int w, int h, float cx, float cy, float rInner, float rOuter)
    {
        std::memset(pix, 0, (size_t)(w * h));
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                float dx = (float)x + 0.5f - cx;
                float dy = (float)y + 0.5f - cy;
                float d2 = dx * dx + dy * dy;
                if (d2 >= rInner * rInner && d2 <= rOuter * rOuter)
                    pix[y * w + x] = 255;
            }
        }
    }

    void FillLShape(unsigned char *pix, int w, int h)
    {
        std::memset(pix, 0, (size_t)(w * h));
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                bool bottomBar = (x >= 4 && x < w - 4 && y >= h - 14 && y < h - 4);
                bool leftBar = (x >= 4 && x < 14 && y >= 4 && y < h - 4);
                if (bottomBar || leftBar)
                    pix[y * w + x] = 255;
            }
        }
    }

    void FillCheckerboardBlobs(unsigned char *pix, int w, int h)
    {
        std::memset(pix, 0, (size_t)(w * h));
        int blobs[4][2] = {{6, 6}, {w - 12, 6}, {6, h - 12}, {w - 12, h - 12}};
        for (int b = 0; b < 4; ++b)
        {
            for (int y = 0; y < 6; ++y)
                for (int x = 0; x < 6; ++x)
                    pix[(blobs[b][1] + y) * w + (blobs[b][0] + x)] = 255;
        }
    }

    double PolylineArea2(const kx::Polyline &line)
    {
        const ct::Vector<Math::Vec2> &pts = line.points;
        size_t n = pts.size();
        if (n < 3)
            return 0.0;
        double area = 0.0;
        for (size_t i = 0; i < n; ++i)
        {
            size_t j = (i + 1) % n;
            area += (double)pts[i].x * pts[j].y - (double)pts[j].x * pts[i].y;
        }
        return area < 0.0 ? -area : area;
    }

    double BodyTriangleArea(kx::Body *body)
    {
        double total = 0.0;
        for (int s = 0; s < body->ShapeCount(); ++s)
        {
            const kx::Polygon &poly = body->Shapes()[s].polygon;
            double area = 0.0;
            for (int32_t i = 0; i < poly.count; ++i)
            {
                int32_t j = (i + 1) % poly.count;
                area += (double)poly.vertices[i].x * poly.vertices[j].y - (double)poly.vertices[j].x * poly.vertices[i].y;
            }
            total += 0.5 * (area < 0.0 ? -area : area);
        }
        return total;
    }

} 

int main()
{
    {
        const int w = 60, h = 60;
        unsigned char pix[60 * 60];
        FillCircle(pix, w, h, 30.0f, 30.0f, 20.0f);

        ct::Vector<kx::Polyline> lines;
        int count = kx::TraceImageOutlines(pix, w, h, 1, 128, lines);
        Check(count == 1, "circulo cheio: exatamente 1 contorno");

        if (count == 1)
        {
            double area = 0.5 * PolylineArea2(lines[0]);
            double expected = 3.14159265359 * 20.0 * 20.0;
            double relError = std::fabs(area - expected) / expected;
            char buf[128];
            std::snprintf(buf, sizeof(buf), "circulo cheio: area do contorno ~pi*r^2 (rel=%.4f)", relError);
            Check(relError < 0.08, buf);
        }
    }

    {
        const int w = 64, h = 64;
        unsigned char pix[64 * 64];
        FillRing(pix, w, h, 32.0f, 32.0f, 10.0f, 24.0f);

        ct::Vector<kx::Polyline> lines;
        int count = kx::TraceImageOutlines(pix, w, h, 1, 128, lines);
        Check(count == 2, "anel (donut): exatamente 2 contornos");
    }

    {
        const int w = 40, h = 40;
        unsigned char pix[40 * 40];
        FillLShape(pix, w, h);

        ct::Vector<kx::Polyline> lines;
        int count = kx::TraceImageOutlines(pix, w, h, 1, 128, lines);
        Check(count == 1, "L-shape: exatamente 1 contorno");
    }

    {
        const int w = 20, h = 20;
        unsigned char pix[20 * 20];
        std::memset(pix, 0, sizeof(pix));
        pix[10 * w + 10] = 255;

        ct::Vector<kx::Polyline> lines;
        int count = kx::TraceImageOutlines(pix, w, h, 1, 128, lines);
        Check(count >= 0, "pixel isolado: nao rebenta");
        if (count > 0)
            Check(lines[0].points.size() >= 3, "pixel isolado: contorno tem pelo menos 3 pontos");
    }

    {
        const int w = 20, h = 20;
        unsigned char pix[20 * 20];
        std::memset(pix, 0, sizeof(pix));

        ct::Vector<kx::Polyline> lines;
        int count = kx::TraceImageOutlines(pix, w, h, 1, 128, lines);
        Check(count == 0, "imagem vazia: zero contornos");

        kx::World world(Math::Vec2(0.0f, 500.0f));
        kx::Body *body = world.CreateFromImage(Math::Vec2(0.0f, 0.0f), pix, w, h, 1, 128, 1.0f);
        Check(body == nullptr, "imagem vazia: CreateFromImage devolve nullptr");
    }

    {
        const int w = 40, h = 40;
        unsigned char pix[40 * 40];
        FillCheckerboardBlobs(pix, w, h);

        ct::Vector<kx::Polyline> lines;
        int count = kx::TraceImageOutlines(pix, w, h, 1, 128, lines);
        Check(count == 4, "checkerboard 2x2: exatamente 4 contornos disjuntos");
    }

    {
        kx::Polyline staircase;
        staircase.points.push_back(Math::Vec2(0.0f, 0.0f));
        staircase.points.push_back(Math::Vec2(5.0f, 0.0f));
        staircase.points.push_back(Math::Vec2(10.0f, 0.0f));
        staircase.points.push_back(Math::Vec2(10.0f, 5.0f));
        staircase.points.push_back(Math::Vec2(10.0f, 10.0f));
        staircase.points.push_back(Math::Vec2(5.0f, 10.0f));
        staircase.points.push_back(Math::Vec2(0.0f, 10.0f));
        staircase.points.push_back(Math::Vec2(0.0f, 5.0f));
        staircase.points.push_back(Math::Vec2(0.0f, 0.0f));

        kx::Polyline simplified;
        kx::SimplifyOutline(staircase, 0.05f, simplified);

        Check(simplified.points.size() == 5, "SimplifyOutline: retangulo com pontos colineares reduz aos 4 cantos (+fecho)");

        bool cornersOk = simplified.points.size() == 5 &&
                        simplified.points[0] == Math::Vec2(0.0f, 0.0f) &&
                        simplified.points[1] == Math::Vec2(10.0f, 0.0f) &&
                        simplified.points[2] == Math::Vec2(10.0f, 10.0f) &&
                        simplified.points[3] == Math::Vec2(0.0f, 10.0f) &&
                        simplified.points[4] == Math::Vec2(0.0f, 0.0f);
        Check(cornersOk, "SimplifyOutline: cantos preservados exatamente (algoritmo de Chipmunk)");
    }

    {
        const int w = 50, h = 50;
        unsigned char pix[50 * 50];
        FillLShape(pix, w, h);

        kx::World world(Math::Vec2(0.0f, 500.0f));
        world.CreateStaticBox(Math::Vec2(0.0f, 300.0f), 500.0f, 10.0f);

        kx::Body *body = world.CreateFromImage(Math::Vec2(0.0f, 0.0f), pix, w, h, 1, 128, 1.0f);
        Check(body != nullptr, "CreateFromImage no L: corpo criado");

        if (body)
        {
            Check(body->Mass() > 0.0f, "CreateFromImage no L: massa positiva");
            Check(body->ShapeCount() > 0, "CreateFromImage no L: pelo menos uma forma");

            double pixelArea = 0.0;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    if (pix[y * w + x] >= 128)
                        pixelArea += 1.0;

            double triArea = BodyTriangleArea(body);
            double relError = std::fabs(triArea - pixelArea) / pixelArea;
            char buf[160];
            std::snprintf(buf, sizeof(buf), "CreateFromImage no L: area triangulada ~area em pixels (rel=%.4f)", relError);
            Check(relError < 0.10, buf);

            for (int i = 0; i < 240; ++i)
                world.Step(1.0f / 60.0f);

            Check(std::isfinite(body->Position().x) && std::isfinite(body->Position().y), "CreateFromImage no L: nao diverge");
            Check(body->Position().y < 320.0f, "CreateFromImage no L: assenta no chao sem atravessar");
        }
    }

    std::printf("\n%d passed, %d failed\n", gPass, gFail);
    return gFail == 0 ? 0 : 1;
}