#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"
#include "k2d/TileSet.h"

#include <ct/string.hpp>
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
        Texture *texture() const { return set().texture; }
        void setCellSize(float width, float height);
        float cellWidth() const { return set().cellWidth; }
        float cellHeight() const { return set().cellHeight; }
        void setMapSize(int columns, int rows);
        void setAtlasTilesX(int tilesX);
        int atlasTilesX() const { return set().atlasTilesX; }
        void setAtlasPadding(float x, float y);
        Math::Vec2 atlasPadding() const { return Math::Vec2(set().paddingX, set().paddingY); }
        void setAtlasGap(float x, float y);
        Math::Vec2 atlasGap() const { return Math::Vec2(set().gapX, set().gapY); }
        void setTile(int x, int y, int atlasTileId);
        void setCollision(int x, int y, bool solid);
        bool loadTMX(Assets &assets, const char *tmxPath, const char *textureName = "tmx_tiles");
        int getTile(int x, int y) const;
        // Solid by the cell's own flag or by its tile's.
        bool hasCollision(int x, int y) const;
        // The cell's own flag only, as painted.
        bool cellCollision(int x, int y) const;
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
        int terrainCount() const { return static_cast<int>(set().terrainNames.size()); }
        const ct::String &terrainName(int terrain) const;
        void setTerrainName(int terrain, const ct::String &name);
        // A terrain below 0 takes the tile out of every terrain.
        void setTileTerrain(int tile, int terrain, unsigned char bits);
        bool tileTerrain(int tile, int &terrain, unsigned char &bits) const;
        const ct::Vector<TerrainTile> &terrainTiles() const { return set().terrainTiles; }
        // The terrain of the cell's tile, or -1.
        int terrainAt(int x, int y) const;
        // Below 0 clears the cell; the neighbours follow either way.
        void paintTerrain(int x, int y, int terrain);

        // The texture, the cutting, the terrains, the solid and the animated
        // tiles: the map's own, or a .k2dtileset shared with other maps. Every
        // setter above writes to the one in use. Leaving a shared set keeps a
        // copy of it as the map's own.
        const TileSet &tileSet() const { return set(); }
        TileSet &editTileSet();
        void setSharedTileSet(TileSet *shared);
        TileSet *sharedTileSet() const { return mSharedSet; }
        // A tile of the tileset that every cell holding it makes solid.
        void setTileSolid(int tile, bool solid);
        bool tileSolid(int tile) const { return set().tileSolid(tile); }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        const TileSet &set() const { return mSharedSet ? *mSharedSet : mOwnSet; }

        int mColumns;
        int mRows;
        ct::Vector<int> mCells;
        ct::Vector<unsigned char> mCollision;
        float mCullX;
        float mCullY;
        float mCullW;
        float mCullH;
        bool mCullEnabled;
        BlendMode mBlendMode;
        TileSet mOwnSet;
        TileSet *mSharedSet = nullptr;

        void fitTerrainCell(int x, int y);
    };

}
