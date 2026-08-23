#include "k2d/Polygon2D.h"

#include <cstdio>
#include <cmath>

int main()
{
    const Math::Vec2 concave[] = {
        Math::Vec2(0.0f, 0.0f), Math::Vec2(40.0f, 0.0f), Math::Vec2(40.0f, 40.0f),
        Math::Vec2(20.0f, 20.0f), Math::Vec2(0.0f, 40.0f)};
    k2d::Polygon2D polygon;
    polygon.setPolygon(concave, 5);
    float triangleArea = 0.0f;
    const ct::Vector<Math::Vec2> &triangles = polygon.triangles();
    for (size_t i = 0; i + 2 < triangles.size(); i += 3)
    {
        Math::Vec2 a = triangles[i + 1] - triangles[i];
        Math::Vec2 b = triangles[i + 2] - triangles[i];
        triangleArea += std::fabs(a.x * b.y - a.y * b.x) * 0.5f;
    }
    bool ok = polygon.valid() && polygon.triangles().size() == 9 &&
              std::fabs(triangleArea - 1200.0f) < 0.01f;
    polygon.setPolygon(nullptr, 0);
    ok = ok && !polygon.valid();

    const int largeCount = 300;
    Math::Vec2 largePolygon[largeCount];
    for (int i = 0; i < largeCount; ++i)
    {
        float angle = (float)i / (float)largeCount * 6.28318530718f;
        largePolygon[i] = Math::Vec2(std::cos(angle), std::sin(angle)) * 1000.0f;
    }
    k2d::Polygon2D largePoly;
    largePoly.setPolygon(largePolygon, largeCount);
    bool largeOk = largePoly.valid() && largePoly.triangles().size() == (size_t)(largeCount - 2) * 3;
    std::printf("polygon2d=%s large_untruncated=%s\n", ok ? "pass" : "fail",
                largeOk ? "pass" : "fail");
    ok = ok && largeOk;
    return ok ? 0 : 1;
}