#pragma once

#include "k2d/Collider2D.h"

namespace k2d
{

    class TileMapComponent;

    // Adds one box shape per merged run of solid cells from the sibling
    // TileMapComponent. Lives on the same GameObject as the RigidBody2D it
    // feeds, so it moves and rotates with that body like any other collider.
    class TileMapCollider2D : public Collider2D
    {
    public:
        TileMapCollider2D();

        int addTo(RigidBody2D &body, float density, float scaleX, float scaleY) const override;

    private:
        TileMapComponent *findTileMap() const;
    };

    template <> struct ComponentMatch<TileMapCollider2D>
    {
        static bool test(const Component *component)
        {
            return dynamic_cast<const TileMapCollider2D *>(component) != nullptr;
        }
    };

}
