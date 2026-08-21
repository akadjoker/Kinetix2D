#include "k2d/TileMapComponent.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"

#include <cmath>

namespace k2d
{

    TileMapComponent::TileMapComponent()
        : Component(Type, ComponentEventRender), mTexture(nullptr),
          mCellW(32.0f), mCellH(32.0f), mColumns(0), mRows(0), mAtlasTilesX(1),
          mCullX(0.0f), mCullY(0.0f), mCullW(0.0f), mCullH(0.0f), mCullEnabled(false)
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

    void TileMapComponent::onRender(RenderQueue &queue)
    {
        if (!mTexture || mCells.empty())
            return;

        RenderItem &item = queue.AddItem(owner()->zIndex());
        item.xform = owner()->globalTransform();

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
                item.commands.push_back(rect);
            }
        }
    }

} // namespace k2d
