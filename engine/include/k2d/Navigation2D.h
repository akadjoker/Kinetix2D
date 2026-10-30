#pragma once

#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{
class Scene;
class NavigationRegion2D;

// Scene-level navigation query service. Regions register themselves when
// attached, so this has no dependency on TileMapComponent.
class Navigation2D
{
  public:
    static bool GetPath(const Scene& scene, const Math::Vec2& from, const Math::Vec2& to,
                        ct::Vector<Math::Vec2>& outPath);
    static bool Contains(const Scene& scene, const Math::Vec2& point);

  private:
    friend class NavigationRegion2D;
    static void Register(NavigationRegion2D* region);
    static void Unregister(NavigationRegion2D* region);
};
} // namespace k2d
