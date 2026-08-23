#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"

#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{

    class Texture;
    class Assets;
    class AStarGrid2D;

    class TileMapComponent : public Component
    {
    public:
        static const ComponentType Type = ComponentType::TileMap;

        TileMapComponent();

        void setTexture(Texture *texture);
        Texture *texture() const { return mTexture; }
        void setCellSize(float width, float height);
        float cellWidth() const { return mCellW; }
        float cellHeight() const { return mCellH; }
        void setMapSize(int columns, int rows);
        void setAtlasTilesX(int tilesX);
        int atlasTilesX() const { return mAtlasTilesX; }
        void setTile(int x, int y, int atlasTileId);
        bool loadTMX(Assets &assets, const char *tmxPath, const char *textureName = "tmx_tiles");
        int getTile(int x, int y) const;
        void setCullRect(float x, float y, float width, float height);
        void clearCullRect();
        bool hasCullRect() const { return mCullEnabled; }

        Math::Vec4 cullRect() const { return Math::Vec4(mCullX, mCullY, mCullW, mCullH); }
        void setBlendMode(BlendMode mode) { mBlendMode = mode; }
        BlendMode blendMode() const { return mBlendMode; }

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
        BlendMode mBlendMode;
    };

}