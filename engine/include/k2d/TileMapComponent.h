#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"

#include <ct/string.hpp>
#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{

    class Texture;
    class Assets;
    class AStarGrid2D;

    // The neighbours a terrain tile joins, as Godot's peering bits: a corner
    // counts only where both sides beside it are of the terrain too.
    enum TerrainBits : unsigned char
    {
        TERRAIN_N = 1,
        TERRAIN_NE = 2,
        TERRAIN_E = 4,
        TERRAIN_SE = 8,
        TERRAIN_S = 16,
        TERRAIN_SW = 32,
        TERRAIN_W = 64,
        TERRAIN_NW = 128
    };

    struct TerrainTile
    {
        int tile;
        int terrain;
        unsigned char bits;
    };

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
        void setAtlasPadding(float x, float y);
        Math::Vec2 atlasPadding() const { return Math::Vec2(mAtlasPaddingX, mAtlasPaddingY); }
        void setAtlasGap(float x, float y);
        Math::Vec2 atlasGap() const { return Math::Vec2(mAtlasGapX, mAtlasGapY); }
        void setTile(int x, int y, int atlasTileId);
        void setCollision(int x, int y, bool solid);
        bool loadTMX(Assets &assets, const char *tmxPath, const char *textureName = "tmx_tiles");
        int getTile(int x, int y) const;
        bool hasCollision(int x, int y) const;
        void clearCollision();
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

        // Autotiling: tiles of the tileset are given a terrain and the
        // neighbours they join; painting a terrain picks, for the cell and the
        // eight around it, the tile whose neighbours match.
        int addTerrain(const ct::String &name);
        void removeTerrain(int terrain);
        int terrainCount() const { return static_cast<int>(mTerrainNames.size()); }
        const ct::String &terrainName(int terrain) const;
        void setTerrainName(int terrain, const ct::String &name);
        // A terrain below 0 takes the tile out of every terrain.
        void setTileTerrain(int tile, int terrain, unsigned char bits);
        bool tileTerrain(int tile, int &terrain, unsigned char &bits) const;
        const ct::Vector<TerrainTile> &terrainTiles() const { return mTerrainTiles; }
        // The terrain of the cell's tile, or -1.
        int terrainAt(int x, int y) const;
        // Below 0 clears the cell; the neighbours follow either way.
        void paintTerrain(int x, int y, int terrain);

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        Texture *mTexture;
        float mCellW;
        float mCellH;
        int mColumns;
        int mRows;
        int mAtlasTilesX;
        float mAtlasPaddingX;
        float mAtlasPaddingY;
        float mAtlasGapX;
        float mAtlasGapY;
        ct::Vector<int> mCells;
        ct::Vector<unsigned char> mCollision;
        float mCullX;
        float mCullY;
        float mCullW;
        float mCullH;
        bool mCullEnabled;
        BlendMode mBlendMode;
        ct::Vector<ct::String> mTerrainNames;
        ct::Vector<TerrainTile> mTerrainTiles;

        void fitTerrainCell(int x, int y);
    };

}
