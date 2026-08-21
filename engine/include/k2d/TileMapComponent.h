#pragma once

#include "k2d/Component.h"

#include <ct/vector.hpp>

namespace k2d
{

    class Texture;

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
        int getTile(int x, int y) const;
        void setCullRect(float x, float y, float width, float height);
        void clearCullRect();

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
