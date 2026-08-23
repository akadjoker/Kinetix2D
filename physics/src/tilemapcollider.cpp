#include "kx/tilemapcollider.h"

namespace kx
{

    namespace
    {
        struct Rectangle
        {
            int x;
            int y;
            int width;
            int height;
        };
    }

    TileMapCollider::TileMapCollider(World &world)
        : mWorld(world), mColumns(0), mRows(0), mCellSize(1.0f), mOffset(0.0f),
          mSolid(), mBodies()
    {
    }

    TileMapCollider::~TileMapCollider()
    {
        Clear();
    }

    void TileMapCollider::SetMapSize(int columns, int rows)
    {
        mColumns = columns > 0 ? columns : 0;
        mRows = rows > 0 ? rows : 0;
        mSolid.resize(mColumns * mRows, 0);
    }

    void TileMapCollider::SetCellSize(const Math::Vec2 &size)
    {
        mCellSize.x = size.x > 0.0f ? size.x : 1.0f;
        mCellSize.y = size.y > 0.0f ? size.y : 1.0f;
    }

    void TileMapCollider::SetOffset(const Math::Vec2 &offset)
    {
        mOffset = offset;
    }

    void TileMapCollider::SetSolid(int x, int y, bool solid)
    {
        if (x < 0 || x >= mColumns || y < 0 || y >= mRows)
            return;
        mSolid[y * mColumns + x] = solid ? 1 : 0;
    }

    bool TileMapCollider::IsSolid(int x, int y) const
    {
        if (x < 0 || x >= mColumns || y < 0 || y >= mRows)
            return false;
        return mSolid[y * mColumns + x] != 0;
    }

    void TileMapCollider::Clear()
    {
        for (size_t i = 0; i < mBodies.size(); ++i)
            mWorld.Destroy(mBodies[i]);
        mBodies.clear();
    }

    void TileMapCollider::Rebuild()
    {
        Clear();
        if (mColumns == 0 || mRows == 0)
            return;

        const int chunkSize = 16;
        for (int chunkY = 0; chunkY < mRows; chunkY += chunkSize)
        {
            for (int chunkX = 0; chunkX < mColumns; chunkX += chunkSize)
            {
                const int endX = chunkX + chunkSize < mColumns ? chunkX + chunkSize : mColumns;
                const int endY = chunkY + chunkSize < mRows ? chunkY + chunkSize : mRows;
                const int width = endX - chunkX;
                const int height = endY - chunkY;
                ct::Vector<unsigned char> used;
                used.resize(width * height, 0);
                ct::Vector<Rectangle> rectangles;

                for (int y = 0; y < height; ++y)
                {
                    for (int x = 0; x < width; ++x)
                    {
                        if (used[y * width + x] || !IsSolid(chunkX + x, chunkY + y))
                            continue;

                        int rectangleWidth = 1;
                        while (x + rectangleWidth < width &&
                               !used[y * width + x + rectangleWidth] &&
                               IsSolid(chunkX + x + rectangleWidth, chunkY + y))
                            ++rectangleWidth;

                        int rectangleHeight = 1;
                        bool fullRow = true;
                        while (y + rectangleHeight < height && fullRow)
                        {
                            for (int rowX = 0; rowX < rectangleWidth; ++rowX)
                            {
                                if (used[(y + rectangleHeight) * width + x + rowX] ||
                                    !IsSolid(chunkX + x + rowX, chunkY + y + rectangleHeight))
                                {
                                    fullRow = false;
                                    break;
                                }
                            }
                            if (fullRow)
                                ++rectangleHeight;
                        }

                        for (int markY = 0; markY < rectangleHeight; ++markY)
                            for (int markX = 0; markX < rectangleWidth; ++markX)
                                used[(y + markY) * width + x + markX] = 1;

                        Rectangle rectangle = {x, y, rectangleWidth, rectangleHeight};
                        rectangles.push_back(rectangle);
                    }
                }

                Body *body = nullptr;
                for (size_t i = 0; i < rectangles.size(); ++i)
                {
                    if (!body || body->ShapeCount() >= Body::kMaxShapes)
                    {
                        body = mWorld.CreateBody(BodyType::Static, Math::Vec2(0.0f));
                        mBodies.push_back(body);
                    }

                    const Rectangle &rectangle = rectangles[i];
                    Math::Vec2 center = mOffset + Math::Vec2(
                        (chunkX + rectangle.x + rectangle.width * 0.5f) * mCellSize.x,
                        (chunkY + rectangle.y + rectangle.height * 0.5f) * mCellSize.y);
                    body->AddBox(rectangle.width * mCellSize.x * 0.5f,
                                 rectangle.height * mCellSize.y * 0.5f,
                                 center, 1.0f);
                }
            }
        }
    }

}