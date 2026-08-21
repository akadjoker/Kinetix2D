#include <poly2tri.h>

#include <kx/internal/triangulate.h>

#include <vector>
#include <cstdio>
#include <cmath>
#include <cstdint>

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

    struct Lcg
    {
        uint32_t state;
        explicit Lcg(uint32_t seed) : state(seed) {}
        uint32_t Next()
        {
            state = state * 1664525u + 1013904223u;
            return state;
        }
        float Range(float lo, float hi)
        {
            float t = (float)(Next() >> 8) / (float)(1u << 24);
            return lo + t * (hi - lo);
        }
    };

    struct Vec2
    {
        float x, y;
    };

    int MakeSquare(Vec2 *out)
    {
        out[0] = Vec2{-20.0f, -20.0f};
        out[1] = Vec2{20.0f, -20.0f};
        out[2] = Vec2{20.0f, 20.0f};
        out[3] = Vec2{-20.0f, 20.0f};
        return 4;
    }

    int MakeLShape(Vec2 *out)
    {
        out[0] = Vec2{-30.0f, -30.0f};
        out[1] = Vec2{30.0f, -30.0f};
        out[2] = Vec2{30.0f, -10.0f};
        out[3] = Vec2{-10.0f, -10.0f};
        out[4] = Vec2{-10.0f, 30.0f};
        out[5] = Vec2{-30.0f, 30.0f};
        return 6;
    }

    int MakeStar(Vec2 *out)
    {
        const int n = 10;
        for (int i = 0; i < n; ++i)
        {
            float angle = -1.57079633f + (float)i * (6.28318531f / (float)n);
            float radius = (i % 2 == 0) ? 30.0f : 12.0f;
            out[i] = Vec2{radius * cosf(angle), radius * sinf(angle)};
        }
        return n;
    }

    int MakeArrow(Vec2 *out)
    {
        out[0] = Vec2{0.0f, -30.0f};
        out[1] = Vec2{25.0f, 0.0f};
        out[2] = Vec2{10.0f, 0.0f};
        out[3] = Vec2{10.0f, 30.0f};
        out[4] = Vec2{-10.0f, 30.0f};
        out[5] = Vec2{-10.0f, 0.0f};
        out[6] = Vec2{-25.0f, 0.0f};
        return 7;
    }

    int MakeRandomSimplePolygon(Lcg &rng, Vec2 *out, int n)
    {
        float angles[32];
        for (int i = 0; i < n; ++i)
            angles[i] = (float)i * (6.28318531f / (float)n) + rng.Range(-0.05f, 0.05f);

        for (int i = 0; i < n; ++i)
        {
            float radius = rng.Range(10.0f, 30.0f);
            out[i] = Vec2{radius * cosf(angles[i]), radius * sinf(angles[i])};
        }
        return n;
    }

    void ReverseWinding(Vec2 *pts, int count)
    {
        for (int i = 0; i < count / 2; ++i)
        {
            Vec2 tmp = pts[i];
            pts[i] = pts[count - 1 - i];
            pts[count - 1 - i] = tmp;
        }
    }

    double SignedArea2(const Vec2 *pts, int count)
    {
        double a = 0.0;
        for (int i = 0; i < count; ++i)
        {
            int j = (i + 1) % count;
            a += (double)pts[i].x * pts[j].y - (double)pts[j].x * pts[i].y;
        }
        return a;
    }

    double ReferenceTriangulate(const Vec2 *outline, int count, int *outCount)
    {
        Vec2 ccw[32];
        for (int i = 0; i < count; ++i)
            ccw[i] = outline[i];
        if (SignedArea2(ccw, count) < 0.0)
            ReverseWinding(ccw, count);

        std::vector<p2t::Point *> polyline;
        polyline.reserve((size_t)count);
        for (int i = 0; i < count; ++i)
            polyline.push_back(new p2t::Point((double)ccw[i].x, (double)ccw[i].y));

        p2t::CDT cdt(polyline);
        cdt.Triangulate();
        std::vector<p2t::Triangle *> triangles = cdt.GetTriangles();

        double totalArea = 0.0;
        for (size_t i = 0; i < triangles.size(); ++i)
        {
            p2t::Point *a = triangles[i]->GetPoint(0);
            p2t::Point *b = triangles[i]->GetPoint(1);
            p2t::Point *c = triangles[i]->GetPoint(2);
            double area = 0.5 * std::fabs((b->x - a->x) * (c->y - a->y) - (c->x - a->x) * (b->y - a->y));
            totalArea += area;
        }

        *outCount = (int)triangles.size();

        for (size_t i = 0; i < polyline.size(); ++i)
            delete polyline[i];

        return totalArea;
    }

    bool PointInInput(const Vec2 &p, const Vec2 *outline, int count, float eps)
    {
        for (int i = 0; i < count; ++i)
        {
            float dx = p.x - outline[i].x;
            float dy = p.y - outline[i].y;
            if (dx * dx + dy * dy <= eps * eps)
                return true;
        }
        return false;
    }

    void RunCase(const char *name, const Vec2 *outline, int count)
    {
        int refCount = 0;
        double refArea = ReferenceTriangulate(outline, count, &refCount);

        glm::vec2 kxOutline[32];
        for (int i = 0; i < count; ++i)
            kxOutline[i] = glm::vec2(outline[i].x, outline[i].y);

        glm::vec2 kxTriangles[32 * 3];
        int kxCount = kx::Triangulate(kxOutline, count, kxTriangles, 32);

        char buf[160];

        std::snprintf(buf, sizeof(buf), "%s: mesmo numero de triangulos (ref=%d kx=%d)", name, refCount, kxCount);
        Check(refCount == kxCount, buf);

        double kxArea = 0.0;
        bool allValid = true;
        for (int i = 0; i < kxCount; ++i)
        {
            glm::vec2 a = kxTriangles[i * 3 + 0];
            glm::vec2 b = kxTriangles[i * 3 + 1];
            glm::vec2 c = kxTriangles[i * 3 + 2];
            double area = 0.5 * std::fabs((double)(b.x - a.x) * (c.y - a.y) - (double)(c.x - a.x) * (b.y - a.y));
            kxArea += area;

            if (area < 1e-4)
                allValid = false;
            if (!PointInInput(Vec2{a.x, a.y}, outline, count, 1e-2f) ||
                !PointInInput(Vec2{b.x, b.y}, outline, count, 1e-2f) ||
                !PointInInput(Vec2{c.x, c.y}, outline, count, 1e-2f))
                allValid = false;
        }

        std::snprintf(buf, sizeof(buf), "%s: todos os triangulos kx tem area positiva e vertices do input", name);
        Check(allValid, buf);

        double relError = refArea > 1e-6 ? std::fabs(kxArea - refArea) / refArea : std::fabs(kxArea - refArea);
        std::snprintf(buf, sizeof(buf), "%s: area total bate (ref=%.3f kx=%.3f rel=%.6f)", name, refArea, kxArea, relError);
        Check(relError < 1e-3, buf);
    }

} // namespace

int main()
{
    Vec2 outline[32];
    int count;

    count = MakeSquare(outline);
    RunCase("quadrado", outline, count);
    ReverseWinding(outline, count);
    RunCase("quadrado invertido", outline, count);

    count = MakeLShape(outline);
    RunCase("L-shape", outline, count);
    ReverseWinding(outline, count);
    RunCase("L-shape invertido", outline, count);

    count = MakeStar(outline);
    RunCase("estrela", outline, count);
    ReverseWinding(outline, count);
    RunCase("estrela invertida", outline, count);

    count = MakeArrow(outline);
    RunCase("seta", outline, count);
    ReverseWinding(outline, count);
    RunCase("seta invertida", outline, count);

    Lcg rng(42);
    for (int n = 5; n <= 30; ++n)
    {
        count = MakeRandomSimplePolygon(rng, outline, n);
        char name[64];
        std::snprintf(name, sizeof(name), "poligono aleatorio n=%d", n);
        RunCase(name, outline, count);
    }

    std::printf("\n%d passed, %d failed\n", gPass, gFail);
    return gFail == 0 ? 0 : 1;
}
