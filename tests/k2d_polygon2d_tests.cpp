#include "k2d/Polygon2D.h"

#include <cstdio>
#include <cmath>

int main()
{
    const glm::vec2 concave[] = {
        glm::vec2(0.0f, 0.0f), glm::vec2(40.0f, 0.0f), glm::vec2(40.0f, 40.0f),
        glm::vec2(20.0f, 20.0f), glm::vec2(0.0f, 40.0f)};
    k2d::Polygon2D polygon;
    polygon.setPolygon(concave, 5);
    float triangleArea = 0.0f;
    const ct::Vector<glm::vec2> &triangles = polygon.triangles();
    for (size_t i = 0; i + 2 < triangles.size(); i += 3)
    {
        glm::vec2 a = triangles[i + 1] - triangles[i];
        glm::vec2 b = triangles[i + 2] - triangles[i];
        triangleArea += std::fabs(a.x * b.y - a.y * b.x) * 0.5f;
    }
    bool ok = polygon.valid() && polygon.triangles().size() == 9 &&
              std::fabs(triangleArea - 1200.0f) < 0.01f;
    polygon.setPolygon(nullptr, 0);
    ok = ok && !polygon.valid();

    const int largeCount = 300;
    glm::vec2 largePolygon[largeCount];
    for (int i = 0; i < largeCount; ++i)
    {
        float angle = (float)i / (float)largeCount * 6.28318530718f;
        largePolygon[i] = glm::vec2(std::cos(angle), std::sin(angle)) * 1000.0f;
    }
    k2d::Polygon2D largePoly;
    largePoly.setPolygon(largePolygon, largeCount);
    bool largeOk = largePoly.valid() && largePoly.triangles().size() == (size_t)(largeCount - 2) * 3;
    std::printf("polygon2d=%s large_untruncated=%s\n", ok ? "pass" : "fail",
                largeOk ? "pass" : "fail");
    ok = ok && largeOk;
    return ok ? 0 : 1;
}