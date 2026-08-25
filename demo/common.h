#include <k2d/k2d.h>

#include <imgui.h>
#include <kx/kx.h>

#include <k2d/OpenGL.h>

#include <ct/string.hpp>
#include <ct/vector.hpp>

static const int SCANCODE_ESCAPE = 41;
static const int SCANCODE_SPACE = 44;
static const int SCANCODE_R = 21;
static const int SCANCODE_W = 26;
static const int SCANCODE_A = 4;
static const int SCANCODE_S = 22;
static const int SCANCODE_D = 7;
static const int SCANCODE_1 = 30;
static const int SCANCODE_2 = 31;
static const int SCANCODE_F1 = 58;
static const int SCANCODE_F2 = 59;
static const int SCANCODE_F3 = 60;
static const int SCANCODE_F4 = 61;

static unsigned gSeed = 777;
inline float Rnd()
{
    gSeed = gSeed * 1664525u + 1013904223u;
    return (float)(gSeed >> 8) / 16777216.0f;
}

struct Camera
{
    Math::Vec2 center;
    float zoom;

    Math::Mat4 Projection(float screenW, float screenH) const
    {
        float hw = screenW * 0.5f / zoom;
        float hh = screenH * 0.5f / zoom;
        return Math::Mat4::Ortho(center.x - hw, center.x + hw, center.y + hh, center.y - hh, -1.0f, 1.0f);
    }

    Math::Vec2 ScreenToWorld(float sx, float sy, float screenW, float screenH) const
    {
        return Math::Vec2(center.x + (sx - screenW * 0.5f) / zoom, center.y + (sy - screenH * 0.5f) / zoom);
    }
};

class StressDebugDraw : public kx::DebugDraw
{
  public:
    explicit StressDebugDraw(k2d::BatchRenderer& batch) : mBatch(batch)
    {
    }

    void DrawCircleShape(const kx::Transform& xf, float radius, kx::Color color) override
    {
        Math::Vec2 c = xf.Transform(0.0f, 0.0f);
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawCircle(c.x, c.y, radius, 12);
    }

    void DrawPolygonShape(const kx::Transform& xf, const Math::Vec2* verts, int count, kx::Color color) override
    {
        if (count <= 0 || count > 16)
            return;
        float points[(16 + 1) * 2];
        for (int i = 0; i < count; ++i)
        {
            Math::Vec2 p = xf.Transform(verts[i]);
            points[i * 2 + 0] = p.x;
            points[i * 2 + 1] = p.y;
        }
        Math::Vec2 first = xf.Transform(verts[0]);
        points[count * 2 + 0] = first.x;
        points[count * 2 + 1] = first.y;
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawPolyline(points, count + 1);
    }

    void DrawSegment(const Math::Vec2& a, const Math::Vec2& b, kx::Color color) override
    {
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawLine(a.x, a.y, b.x, b.y);
    }

    void DrawPoint(const Math::Vec2& p, float size, kx::Color color) override
    {
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawCircle(p.x, p.y, size, 6);
    }

    void DrawAABB(const Math::Vec2& lower, const Math::Vec2& upper, kx::Color color) override
    {
        mBatch.SetColor(color.r, color.g, color.b, color.a);
        mBatch.DrawRect(lower.x, lower.y, upper.x - lower.x, upper.y - lower.y, false);
    }

  private:
    k2d::BatchRenderer& mBatch;
};
