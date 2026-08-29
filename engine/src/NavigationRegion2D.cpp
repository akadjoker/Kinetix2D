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

// The corridor's shared edge between two adjacent faces, ordered so that
// `left` is on the left of the direction of travel.
bool portalBetween(const NavigationRegion2D::Face& a, const NavigationRegion2D::Face& b, const Math::Vec2& apex,
                   Math::Vec2& left, Math::Vec2& right)
{
    Math::Vec2 shared[2];
    int count = 0;
    for (int ia = 0; ia < 3 && count < 2; ++ia)
        for (int ib = 0; ib < 3; ++ib)
            if (distanceSq(a.points[ia], b.points[ib]) < 0.001f)
            {
                shared[count++] = a.points[ia];
                break;
            }
    if (count < 2)
        return false;
    left = shared[0];
    right = shared[1];
    if (cross(apex, left, right) < 0.0f)
    {
        left = shared[1];
        right = shared[0];
    }
    return true;
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

namespace
{
bool segmentsCross(const Math::Vec2& a1, const Math::Vec2& a2, const Math::Vec2& b1, const Math::Vec2& b2)
{
    const float d1 = cross(b1, b2, a1);
    const float d2 = cross(b1, b2, a2);
    const float d3 = cross(a1, a2, b1);
    const float d4 = cross(a1, a2, b2);
    return ((d1 > 0.0f) != (d2 > 0.0f)) && ((d3 > 0.0f) != (d4 > 0.0f));
}

// A crossed outline has no inside, so whatever the triangulator makes of it is
// meaningless. Endpoints shared by neighbouring edges are not crossings.
bool outlineCrossesItself(const Math::Vec2* points, int count)
{
    for (int i = 0; i < count; ++i)
    {
        const int iNext = (i + 1) % count;
        for (int j = i + 1; j < count; ++j)
        {
            const int jNext = (j + 1) % count;
            if (j == iNext || jNext == i)
                continue;
            if (segmentsCross(points[i], points[iNext], points[j], points[jNext]))
                return true;
        }
    }
    return false;
}
} // namespace

void NavigationRegion2D::setPolygon(const Math::Vec2* points, int count)
{
    mPolygon.clear();
    mHoles.clear();
    mTriangles.clear();
    mFaces.clear();
    if (!points || count < 3 || outlineCrossesItself(points, count))
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
    if (!outline || outlineCount < 3 || outlineCrossesItself(outline, outlineCount))
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

namespace
{
Math::Vec2 closestOnSegment(const Math::Vec2& a, const Math::Vec2& b, const Math::Vec2& point)
{
    const Math::Vec2 edge = b - a;
    const float lengthSq = edge.x * edge.x + edge.y * edge.y;
    if (lengthSq < 0.000001f)
        return a;
    float t = ((point.x - a.x) * edge.x + (point.y - a.y) * edge.y) / lengthSq;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return a + edge * t;
}
} // namespace

// Godot snaps both ends of a query onto the map before pathing
// (map_get_closest_point). Refusing a point that is a hair outside the mesh is
// what left an agent lost for good: it only takes one corner cut, or a target
// standing against a wall the mesh does not quite reach, and every later
// request fails from the same spot.
int NavigationRegion2D::faceNear(const Math::Vec2& localPoint, Math::Vec2& outSnapped) const
{
    outSnapped = localPoint;
    for (size_t i = 0; i < mFaces.size(); ++i)
        if (contains(mFaces[i], localPoint))
            return static_cast<int>(i);

    int best = -1;
    float bestDistance = 0.0f;
    for (size_t i = 0; i < mFaces.size(); ++i)
    {
        const Face& face = mFaces[i];
        for (int edge = 0; edge < 3; ++edge)
        {
            const Math::Vec2 candidate =
                closestOnSegment(face.points[edge], face.points[(edge + 1) % 3], localPoint);
            const float distance = distanceSq(candidate, localPoint);
            if (best < 0 || distance < bestDistance)
            {
                bestDistance = distance;
                best = static_cast<int>(i);
                outSnapped = candidate;
            }
        }
    }
    return best;
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

    Math::Vec2 snappedFrom = localFrom;
    Math::Vec2 snappedTo = localTo;
    const int start = faceNear(localFrom, snappedFrom);
    const int end = faceNear(localTo, snappedTo);
    if (start < 0 || end < 0)
        return false;
    const Math::Vec2 worldStart = transform.Transform(snappedFrom);
    const Math::Vec2 worldEnd = transform.Transform(snappedTo);
    if (start == end)
    {
        outPath.push_back(worldStart);
        outPath.push_back(worldEnd);
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
    mScore[start] = std::sqrt(distanceSq(mFaces[start].center, snappedTo));
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
            mScore[neighbor] = next + std::sqrt(distanceSq(mFaces[neighbor].center, snappedTo));
            mOpen.push_back(neighbor);
        }
    }
    if (mParent[end] < 0)
        return false;
    mReverse.clear();
    for (int face = end; face >= 0; face = mParent[face])
        mReverse.push_back(face);

    // The corridor of triangles is not the path: emitting their centroids
    // makes the agent zigzag between the middle of each triangle. Pull a
    // string through the portals instead and only turn at real corners.
    mCorridor.clear();
    for (size_t i = mReverse.size(); i-- > 0;)
        mCorridor.push_back(mReverse[i]);

    outPath.push_back(worldStart);

    Math::Vec2 apex = snappedFrom;
    Math::Vec2 portalLeft = snappedFrom;
    Math::Vec2 portalRight = snappedFrom;
    size_t leftIndex = 0;
    size_t rightIndex = 0;

    for (size_t i = 1; i <= mCorridor.size(); ++i)
    {
        Math::Vec2 left = snappedTo;
        Math::Vec2 right = snappedTo;
        if (i < mCorridor.size() &&
            !portalBetween(mFaces[mCorridor[i - 1]], mFaces[mCorridor[i]], apex, left, right))
            continue;

        if (cross(apex, portalRight, right) <= 0.0f)
        {
            if (distanceSq(apex, portalRight) < 0.001f || cross(apex, portalLeft, right) > 0.0f)
            {
                portalRight = right;
                rightIndex = i;
            }
            else
            {
                outPath.push_back(transform.Transform(portalLeft));
                apex = portalLeft;
                portalRight = apex;
                portalLeft = apex;
                i = leftIndex;
                rightIndex = leftIndex;
                continue;
            }
        }

        if (cross(apex, portalLeft, left) >= 0.0f)
        {
            if (distanceSq(apex, portalLeft) < 0.001f || cross(apex, portalRight, left) < 0.0f)
            {
                portalLeft = left;
                leftIndex = i;
            }
            else
            {
                outPath.push_back(transform.Transform(portalRight));
                apex = portalRight;
                portalLeft = apex;
                portalRight = apex;
                i = rightIndex;
                leftIndex = rightIndex;
                continue;
            }
        }
    }

    outPath.push_back(worldEnd);

    // The funnel can leave a vertex that lies on the segment it joins, which
    // is a turn the agent does not need to make.
    size_t kept = 1;
    for (size_t i = 1; i + 1 < outPath.size(); ++i)
    {
        const Math::Vec2& previous = outPath[kept - 1];
        const Math::Vec2& current = outPath[i];
        const Math::Vec2& next = outPath[i + 1];
        const float area = (current.x - previous.x) * (next.y - previous.y) -
                           (current.y - previous.y) * (next.x - previous.x);
        if (std::fabs(area) < 0.01f)
            continue;
        outPath[kept++] = current;
    }
    if (outPath.size() > 1)
        outPath[kept++] = outPath[outPath.size() - 1];
    while (outPath.size() > kept)
        outPath.pop_back();
    return true;
}

} // namespace k2d
