#pragma once

#include "body.h"
#include "world.h"

#include <ct/vector.hpp>

namespace kx
{

    class TileMapCollider
    {
    public:
        explicit TileMapCollider(World &world);
        ~TileMapCollider();

        TileMapCollider(const TileMapCollider &) = delete;
        TileMapCollider &operator=(const TileMapCollider &) = delete;

        void SetMapSize(int columns, int rows);
        void SetCellSize(const Math::Vec2 &size);
        void SetOffset(const Math::Vec2 &offset);
        void SetSolid(int x, int y, bool solid);
        bool IsSolid(int x, int y) const;
        void Clear();
        void Rebuild();

        int Columns() const { return mColumns; }
        int Rows() const { return mRows; }
        const Math::Vec2 &CellSize() const { return mCellSize; }
        const ct::Vector<Body *> &Bodies() const { return mBodies; }

    private:
        World &mWorld;
        int mColumns;
        int mRows;
        Math::Vec2 mCellSize;
        Math::Vec2 mOffset;
        ct::Vector<unsigned char> mSolid;
        ct::Vector<Body *> mBodies;
    };

}