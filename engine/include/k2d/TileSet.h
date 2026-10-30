#pragma once

#include <ct/json.hpp>
#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{

    class Assets;
    class Texture;

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

    // A tile drawn as its frames in turn; the cells keep the first tile's id.
    struct TileAnimation
    {
        int tile = 0;
        ct::Vector<int> frames;
        float fps = 8.0f;
    };

    // What the tiles of a map are: the image they are cut from, how, the
    // terrains they autotile into, which are solid and which are animated. A
    // map has one of its own, or shares one read from a .k2dtileset file.
    class TileSet
    {
    public:
        Texture *texture = nullptr;
        float cellWidth = 32.0f;
        float cellHeight = 32.0f;
        int atlasTilesX = 1;
        float paddingX = 0.0f;
        float paddingY = 0.0f;
        float gapX = 0.0f;
        float gapY = 0.0f;
        ct::Vector<ct::String> terrainNames;
        ct::Vector<TerrainTile> terrainTiles;
        ct::Vector<int> solidTiles;
        ct::Vector<TileAnimation> animations;
        // Changed since it was last read or saved.
        bool dirty = false;

        bool tileSolid(int tile) const;
        void setTileSolid(int tile, bool solid);
        const TileAnimation *animation(int tile) const;
        // No frames take the animation away.
        void setAnimation(int tile, const ct::Vector<int> &frames, float fps);
        // The tile drawn for tile at seconds.
        int frameAt(int tile, double seconds) const;

        // The keys a map and a .k2dtileset file share: texture, cellWidth,
        // cellHeight, atlasTilesX, atlasPadding, atlasGap, terrains, solid,
        // animations.
        void write(ct::Json &data, Assets *assets) const;
        void read(const ct::Json &data, Assets *assets);
        bool save(const char *path, Assets *assets) const;
    };

}
