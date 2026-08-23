#pragma once

#include "k2d/IVec2.h"

#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{

    class AStarGrid2D
    {
    public:
        enum class Heuristic
        {
            Euclidean,
            Manhattan,
            Octile,
            Chebyshev
        };

        enum class DiagonalMode
        {
            Always,
            Never,
            AtLeastOneWalkable,
            OnlyIfNoObstacles
        };

        AStarGrid2D();

        void SetSize(int width, int height);
        int Width() const { return mWidth; }
        int Height() const { return mHeight; }

        void SetCellSize(float width, float height) { mCellSize = Math::Vec2(width, height); }
        void SetOffset(const Math::Vec2 &offset) { mOffset = offset; }

        void SetHeuristic(Heuristic heuristic) { mHeuristic = heuristic; }
        void SetDiagonalMode(DiagonalMode mode) { mDiagonalMode = mode; }

        void Clear();

        bool IsInBounds(int x, int y) const;
        void SetSolid(int x, int y, bool solid = true);
        bool IsSolid(int x, int y) const;
        void FillSolidRegion(int x, int y, int width, int height, bool solid = true);
        void SetWeightScale(int x, int y, float weightScale);
        float GetWeightScale(int x, int y) const;

        Math::Vec2 GetPointPosition(int x, int y) const;

        bool GetIdPath(IVec2 from, IVec2 to, ct::Vector<IVec2> &outPath,
                       bool allowPartialPath = false);
        bool GetPointPath(IVec2 from, IVec2 to, ct::Vector<Math::Vec2> &outPath,
                          bool allowPartialPath = false);

    private:
        struct Point
        {
            IVec2 id{0, 0};
            Math::Vec2 pos{0.0f, 0.0f};
            float weightScale = 1.0f;
            bool solid = false;

            int prevIndex = -1;
            float gScore = 0.0f;
            float fScore = 0.0f;
            unsigned int openPass = 0;
            unsigned int closedPass = 0;
        };

        int ToIndex(int x, int y) const { return (y - mMinY) * mWidth + (x - mMinX); }
        Point *At(int x, int y);
        float EstimateCost(const IVec2 &from, const IVec2 &to) const;
        float ComputeCost(const IVec2 &from, const IVec2 &to) const;
        void GetNeighbors(const Point &point, IVec2 (&outNeighbors)[8], int &outCount) const;
        bool Solve(const IVec2 &begin, const IVec2 &end, bool allowPartialPath);

        int mMinX, mMinY, mWidth, mHeight;
        Math::Vec2 mCellSize;
        Math::Vec2 mOffset;
        Heuristic mHeuristic;
        DiagonalMode mDiagonalMode;

        ct::Vector<Point> mPoints;
        unsigned int mPass;
        int mEndIndex;
        int mLastClosestIndex;

        ct::Vector<int> mOpenHeap;
    };

}