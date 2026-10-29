#include "k2d/TileMapComponent.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"
#include "k2d/Assets.h"
#include "k2d/FileBuffer.h"
#include "k2d/FileSystem.h"
#include "k2d/AStarGrid2D.h"

#include <ct/string.hpp>

#include <cmath>
#include <cstdlib>
#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace
{
static ct::String tmxAttribute(const ct::String& text, const char* name)
{
    ct::String key(name);
    key += "=\"";
    std::size_t begin = text.find(key);
    if (begin == ct::String::npos)
        return ct::String();
    begin += key.size();
    std::size_t end = text.find('"', begin);
    return end == ct::String::npos ? ct::String() : text.substr(begin, end - begin);
}

static int tmxInt(const ct::String& text, const char* name, int fallback)
{
    ct::String value = tmxAttribute(text, name);
    if (value.empty())
        return fallback;
    return std::atoi(value.c_str());
}
} // namespace

namespace k2d
{

TileMapComponent::TileMapComponent()
    : Component(Type, ComponentEventRender), mTexture(nullptr), mCellW(32.0f), mCellH(32.0f), mColumns(0), mRows(0),
      mAtlasTilesX(1), mAtlasPaddingX(0.0f), mAtlasPaddingY(0.0f), mAtlasGapX(0.0f), mAtlasGapY(0.0f), mCullX(0.0f),
      mCullY(0.0f), mCullW(0.0f), mCullH(0.0f), mCullEnabled(false), mBlendMode(BLEND_MIX)
{
}

void TileMapComponent::setTexture(Texture* texture)
{
    mTexture = texture;
}

void TileMapComponent::setCellSize(float width, float height)
{
    mCellW = width;
    mCellH = height;
}

void TileMapComponent::setMapSize(int columns, int rows)
{
    columns = columns > 0 ? columns : 0;
    rows = rows > 0 ? rows : 0;
    ct::Vector<int> cells;
    ct::Vector<unsigned char> collision;
    cells.resize(columns * rows, 0);
    collision.resize(columns * rows, 0);
    const int copyColumns = columns < mColumns ? columns : mColumns;
    const int copyRows = rows < mRows ? rows : mRows;
    for (int y = 0; y < copyRows; ++y)
        for (int x = 0; x < copyColumns; ++x)
        {
            cells[y * columns + x] = mCells[y * mColumns + x];
            collision[y * columns + x] = mCollision[y * mColumns + x];
        }
    mColumns = columns;
    mRows = rows;
    mCells = ct::detail::move(cells);
    mCollision = ct::detail::move(collision);
}

void TileMapComponent::setAtlasTilesX(int tilesX)
{
    mAtlasTilesX = tilesX > 0 ? tilesX : 1;
}

void TileMapComponent::setAtlasPadding(float x, float y)
{
    mAtlasPaddingX = x > 0.0f ? x : 0.0f;
    mAtlasPaddingY = y > 0.0f ? y : 0.0f;
}

void TileMapComponent::setAtlasGap(float x, float y)
{
    mAtlasGapX = x > 0.0f ? x : 0.0f;
    mAtlasGapY = y > 0.0f ? y : 0.0f;
}

void TileMapComponent::setTile(int x, int y, int atlasTileId)
{
    if (x < 0 || x >= mColumns || y < 0 || y >= mRows)
        return;
    mCells[y * mColumns + x] = atlasTileId;
}

void TileMapComponent::setCollision(int x, int y, bool solid)
{
    if (x < 0 || x >= mColumns || y < 0 || y >= mRows)
        return;
    mCollision[y * mColumns + x] = solid ? 1 : 0;
}

int TileMapComponent::addTerrain(const ct::String& name)
{
    mTerrainNames.push_back(name);
    return static_cast<int>(mTerrainNames.size()) - 1;
}

void TileMapComponent::removeTerrain(int terrain)
{
    if (terrain < 0 || terrain >= terrainCount())
        return;
    mTerrainNames.erase(mTerrainNames.begin() + terrain);
    for (std::size_t i = 0; i < mTerrainTiles.size();)
    {
        if (mTerrainTiles[i].terrain == terrain)
        {
            mTerrainTiles.erase(mTerrainTiles.begin() + i);
            continue;
        }
        if (mTerrainTiles[i].terrain > terrain)
            --mTerrainTiles[i].terrain;
        ++i;
    }
}

const ct::String& TileMapComponent::terrainName(int terrain) const
{
    static const ct::String none;
    return terrain >= 0 && terrain < terrainCount() ? mTerrainNames[terrain] : none;
}

void TileMapComponent::setTerrainName(int terrain, const ct::String& name)
{
    if (terrain >= 0 && terrain < terrainCount())
        mTerrainNames[terrain] = name;
}

void TileMapComponent::setTileTerrain(int tile, int terrain, unsigned char bits)
{
    for (std::size_t i = 0; i < mTerrainTiles.size(); ++i)
        if (mTerrainTiles[i].tile == tile)
        {
            if (terrain < 0 || terrain >= terrainCount())
                mTerrainTiles.erase(mTerrainTiles.begin() + i);
            else
            {
                mTerrainTiles[i].terrain = terrain;
                mTerrainTiles[i].bits = bits;
            }
            return;
        }
    if (tile > 0 && terrain >= 0 && terrain < terrainCount())
        mTerrainTiles.push_back(TerrainTile{tile, terrain, bits});
}

bool TileMapComponent::tileTerrain(int tile, int& terrain, unsigned char& bits) const
{
    for (const TerrainTile& entry : mTerrainTiles)
        if (entry.tile == tile)
        {
            terrain = entry.terrain;
            bits = entry.bits;
            return true;
        }
    return false;
}

int TileMapComponent::terrainAt(int x, int y) const
{
    int terrain = -1;
    unsigned char bits = 0;
    return tileTerrain(getTile(x, y), terrain, bits) ? terrain : -1;
}

void TileMapComponent::paintTerrain(int x, int y, int terrain)
{
    if (x < 0 || x >= mColumns || y < 0 || y >= mRows)
        return;
    int tile = 0;
    for (const TerrainTile& entry : mTerrainTiles)
        if (entry.terrain == terrain)
        {
            tile = entry.tile;
            break;
        }
    if (terrain >= 0 && tile == 0)
        return;
    setTile(x, y, tile);
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            fitTerrainCell(x + dx, y + dy);
}

void TileMapComponent::fitTerrainCell(int x, int y)
{
    const int terrain = terrainAt(x, y);
    if (terrain < 0)
        return;
    auto same = [this, terrain](int cx, int cy) { return terrainAt(cx, cy) == terrain; };
    const bool n = same(x, y - 1), e = same(x + 1, y), s = same(x, y + 1), w = same(x - 1, y);
    unsigned char want = (n ? TERRAIN_N : 0) | (e ? TERRAIN_E : 0) | (s ? TERRAIN_S : 0) | (w ? TERRAIN_W : 0);
    bool corners = false;
    for (const TerrainTile& entry : mTerrainTiles)
        if (entry.terrain == terrain && (entry.bits & (TERRAIN_NE | TERRAIN_SE | TERRAIN_SW | TERRAIN_NW)))
            corners = true;
    if (corners)
    {
        if (n && e && same(x + 1, y - 1))
            want |= TERRAIN_NE;
        if (s && e && same(x + 1, y + 1))
            want |= TERRAIN_SE;
        if (s && w && same(x - 1, y + 1))
            want |= TERRAIN_SW;
        if (n && w && same(x - 1, y - 1))
            want |= TERRAIN_NW;
    }
    int best = 0;
    int bestDistance = 9;
    for (const TerrainTile& entry : mTerrainTiles)
    {
        if (entry.terrain != terrain)
            continue;
        int distance = 0;
        for (unsigned char diff = static_cast<unsigned char>(entry.bits ^ want); diff; diff &= diff - 1)
            ++distance;
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = entry.tile;
        }
    }
    if (best > 0)
        setTile(x, y, best);
}

