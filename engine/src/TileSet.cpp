#include "k2d/TileSet.h"

#include "k2d/Assets.h"
#include "k2d/FileSystem.h"

namespace k2d
{

    namespace
    {
        ct::Json pair(float x, float y)
        {
            ct::Json json = ct::Json::array();
            json.push_back(ct::Json((double)x));
            json.push_back(ct::Json((double)y));
            return json;
        }
    }

    bool TileSet::tileSolid(int tile) const
    {
        for (int solid : solidTiles)
            if (solid == tile)
                return true;
        return false;
    }

    void TileSet::setTileSolid(int tile, bool solid)
    {
        for (std::size_t i = 0; i < solidTiles.size(); ++i)
            if (solidTiles[i] == tile)
            {
                if (!solid)
                    solidTiles.erase(solidTiles.begin() + i);
                return;
            }
        if (solid && tile > 0)
            solidTiles.push_back(tile);
    }

    const TileAnimation *TileSet::animation(int tile) const
    {
        for (const TileAnimation &entry : animations)
            if (entry.tile == tile)
                return &entry;
        return nullptr;
    }

    void TileSet::setAnimation(int tile, const ct::Vector<int> &frames, float fps)
    {
        for (std::size_t i = 0; i < animations.size(); ++i)
            if (animations[i].tile == tile)
            {
                if (frames.empty())
                    animations.erase(animations.begin() + i);
                else
                {
                    animations[i].frames = frames;
                    animations[i].fps = fps;
                }
                return;
            }
        if (frames.empty() || tile <= 0)
            return;
        TileAnimation entry;
        entry.tile = tile;
        entry.frames = frames;
        entry.fps = fps;
        animations.push_back(entry);
    }

    int TileSet::frameAt(int tile, double seconds) const
    {
        if (animations.empty())
            return tile;
        const TileAnimation *entry = animation(tile);
        if (!entry || entry->frames.empty() || entry->fps <= 0.0f)
            return tile;
        const long long frame = static_cast<long long>(seconds * entry->fps);
        return entry->frames[static_cast<std::size_t>(frame % static_cast<long long>(entry->frames.size()))];
    }

    void TileSet::write(ct::Json &data, Assets *assets) const
    {
        if (assets)
            if (const char *name = assets->FindTextureName(texture))
                data.set("texture", ct::Json(name));
        data.set("cellWidth", ct::Json((double)cellWidth));
        data.set("cellHeight", ct::Json((double)cellHeight));
        data.set("atlasTilesX", ct::Json(atlasTilesX));
        data.set("atlasPadding", pair(paddingX, paddingY));
        data.set("atlasGap", pair(gapX, gapY));
        if (!terrainNames.empty())
        {
            ct::Json terrains = ct::Json::array();
            for (std::size_t t = 0; t < terrainNames.size(); ++t)
            {
                ct::Json terrain = ct::Json::object();
                terrain.set("name", ct::Json(terrainNames[t].c_str()));
                ct::Json tiles = ct::Json::array();
                for (const TerrainTile &entry : terrainTiles)
                    if (entry.terrain == static_cast<int>(t))
                    {
                        ct::Json tile = ct::Json::array();
                        tile.push_back(ct::Json(entry.tile));
                        tile.push_back(ct::Json((int)entry.bits));
                        tiles.push_back(tile);
                    }
                terrain.set("tiles", tiles);
                terrains.push_back(terrain);
            }
            data.set("terrains", terrains);
        }
        if (!solidTiles.empty())
        {
            ct::Json solid = ct::Json::array();
            for (int tile : solidTiles)
                solid.push_back(ct::Json(tile));
            data.set("solid", solid);
        }
        if (!animations.empty())
        {
            ct::Json list = ct::Json::array();
            for (const TileAnimation &entry : animations)
            {
                ct::Json animation = ct::Json::object();
                animation.set("tile", ct::Json(entry.tile));
                animation.set("fps", ct::Json((double)entry.fps));
                ct::Json frames = ct::Json::array();
                for (int frame : entry.frames)
                    frames.push_back(ct::Json(frame));
                animation.set("frames", frames);
                list.push_back(animation);
            }
            data.set("animations", list);
        }
    }

    void TileSet::read(const ct::Json &data, Assets *assets)
    {
        if (assets)
            if (const ct::Json *name = data.find("texture"))
            {
                const char *path = name->as_cstr("");
                texture = assets->GetTexture(path);
                if (!texture && path[0])
                    texture = assets->LoadTexture(path, path);
            }
        cellWidth = (float)data["cellWidth"].as_double(32.0);
        cellHeight = (float)data["cellHeight"].as_double(32.0);
        atlasTilesX = (int)data["atlasTilesX"].as_int(1);
        atlasTilesX = atlasTilesX > 0 ? atlasTilesX : 1;
        const ct::Json &padding = data["atlasPadding"];
        const ct::Json &gap = data["atlasGap"];
        paddingX = (float)padding[0].as_double(0.0);
        paddingY = (float)padding[1].as_double(0.0);
        gapX = (float)gap[0].as_double(0.0);
        gapY = (float)gap[1].as_double(0.0);

        terrainNames.clear();
        terrainTiles.clear();
        const ct::Json &terrains = data["terrains"];
        for (std::size_t t = 0; terrains.is_array() && t < terrains.size(); ++t)
        {
            terrainNames.push_back(terrains[t]["name"].as_cstr(""));
            const ct::Json &tiles = terrains[t]["tiles"];
            for (std::size_t i = 0; tiles.is_array() && i < tiles.size(); ++i)
                terrainTiles.push_back(TerrainTile{(int)tiles[i][0].as_int(0), static_cast<int>(t),
                                                   (unsigned char)tiles[i][1].as_int(0)});
        }
        solidTiles.clear();
        const ct::Json &solid = data["solid"];
        for (std::size_t i = 0; solid.is_array() && i < solid.size(); ++i)
            solidTiles.push_back((int)solid[i].as_int(0));
        animations.clear();
        const ct::Json &list = data["animations"];
        for (std::size_t i = 0; list.is_array() && i < list.size(); ++i)
        {
            TileAnimation entry;
            entry.tile = (int)list[i]["tile"].as_int(0);
            entry.fps = (float)list[i]["fps"].as_double(8.0);
            const ct::Json &frames = list[i]["frames"];
            for (std::size_t f = 0; frames.is_array() && f < frames.size(); ++f)
                entry.frames.push_back((int)frames[f].as_int(0));
            if (entry.tile > 0 && !entry.frames.empty())
                animations.push_back(entry);
        }
    }

    bool TileSet::save(const char *path, Assets *assets) const
    {
        ct::Json data = ct::Json::object();
        write(data, assets);
        return path && FileSystem::Instance().SaveTextFile(path, data.dump(2));
    }

}
