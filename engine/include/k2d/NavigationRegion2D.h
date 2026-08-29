#pragma once

#include "k2d/Component.h"

#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{
// A walkable polygon that is baked into a triangle navigation mesh. The
// polygon is local to its owner and may be positioned/rotated/scaled normally.
class NavigationRegion2D final : public Component
{
  public:
    struct Face
    {
        Math::Vec2 points[3];
        Math::Vec2 center;
        int neighbors[3];
        int neighborCount;
    };

    static const ComponentType Type = ComponentType::NavigationRegion;

    NavigationRegion2D();

    void setPolygon(const Math::Vec2* points, int count);
    void setPolygonWithHoles(const Math::Vec2* outline, int outlineCount, const Math::Vec2* const* holes,
                              const int* holeCounts, int holeCount);
    const ct::Vector<Math::Vec2>& polygon() const
    {
        return mPolygon;
    }
    const ct::Vector<ct::Vector<Math::Vec2>>& holes() const
    {
        return mHoles;
    }
    const ct::Vector<Math::Vec2>& triangles() const
    {
        return mTriangles;
    }
    bool valid() const
    {
        return !mTriangles.empty();
    }
    bool getPath(const Math::Vec2& from, const Math::Vec2& to, ct::Vector<Math::Vec2>& outPath) const;
    // True when the world point falls on the walkable mesh. This is what an AI
    // needs before picking a destination, so it does not path to dry land.
    bool containsPoint(const Math::Vec2& point) const;

  protected:
    void onAwake() override;
    void onDestroy() override;

  private:
    // Adjacency and the A* scratch are baked when the mesh changes, not per
    // query: the mesh is immutable between setPolygon calls, and rebuilding a
    // face graph plus six vectors on every path request is the whole cost.
    void bakeFaces();
    // Face containing the point, or the nearest one with the point pulled onto
    // it. Never fails while the mesh has faces.
    int faceNear(const Math::Vec2& localPoint, Math::Vec2& outSnapped) const;

    ct::Vector<Face> mFaces;
    mutable ct::Vector<float> mCost;
    mutable ct::Vector<float> mScore;
    mutable ct::Vector<int> mParent;
    mutable ct::Vector<unsigned char> mClosed;
    mutable ct::Vector<int> mOpen;
    mutable ct::Vector<int> mReverse;
    mutable ct::Vector<int> mCorridor;
    ct::Vector<Math::Vec2> mPolygon;
    ct::Vector<ct::Vector<Math::Vec2>> mHoles;
    ct::Vector<Math::Vec2> mTriangles;
};
} // namespace k2d
