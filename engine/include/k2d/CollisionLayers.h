#pragma once

#include <ct/string.hpp>

namespace k2d
{

    // The project's names for the sixteen bits of a collider's category and
    // mask, as project.k2dproj keeps them under physics.layers.
    static const int kCollisionLayers = 16;

    void SetCollisionLayerName(int layer, const char *name);
    // Empty for a layer with no name.
    const ct::String &CollisionLayerName(int layer);
    // The layer with that name, or -1.
    int CollisionLayerIndex(const char *name);
    void ClearCollisionLayerNames();

}
