#include "k2d/TileMapCollider2D.h"

#include "k2d/GameObject.h"
#include "k2d/RigidBody2D.h"
#include "k2d/TileMapComponent.h"

#include <ct/vector.hpp>

namespace k2d
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

TileMapCollider2D::TileMapCollider2D()
{
}

TileMapComponent *TileMapCollider2D::findTileMap() const
{
    GameObject *object = owner();
    return object ? object->getComponent<TileMapComponent>() : nullptr;
}

int TileMapCollider2D::addTo(RigidBody2D &body, float density, float scaleX, float scaleY) const
{
    TileMapComponent *tileMap = findTileMap();
    if (!tileMap || tileMap->columns() <= 0 || tileMap->rows() <= 0)
        return 0;

    const int columns = tileMap->columns();
    const int rows = tileMap->rows();
    const Math::Vec2 cellSize(tileMap->cellWidth() * scaleX, tileMap->cellHeight() * scaleY);

    int added = 0;
    const int chunkSize = 16;
    ct::Vector<unsigned char> used;
    ct::Vector<Rectangle> rectangles;

    for (int chunkY = 0; chunkY < rows; chunkY += chunkSize)
    {
        for (int chunkX = 0; chunkX < columns; chunkX += chunkSize)
        {
            const int endX = chunkX + chunkSize < columns ? chunkX + chunkSize : columns;
            const int endY = chunkY + chunkSize < rows ? chunkY + chunkSize : rows;
            const int width = endX - chunkX;
            const int height = endY - chunkY;
            used.clear();
            used.resize((size_t)(width * height), 0);
            rectangles.clear();

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    if (used[(size_t)(y * width + x)] || !tileMap->hasCollision(chunkX + x, chunkY + y))
                        continue;

                    int rectangleWidth = 1;
                    while (x + rectangleWidth < width && !used[(size_t)(y * width + x + rectangleWidth)] &&
                           tileMap->hasCollision(chunkX + x + rectangleWidth, chunkY + y))
                        ++rectangleWidth;

                    int rectangleHeight = 1;
                    bool fullRow = true;
                    while (y + rectangleHeight < height && fullRow)
                    {
                        for (int rowX = 0; rowX < rectangleWidth; ++rowX)
                        {
                            if (used[(size_t)((y + rectangleHeight) * width + x + rowX)] ||
                                !tileMap->hasCollision(chunkX + x + rowX, chunkY + y + rectangleHeight))
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
                            used[(size_t)((y + markY) * width + x + markX)] = 1;

                    Rectangle rectangle = {x, y, rectangleWidth, rectangleHeight};
                    rectangles.push_back(rectangle);
                }
            }

            for (size_t i = 0; i < rectangles.size(); ++i)
            {
                const Rectangle &rectangle = rectangles[i];
                Math::Vec2 center = mOffset + Math::Vec2((chunkX + rectangle.x + rectangle.width * 0.5f) * cellSize.x,
                                                         (chunkY + rectangle.y + rectangle.height * 0.5f) * cellSize.y);
                body.AddBox(rectangle.width * cellSize.x * 0.5f, rectangle.height * cellSize.y * 0.5f, center, density);
                ++added;
            }
        }
    }

    return added;
}

}