bool TileMapComponent::loadTMX(Assets& assets, const char* tmxPath, const char* textureName)
{
    if (!tmxPath)
        return false;

    FileBuffer file;
    if (!FileSystem::Instance().LoadFile(tmxPath, file, true))
        return false;
    const ct::String xml(file.Text(), file.Size());

    std::size_t mapStart = xml.find("<map");
    std::size_t tilesetStart = xml.find("<tileset", mapStart);
    std::size_t imageStart = xml.find("<image", tilesetStart);
    std::size_t layerStart = xml.find("<layer", imageStart);
    std::size_t dataStart = xml.find("<data", layerStart);
    std::size_t dataEnd = xml.find("</data>", dataStart);
    if (mapStart == ct::String::npos || tilesetStart == ct::String::npos || imageStart == ct::String::npos ||
        layerStart == ct::String::npos || dataStart == ct::String::npos || dataEnd == ct::String::npos)
        return false;

    const std::size_t mapTagEnd = xml.find('>', mapStart);
    const std::size_t tilesetTagEnd = xml.find('>', tilesetStart);
    const std::size_t imageTagEnd = xml.find('>', imageStart);
    const std::size_t layerTagEnd = xml.find('>', layerStart);
    if (mapTagEnd == ct::String::npos || tilesetTagEnd == ct::String::npos || imageTagEnd == ct::String::npos ||
        layerTagEnd == ct::String::npos)
        return false;

    const ct::String mapTag = xml.substr(mapStart, mapTagEnd - mapStart + 1);
    const ct::String tilesetTag = xml.substr(tilesetStart, tilesetTagEnd - tilesetStart + 1);
    const ct::String imageTag = xml.substr(imageStart, imageTagEnd - imageStart + 1);
    const ct::String layerTag = xml.substr(layerStart, layerTagEnd - layerStart + 1);

    const int columns = tmxInt(mapTag, "width", 0);
    const int rows = tmxInt(mapTag, "height", 0);
    const int cellW = tmxInt(mapTag, "tilewidth", 0);
    const int cellH = tmxInt(mapTag, "tileheight", 0);
    const int firstGid = tmxInt(tilesetTag, "firstgid", 1);
    const int atlasW = tmxInt(imageTag, "width", 0);
    const int atlasH = tmxInt(imageTag, "height", 0);
    if (columns <= 0 || rows <= 0 || cellW <= 0 || cellH <= 0 || firstGid <= 0 || atlasW <= 0 || atlasH <= 0)
        return false;

    ct::String imagePath = tmxAttribute(imageTag, "source");
    if (imagePath.empty())
        return false;
    const ct::String tmx(tmxPath);
    std::size_t slash = tmx.find_last_of("/\\");
    ct::String texturePath = slash == ct::String::npos ? imagePath : tmx.substr(0, slash + 1) + imagePath;
    Texture* texture = assets.LoadTexture(textureName, texturePath.c_str());
    if (!texture)
        return false;

    setTexture(texture);
    setCellSize((float)cellW, (float)cellH);
    setMapSize(columns, rows);
    const int margin = tmxInt(tilesetTag, "margin", 0);
    const int spacing = tmxInt(tilesetTag, "spacing", 0);
    setAtlasPadding((float)margin, (float)margin);
    setAtlasGap((float)spacing, (float)spacing);
    setAtlasTilesX((atlasW - margin * 2 + spacing) / (cellW + spacing));

    const std::size_t csvStart = xml.find('>', dataStart) + 1;
    const ct::Vector<ct::String> tokens = xml.substr(csvStart, dataEnd - csvStart).split(',');
    int index = 0;
    for (const ct::String& token : tokens)
    {
        const ct::String value = token.trimmed();
        if (value.empty())
            continue;

        unsigned long rawGid = std::strtoul(value.c_str(), nullptr, 10);
        unsigned long gid = rawGid & 0x1FFFFFFFul;
        int atlasId = gid == 0 ? 0 : (int)gid - firstGid + 1;
        if (index < columns * rows)
            mCells[index++] = atlasId > 0 ? atlasId : 0;
    }

    return index == columns * rows;
}

