#include "k2d/NavigationRegion2D.h"

#include "k2d/GameObject.h"
#include "k2d/Navigation2D.h"

#include "kx/internal/triangulate.h"

#include <cmath>
#include <limits>

namespace k2d
{
namespace
{
struct Face
{
    Math::Vec2 points[3];
    Math::Vec2 center;
    ct::Vector<int> neighbors;
};

float distanceSq(const Math::Vec2& a, const Math::Vec2& b)
{
    const float x = a.x - b.x, y = a.y - b.y;
    return x * x + y * y;
}

float cross(const Math::Vec2& a, const Math::Vec2& b, const Math::Vec2& p)
{
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

bool contains(const Face& face, const Math::Vec2& point)
{
    const float a = cross(face.points[0], face.points[1], point);
    const float b = cross(face.points[1], face.points[2], point);
    const float c = cross(face.points[2], face.points[0], point);
    return (a >= -0.001f && b >= -0.001f && c >= -0.001f) || (a <= 0.001f && b <= 0.001f && c <= 0.001f);
}

bool sharesEdge(const Face& a, const Face& b)
{
    int shared = 0;
    for (int ia = 0; ia < 3; ++ia)
        for (int ib = 0; ib < 3; ++ib)
            if (distanceSq(a.points[ia], b.points[ib]) < 0.001f)
            {
                ++shared;
                break;
            }
    return shared >= 2;
}
} // namespace

NavigationRegion2D::NavigationRegion2D() : Component(Type)
{
}

void NavigationRegion2D::onAwake()
{
    Navigation2D::Register(this);
}

void NavigationRegion2D::onDestroy()
{
    Navigation2D::Unregister(this);
}

void NavigationRegion2D::setPolygon(const Math::Vec2* points, int count)
{
    mPolygon.clear();
    mTriangles.clear();
    if (!points || count < 3)
        return;
    for (int i = 0; i < count; ++i)
        mPolygon.push_back(points[i]);
    ct::Vector<Math::Vec2> baked;
    baked.resize(static_cast<size_t>(count - 2) * 3u);
    const int triangleCount = kx::Triangulate(mPolygon.data(), count, baked.data(), count - 2);
    for (int i = 0; i < triangleCount * 3; ++i)
        mTriangles.push_back(baked[i]);
}

bool NavigationRegion2D::getPath(const Math::Vec2& from, const Math::Vec2& to, ct::Vector<Math::Vec2>& outPath) const
{
    outPath.clear();
    if (mTriangles.empty() || !owner())
        return false;
    const Matrix2D& transform = owner()->globalTransform();
    ct::Vector<Face> faces;
    faces.resize(mTriangles.size() / 3);
    for (size_t i = 0; i < faces.size(); ++i)
    {
        Face& face = faces[i];
        face.points[0] = transform.Transform(mTriangles[i * 3]);
        face.points[1] = transform.Transform(mTriangles[i * 3 + 1]);
        face.points[2] = transform.Transform(mTriangles[i * 3 + 2]);
        face.center = (face.points[0] + face.points[1] + face.points[2]) / 3.0f;
    }
    for (size_t a = 0; a < faces.size(); ++a)
        for (size_t b = a + 1; b < faces.size(); ++b)
            if (sharesEdge(faces[a], faces[b]))
            {
                faces[a].neighbors.push_back(static_cast<int>(b));
                faces[b].neighbors.push_back(static_cast<int>(a));
            }

    int start = -1, end = -1;
    for (size_t i = 0; i < faces.size(); ++i)
    {
        if (contains(faces[i], from))
            start = static_cast<int>(i);
        if (contains(faces[i], to))
            end = static_cast<int>(i);
    }
    if (start < 0 || end < 0)
        return false;
    if (start == end)
    {
        outPath.push_back(from);
        outPath.push_back(to);
        return true;
    }

    const size_t count = faces.size();
    ct::Vector<float> cost, score;
    ct::Vector<int> parent;
    ct::Vector<bool> closed;
    cost.resize(count);
    score.resize(count);
    parent.resize(count);
    closed.resize(count);
    for (size_t i = 0; i < count; ++i)
    {
        cost[i] = std::numeric_limits<float>::max();
        score[i] = std::numeric_limits<float>::max();
        parent[i] = -1;
        closed[i] = false;
    }
    ct::Vector<int> open;
    cost[start] = 0.0f;
    score[start] = std::sqrt(distanceSq(faces[start].center, to));
    open.push_back(start);
    while (!open.empty())
    {
        size_t best = 0;
        for (size_t i = 1; i < open.size(); ++i)
            if (score[open[i]] < score[open[best]])
                best = i;
        const int current = open[best];
        open.erase(open.begin() + best);
        if (current == end)
            break;
        if (closed[current])
            continue;
        closed[current] = true;
        for (int neighbor : faces[current].neighbors)
        {
            if (closed[neighbor])
                continue;
            const float next = cost[current] + std::sqrt(distanceSq(faces[current].center, faces[neighbor].center));
            if (next >= cost[neighbor])
                continue;
            parent[neighbor] = current;
            cost[neighbor] = next;
            score[neighbor] = next + std::sqrt(distanceSq(faces[neighbor].center, to));
            open.push_back(neighbor);
        }
    }
    if (parent[end] < 0)
        return false;
    ct::Vector<int> reverse;
    for (int face = end; face >= 0; face = parent[face])
        reverse.push_back(face);
    outPath.push_back(from);
    for (size_t i = reverse.size() - 1; i-- > 1;)
        outPath.push_back(faces[reverse[i]].center);
    outPath.push_back(to);
    return true;
}
} // namespace k2d
