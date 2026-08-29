#include "k2d/NavigationRegion2D.h"

#include "k2d/GameObject.h"
#include "k2d/Navigation2D.h"

#include "k2d/Triangulate2D.h"

#include <cmath>
#include <limits>

namespace k2d
{
namespace
{

float distanceSq(const Math::Vec2& a, const Math::Vec2& b)
{
    const float x = a.x - b.x, y = a.y - b.y;
    return x * x + y * y;
}

float cross(const Math::Vec2& a, const Math::Vec2& b, const Math::Vec2& p)
{
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

bool contains(const NavigationRegion2D::Face& face, const Math::Vec2& point)
{
    const float a = cross(face.points[0], face.points[1], point);
    const float b = cross(face.points[1], face.points[2], point);
    const float c = cross(face.points[2], face.points[0], point);
    // Both windings must be accepted because poly2tri does not normalise its
    // output, but a near-degenerate triangle satisfies BOTH branches over an
    // unbounded strip along its supporting line, so guard on real area first.
    const float twiceArea = a + b + c;
    if (twiceArea > -0.003f && twiceArea < 0.003f)
        return false;
    return (a >= -0.001f && b >= -0.001f && c >= -0.001f) || (a <= 0.001f && b <= 0.001f && c <= 0.001f);
}

bool sharesEdge(const NavigationRegion2D::Face& a, const NavigationRegion2D::Face& b)
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
    mHoles.clear();
    mTriangles.clear();
    mFaces.clear();
    if (!points || count < 3)
        return;
    for (int i = 0; i < count; ++i)
        mPolygon.push_back(points[i]);
    ct::Vector<Math::Vec2> baked;
    baked.resize(static_cast<size_t>(count - 2) * 3u);
    const int triangleCount = Triangulate(mPolygon.data(), count, baked.data(), count - 2);
    for (int i = 0; i < triangleCount * 3; ++i)
        mTriangles.push_back(baked[i]);
    bakeFaces();
}

void NavigationRegion2D::setPolygonWithHoles(const Math::Vec2* outline, int outlineCount,
                                              const Math::Vec2* const* holes, const int* holeCounts, int holeCount)
{
    mPolygon.clear();
    mHoles.clear();
    mTriangles.clear();
    mFaces.clear();
    if (!outline || outlineCount < 3)
        return;
    for (int i = 0; i < outlineCount; ++i)
        mPolygon.push_back(outline[i]);

    for (int h = 0; h < holeCount; ++h)
    {
        if (!holes[h] || holeCounts[h] < 3)
            continue;
        ct::Vector<Math::Vec2> hole;
        for (int i = 0; i < holeCounts[h]; ++i)
            hole.push_back(holes[h][i]);
        mHoles.push_back(hole);
    }

    if (mHoles.empty())
    {
        ct::Vector<Math::Vec2> baked;
        baked.resize(static_cast<size_t>(outlineCount - 2) * 3u);
        const int triangleCount = Triangulate(mPolygon.data(), outlineCount, baked.data(), outlineCount - 2);
        for (int i = 0; i < triangleCount * 3; ++i)
            mTriangles.push_back(baked[i]);
        return;
    }

    ct::Vector<const Math::Vec2*> holePtrs;
    ct::Vector<int> holePointCounts;
    int holePointTotal = 0;
    for (size_t h = 0; h < mHoles.size(); ++h)
    {
        holePtrs.push_back(mHoles[h].data());
        holePointCounts.push_back((int)mHoles[h].size());
        holePointTotal += (int)mHoles[h].size();
    }

    const int maxTriangles = outlineCount + holePointTotal - 2 + 2 * (int)mHoles.size();
    if (maxTriangles <= 0)
        return;
    ct::Vector<Math::Vec2> baked;
    baked.resize(static_cast<size_t>(maxTriangles) * 3u);
    const int triangleCount = Triangulate(mPolygon.data(), outlineCount, holePtrs.data(), holePointCounts.data(),
                                           (int)mHoles.size(), baked.data(), maxTriangles);
    for (int i = 0; i < triangleCount * 3; ++i)
        mTriangles.push_back(baked[i]);
    bakeFaces();
}

bool NavigationRegion2D::containsPoint(const Math::Vec2& point) const
{
    if (mFaces.empty() || !owner())
        return false;
    const Math::Vec2 local = owner()->globalTransform().AffineInverse().Transform(point);
    for (size_t i = 0; i < mFaces.size(); ++i)
        if (contains(mFaces[i], local))
            return true;
    return false;
}

void NavigationRegion2D::bakeFaces()
{
    mFaces.clear();
    const size_t triangleCount = mTriangles.size() / 3;
    mFaces.resize(triangleCount);
    for (size_t i = 0; i < triangleCount; ++i)
    {
        Face& face = mFaces[i];
        face.points[0] = mTriangles[i * 3];
        face.points[1] = mTriangles[i * 3 + 1];
        face.points[2] = mTriangles[i * 3 + 2];
        face.center = (face.points[0] + face.points[1] + face.points[2]) / 3.0f;
        face.neighborCount = 0;
    }
    for (size_t a = 0; a < mFaces.size(); ++a)
        for (size_t b = a + 1; b < mFaces.size(); ++b)
            if (sharesEdge(mFaces[a], mFaces[b]))
            {
                if (mFaces[a].neighborCount < 3)
                    mFaces[a].neighbors[mFaces[a].neighborCount++] = static_cast<int>(b);
                if (mFaces[b].neighborCount < 3)
                    mFaces[b].neighbors[mFaces[b].neighborCount++] = static_cast<int>(a);
            }

    mCost.resize(triangleCount);
    mScore.resize(triangleCount);
    mParent.resize(triangleCount);
    mClosed.resize(triangleCount);
}

bool NavigationRegion2D::getPath(const Math::Vec2& from, const Math::Vec2& to, ct::Vector<Math::Vec2>& outPath) const
{
    outPath.clear();
    if (mFaces.empty() || !owner())
        return false;

    // The faces are baked in local space, so the query moves into that space
    // instead of transforming every face on every call.
    const Matrix2D& transform = owner()->globalTransform();
    const Matrix2D inverse = transform.AffineInverse();
    const Math::Vec2 localFrom = inverse.Transform(from);
    const Math::Vec2 localTo = inverse.Transform(to);

    int start = -1, end = -1;
    for (size_t i = 0; i < mFaces.size(); ++i)
    {
        if (contains(mFaces[i], localFrom))
            start = static_cast<int>(i);
        if (contains(mFaces[i], localTo))
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

    const size_t count = mFaces.size();
    for (size_t i = 0; i < count; ++i)
    {
        mCost[i] = std::numeric_limits<float>::max();
        mScore[i] = std::numeric_limits<float>::max();
        mParent[i] = -1;
        mClosed[i] = 0;
    }
    mOpen.clear();
    mCost[start] = 0.0f;
    mScore[start] = std::sqrt(distanceSq(mFaces[start].center, localTo));
    mOpen.push_back(start);
    while (!mOpen.empty())
    {
        size_t best = 0;
        for (size_t i = 1; i < mOpen.size(); ++i)
            if (mScore[mOpen[i]] < mScore[mOpen[best]])
                best = i;
        const int current = mOpen[best];
        mOpen.erase(mOpen.begin() + best);
        if (current == end)
            break;
        if (mClosed[current])
            continue;
        mClosed[current] = 1;
        for (int n = 0; n < mFaces[current].neighborCount; ++n)
        {
            const int neighbor = mFaces[current].neighbors[n];
            if (mClosed[neighbor])
                continue;
            const float next = mCost[current] + std::sqrt(distanceSq(mFaces[current].center, mFaces[neighbor].center));
            if (next >= mCost[neighbor])
                continue;
            mParent[neighbor] = current;
            mCost[neighbor] = next;
            mScore[neighbor] = next + std::sqrt(distanceSq(mFaces[neighbor].center, localTo));
            mOpen.push_back(neighbor);
        }
    }
    if (mParent[end] < 0)
        return false;
    mReverse.clear();
    for (int face = end; face >= 0; face = mParent[face])
        mReverse.push_back(face);
    outPath.push_back(from);
    for (size_t i = mReverse.size() - 1; i-- > 1;)
        outPath.push_back(transform.Transform(mFaces[mReverse[i]].center));
    outPath.push_back(to);
    return true;
}

} // namespace k2d
