#pragma once

#include "k2d/Component.h"

#include <ct/vector.hpp>

namespace k2d
{

    class Texture;
    class Assets;
    class AStarGrid2D;

    // Tile map component. Roughly follows Godot's TileMapLayer + TileSetAtlasSource
    // (modules/tilemap/tile_map_layer.cpp): the atlas texture is a uniform grid of
    // tiles (cell size), and each map cell stores the atlas tile index (0 = empty,
    // 1..N map to atlas tiles in row-major order). Visible cells are emitted as rect
    // commands on a single render item, so they sort and batch with sprites.
    class TileMapComponent : public Component
    {
    public:
        static const ComponentType Type = ComponentType::TileMap;

        TileMapComponent();

        void setTexture(Texture *texture);
        void setCellSize(float width, float height);
        void setMapSize(int columns, int rows);
        void setAtlasTilesX(int tilesX);
        void setTile(int x, int y, int atlasTileId);
        bool loadTMX(Assets &assets, const char *tmxPath, const char *textureName = "tmx_tiles");
        int getTile(int x, int y) const;
        void setCullRect(float x, float y, float width, float height);
        void clearCullRect();

        // Configures `grid` to match this tilemap's dimensions/cell size and
        // marks a cell solid iff its atlas tile id is one of `solidTileIds`
        // (e.g. wall/obstacle tiles in your atlas). Empty cells (id 0) and any
        // tile id not listed stay walkable -- this deliberately does NOT
        // assume "no tile drawn" means "blocked", since blank cells are
        // commonly walkable floor with no decoration. Pass a wall-tile id
        // list explicitly; call again after editing tiles to refresh the
        // grid. Positions returned by the grid (GetPointPosition/GetPointPath)
        // are in this component's local space, matching where tiles are
        // drawn -- transform by owner()->globalTransform() for world space.
        void buildPathfindingGrid(AStarGrid2D &grid, const int *solidTileIds = nullptr,
                                   int solidTileIdCount = 0) const;

        int columns() const { return mColumns; }
        int rows() const { return mRows; }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        Texture *mTexture;
        float mCellW;
        float mCellH;
        int mColumns;
        int mRows;
        int mAtlasTilesX;
        ct::Vector<int> mCells;
        float mCullX;
        float mCullY;
        float mCullW;
        float mCullH;
        bool mCullEnabled;
    };

}