int TileMapComponent::getTile(int x, int y) const
{
    if (x < 0 || x >= mColumns || y < 0 || y >= mRows)
        return 0;
    return mCells[y * mColumns + x];
}

bool TileMapComponent::hasCollision(int x, int y) const
{
    if (x < 0 || x >= mColumns || y < 0 || y >= mRows)
        return false;
    return mCollision[y * mColumns + x] != 0;
}

void TileMapComponent::clearCollision()
{
    for (size_t i = 0; i < mCollision.size(); ++i)
        mCollision[i] = 0;
}

void TileMapComponent::setCullRect(float x, float y, float width, float height)
{
    mCullX = x;
    mCullY = y;
    mCullW = width;
    mCullH = height;
    mCullEnabled = true;
}

void TileMapComponent::clearCullRect()
{
    mCullEnabled = false;
}

void TileMapComponent::buildPathfindingGrid(AStarGrid2D& grid, const int* solidTileIds, int solidTileIdCount) const
{
    grid.SetSize(mColumns, mRows);
    grid.SetCellSize(mCellW, mCellH);

    for (int y = 0; y < mRows; ++y)
    {
        for (int x = 0; x < mColumns; ++x)
        {
            bool solid = hasCollision(x, y);
            if (!solid)
            {
                int cell = mCells[y * mColumns + x];
                for (int i = 0; solidTileIds && i < solidTileIdCount; ++i)
                    if (solidTileIds[i] == cell)
                    {
                        solid = true;
                        break;
                    }
            }
            grid.SetSolid(x, y, solid);
        }
    }
}

