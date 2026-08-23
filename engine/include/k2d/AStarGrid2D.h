#pragma once

#include <ct/vector.hpp>
#include <glm/glm.hpp>

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

        // How diagonal moves are allowed relative to the two cells a diagonal
        // step "cuts past". AtLeastOneWalkable matches Godot's default: you
        // can cut a diagonal corner as long as one of the two side cells is
        // open (stops you phasing through an actual wall corner but doesn't
        // forbid grazing a single solid corner).
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

        void SetCellSize(float width, float height) { mCellSize = glm::vec2(width, height); }
        void SetOffset(const glm::vec2 &offset) { mOffset = offset; }

        void SetHeuristic(Heuristic heuristic) { mHeuristic = heuristic; }
        void SetDiagonalMode(DiagonalMode mode) { mDiagonalMode = mode; }

        // Clears solid/weight state without resizing (Godot's fill_solid_region
        // with the full rect is the common case; this is the direct clear()).
        void Clear();

        bool IsInBounds(int x, int y) const;
        void SetSolid(int x, int y, bool solid = true);
        bool IsSolid(int x, int y) const;
        void FillSolidRegion(int x, int y, int width, int height, bool solid = true);
        void SetWeightScale(int x, int y, float weightScale);
        float GetWeightScale(int x, int y) const;

        glm::vec2 GetPointPosition(int x, int y) const;

        // Returns false only when no path (not even a partial one) exists,
        // e.g. out-of-bounds endpoints. outPath is left empty in that case.
        bool GetIdPath(glm::ivec2 from, glm::ivec2 to, ct::Vector<glm::ivec2> &outPath,
                       bool allowPartialPath = false);
        bool GetPointPath(glm::ivec2 from, glm::ivec2 to, ct::Vector<glm::vec2> &outPath,
                          bool allowPartialPath = false);

    private:
        struct Point
        {
            glm::ivec2 id{0, 0};
            glm::vec2 pos{0.0f, 0.0f};
            float weightScale = 1.0f;
            bool solid = false;

            // Pathfinding scratch state, valid only while `pass` matches.
            int prevIndex = -1;
            float gScore = 0.0f;
            float fScore = 0.0f;
            unsigned int openPass = 0;
            unsigned int closedPass = 0;
        };

        int ToIndex(int x, int y) const { return (y - mMinY) * mWidth + (x - mMinX); }
        Point *At(int x, int y);
        float EstimateCost(const glm::ivec2 &from, const glm::ivec2 &to) const;
        float ComputeCost(const glm::ivec2 &from, const glm::ivec2 &to) const;
        void GetNeighbors(const Point &point, glm::ivec2 (&outNeighbors)[8], int &outCount) const;
        bool Solve(const glm::ivec2 &begin, const glm::ivec2 &end, bool allowPartialPath);

        int mMinX, mMinY, mWidth, mHeight;
        glm::vec2 mCellSize;
        glm::vec2 mOffset;
        Heuristic mHeuristic;
        DiagonalMode mDiagonalMode;

        ct::Vector<Point> mPoints;
        unsigned int mPass;
        int mEndIndex;
        int mLastClosestIndex;

        // Reused across Solve() calls -- see comment in the .cpp.
        ct::Vector<int> mOpenHeap;
    };

}
