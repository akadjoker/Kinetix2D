#include "k2d/TileMapComponent.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"
#include "k2d/Assets.h"
#include "k2d/FileSystem.h"
#include "k2d/AStarGrid2D.h"

#include <ct/string.hpp>

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace
{
    static std::string tmxAttribute(const std::string &text, const char *name)
    {
        std::string key = std::string(name) + "=\"";
        std::size_t begin = text.find(key);
        if (begin == std::string::npos)
            return std::string();
        begin += key.size();
        std::size_t end = text.find('"', begin);
        return end == std::string::npos ? std::string() : text.substr(begin, end - begin);
    }

    static int tmxInt(const std::string &text, const char *name, int fallback)
    {
        std::string value = tmxAttribute(text, name);
        if (value.empty())
            return fallback;
        return std::atoi(value.c_str());
    }
}

namespace k2d
{

    TileMapComponent::TileMapComponent()
        : Component(Type, ComponentEventRender), mTexture(nullptr),
          mCellW(32.0f), mCellH(32.0f), mColumns(0), mRows(0), mAtlasTilesX(1),
          mCullX(0.0f), mCullY(0.0f), mCullW(0.0f), mCullH(0.0f), mCullEnabled(false),
          mBlendMode(BLEND_MIX)
    {
    }

    void TileMapComponent::setTexture(Texture *texture)
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
        mColumns = columns;
        mRows = rows;
        mCells.resize(columns * rows, 0);
    }

    void TileMapComponent::setAtlasTilesX(int tilesX)
    {
        mAtlasTilesX = tilesX > 0 ? tilesX : 1;
    }

    void TileMapComponent::setTile(int x, int y, int atlasTileId)
    {
        if (x < 0 || x >= mColumns || y < 0 || y >= mRows)
            return;
        mCells[y * mColumns + x] = atlasTileId;
    }

    bool TileMapComponent::loadTMX(Assets &assets, const char *tmxPath, const char *textureName)
    {
        if (!tmxPath)
            return false;

        ct::String resolvedPath;
        if (!FileSystem::Instance().Resolve(tmxPath, resolvedPath))
            resolvedPath = tmxPath;
        const char *tmx = resolvedPath.c_str();

        std::ifstream file(tmx);
        if (!file.is_open())
            return false;

        std::stringstream stream;
        stream << file.rdbuf();
        const std::string xml = stream.str();

        std::size_t mapStart = xml.find("<map");
        std::size_t tilesetStart = xml.find("<tileset", mapStart);
        std::size_t imageStart = xml.find("<image", tilesetStart);
        std::size_t layerStart = xml.find("<layer", imageStart);
        std::size_t dataStart = xml.find("<data", layerStart);
        std::size_t dataEnd = xml.find("</data>", dataStart);
        if (mapStart == std::string::npos || tilesetStart == std::string::npos ||
            imageStart == std::string::npos || layerStart == std::string::npos ||
            dataStart == std::string::npos || dataEnd == std::string::npos)
            return false;

        const std::size_t mapTagEnd = xml.find('>', mapStart);
        const std::size_t tilesetTagEnd = xml.find('>', tilesetStart);
        const std::size_t imageTagEnd = xml.find('>', imageStart);
        const std::size_t layerTagEnd = xml.find('>', layerStart);
        if (mapTagEnd == std::string::npos || tilesetTagEnd == std::string::npos ||
            imageTagEnd == std::string::npos || layerTagEnd == std::string::npos)
            return false;

        const std::string mapTag = xml.substr(mapStart, mapTagEnd - mapStart + 1);
        const std::string tilesetTag = xml.substr(tilesetStart, tilesetTagEnd - tilesetStart + 1);
        const std::string imageTag = xml.substr(imageStart, imageTagEnd - imageStart + 1);
        const std::string layerTag = xml.substr(layerStart, layerTagEnd - layerStart + 1);

        const int columns = tmxInt(mapTag, "width", 0);
        const int rows = tmxInt(mapTag, "height", 0);
        const int cellW = tmxInt(mapTag, "tilewidth", 0);
        const int cellH = tmxInt(mapTag, "tileheight", 0);
        const int firstGid = tmxInt(tilesetTag, "firstgid", 1);
        const int atlasW = tmxInt(imageTag, "width", 0);
        const int atlasH = tmxInt(imageTag, "height", 0);
        if (columns <= 0 || rows <= 0 || cellW <= 0 || cellH <= 0 ||
            firstGid <= 0 || atlasW <= 0 || atlasH <= 0)
            return false;

        std::string imagePath = tmxAttribute(imageTag, "source");
        if (imagePath.empty())
            return false;
        std::size_t slash = std::string(tmx).find_last_of("/\\");
        std::string texturePath = slash == std::string::npos
                                      ? imagePath
                                      : std::string(tmx).substr(0, slash + 1) + imagePath;
        Texture *texture = assets.LoadTexture(textureName, texturePath.c_str());
        if (!texture)
            return false;

        setTexture(texture);
        setCellSize((float)cellW, (float)cellH);
        setMapSize(columns, rows);
        setAtlasTilesX(atlasW / cellW);

        const std::string csv = xml.substr(dataStart + xml.substr(dataStart).find('>') + 1,
                                            dataEnd - (dataStart + xml.substr(dataStart).find('>') + 1));
        std::stringstream values(csv);
        std::string token;
        int index = 0;
        while (std::getline(values, token, ','))
        {
            std::size_t begin = token.find_first_not_of(" \t\r\n");
            std::size_t end = token.find_last_not_of(" \t\r\n");
            if (begin == std::string::npos || end == std::string::npos)
                continue;

            unsigned long rawGid = std::strtoul(token.substr(begin, end - begin + 1).c_str(), nullptr, 10);
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

    void TileMapComponent::buildPathfindingGrid(AStarGrid2D &grid, const int *solidTileIds,
                                                 int solidTileIdCount) const
    {
        grid.SetSize(mColumns, mRows);
        grid.SetCellSize(mCellW, mCellH);

        if (!solidTileIds || solidTileIdCount <= 0)
            return;

        for (int y = 0; y < mRows; ++y)
        {
            for (int x = 0; x < mColumns; ++x)
            {
                int cell = mCells[y * mColumns + x];
                for (int i = 0; i < solidTileIdCount; ++i)
                {
                    if (solidTileIds[i] == cell)
                    {
                        grid.SetSolid(x, y, true);
                        break;
                    }
                }
            }
        }
    }

    void TileMapComponent::onRender(RenderQueue &queue)
    {
        if (!mTexture || mCells.empty())
            return;

        RenderItem &item = queue.AddItem(owner()->zIndex());
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

                RenderCommand rect = RenderCommand::MakeRect(mTexture->Id(),
                                                             x * mCellW, y * mCellH, mCellW, mCellH);
                rect.srcX = tx * mCellW;
                rect.srcY = ty * mCellH;
                rect.srcW = mCellW;
                rect.srcH = mCellH;
                rect.texWidth = atlasW;
                rect.texHeight = atlasH;
                rect.pivotX = 0.0f;
                rect.pivotY = 0.0f;
                item.commands.push_back(rect);
            }
        }
    }

} // namespace k2d