void TileMapComponent::onRender(RenderQueue& queue)
{
    if (!mTexture || mCells.empty())
        return;

    RenderItem& item = queue.AddItem(owner()->zIndex());
    item.xform = owner()->globalTransform();
    item.blendMode = mBlendMode;

    int minX = 0;
    int minY = 0;
    int maxX = mColumns - 1;
    int maxY = mRows - 1;

    if (mCullEnabled)
    {
        minX = (int)floorf(mCullX / mCellW);
        minY = (int)floorf(mCullY / mCellH);
        maxX = (int)floorf((mCullX + mCullW) / mCellW);
        maxY = (int)floorf((mCullY + mCullH) / mCellH);
        if (minX < 0)
            minX = 0;
        if (minY < 0)
            minY = 0;
        if (maxX >= mColumns)
            maxX = mColumns - 1;
        if (maxY >= mRows)
            maxY = mRows - 1;
    }

    const int atlasW = mTexture->Width();
    const int atlasH = mTexture->Height();

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            int cell = mCells[y * mColumns + x];
            if (cell == 0)
                continue;

            int atlasIndex = cell - 1;
            int tx = atlasIndex % mAtlasTilesX;
            int ty = atlasIndex / mAtlasTilesX;

            RenderCommand rect = RenderCommand::MakeRect(mTexture->Id(), x * mCellW, y * mCellH, mCellW, mCellH);
            rect.srcX = mAtlasPaddingX + tx * (mCellW + mAtlasGapX);
            rect.srcY = mAtlasPaddingY + ty * (mCellH + mAtlasGapY);
            rect.srcW = mCellW;
            rect.srcH = mCellH;
            rect.texWidth = atlasW;
            rect.texHeight = atlasH;
            rect.pivotX = 0.0f;
            rect.pivotY = 0.0f;
            item.commands.push_back(ct::detail::move(rect));
        }
    }
}

} // namespace k2d
