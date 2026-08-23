#include "k2d/AStarGrid2D.h"

#include <algorithm>
#include <cmath>

namespace k2d
{

    namespace
    {
        float HeuristicEuclidean(const glm::ivec2 &from, const glm::ivec2 &to)
        {
            float dx = (float)std::abs(to.x - from.x);
            float dy = (float)std::abs(to.y - from.y);
            return std::sqrt(dx * dx + dy * dy);
        }

        float HeuristicManhattan(const glm::ivec2 &from, const glm::ivec2 &to)
        {
            return (float)(std::abs(to.x - from.x) + std::abs(to.y - from.y));
        }

        float HeuristicOctile(const glm::ivec2 &from, const glm::ivec2 &to)
        {
            float dx = (float)std::abs(to.x - from.x);
            float dy = (float)std::abs(to.y - from.y);
            static const float kDiag = 1.4142135623730951f - 1.0f; 
            return (dx < dy) ? kDiag * dx + dy : kDiag * dy + dx;
        }

        float HeuristicChebyshev(const glm::ivec2 &from, const glm::ivec2 &to)
        {
            float dx = (float)std::abs(to.x - from.x);
            float dy = (float)std::abs(to.y - from.y);
            return dx > dy ? dx : dy;
        }
    }

    AStarGrid2D::AStarGrid2D()
        : mMinX(0), mMinY(0), mWidth(0), mHeight(0), mCellSize(1.0f, 1.0f), mOffset(0.0f, 0.0f),
          mHeuristic(Heuristic::Euclidean), mDiagonalMode(DiagonalMode::AtLeastOneWalkable),
          mPass(1), mEndIndex(-1), mLastClosestIndex(-1)
    {
    }

    void AStarGrid2D::SetSize(int width, int height)
    {
        mWidth = width > 0 ? width : 0;
        mHeight = height > 0 ? height : 0;
        mMinX = 0;
        mMinY = 0;
        mPoints.clear();
        mPoints.reserve((size_t)mWidth * (size_t)mHeight);
        for (int y = 0; y < mHeight; ++y)
        {
            for (int x = 0; x < mWidth; ++x)
            {
                Point p;
                p.id = glm::ivec2(x, y);
                mPoints.push_back(p);
            }
        }
    }

    void AStarGrid2D::Clear()
    {
        for (size_t i = 0; i < mPoints.size(); ++i)
        {
            mPoints[i].solid = false;
            mPoints[i].weightScale = 1.0f;
        }
    }

    bool AStarGrid2D::IsInBounds(int x, int y) const
    {
        return x >= mMinX && x < mMinX + mWidth && y >= mMinY && y < mMinY + mHeight;
    }

    AStarGrid2D::Point *AStarGrid2D::At(int x, int y)
    {
        if (!IsInBounds(x, y))
            return nullptr;
        return &mPoints[(size_t)ToIndex(x, y)];
    }

    void AStarGrid2D::SetSolid(int x, int y, bool solid)
    {
        if (Point *p = At(x, y))
            p->solid = solid;
    }

    bool AStarGrid2D::IsSolid(int x, int y) const
    {
        if (!IsInBounds(x, y))
            return true; 

        return mPoints[(size_t)ToIndex(x, y)].solid;
    }

    void AStarGrid2D::FillSolidRegion(int x, int y, int width, int height, bool solid)
    {
        for (int yy = y; yy < y + height; ++yy)
            for (int xx = x; xx < x + width; ++xx)
                SetSolid(xx, yy, solid);
    }

    void AStarGrid2D::SetWeightScale(int x, int y, float weightScale)
    {
        if (Point *p = At(x, y))
            p->weightScale = weightScale;
    }

    float AStarGrid2D::GetWeightScale(int x, int y) const
    {
        if (!IsInBounds(x, y))
            return 1.0f;
        return mPoints[(size_t)ToIndex(x, y)].weightScale;
    }

    glm::vec2 AStarGrid2D::GetPointPosition(int x, int y) const
    {
        return mOffset + glm::vec2((float)x + 0.5f, (float)y + 0.5f) * mCellSize;
    }

    float AStarGrid2D::EstimateCost(const glm::ivec2 &from, const glm::ivec2 &to) const
    {
        switch (mHeuristic)
        {
        case Heuristic::Manhattan: return HeuristicManhattan(from, to);
        case Heuristic::Octile: return HeuristicOctile(from, to);
        case Heuristic::Chebyshev: return HeuristicChebyshev(from, to);
        default: return HeuristicEuclidean(from, to);
        }
    }

    float AStarGrid2D::ComputeCost(const glm::ivec2 &from, const glm::ivec2 &to) const
    {

        return EstimateCost(from, to);
    }

    void AStarGrid2D::GetNeighbors(const Point &point, glm::ivec2 (&outNeighbors)[8], int &outCount) const
    {
        outCount = 0;
        int x = point.id.x;
        int y = point.id.y;

        bool hasLeft = IsInBounds(x - 1, y);
        bool hasRight = IsInBounds(x + 1, y);
        bool hasTop = IsInBounds(x, y - 1);
        bool hasBottom = IsInBounds(x, y + 1);

        bool cardinalTop = hasTop && !IsSolid(x, y - 1);
        bool cardinalRight = hasRight && !IsSolid(x + 1, y);
        bool cardinalBottom = hasBottom && !IsSolid(x, y + 1);
        bool cardinalLeft = hasLeft && !IsSolid(x - 1, y);

        if (cardinalTop) outNeighbors[outCount++] = glm::ivec2(x, y - 1);
        if (cardinalRight) outNeighbors[outCount++] = glm::ivec2(x + 1, y);
        if (cardinalBottom) outNeighbors[outCount++] = glm::ivec2(x, y + 1);
        if (cardinalLeft) outNeighbors[outCount++] = glm::ivec2(x - 1, y);

        bool diagTopLeft = false, diagTopRight = false, diagBottomRight = false, diagBottomLeft = false;
        switch (mDiagonalMode)
        {
        case DiagonalMode::Always:
            diagTopLeft = diagTopRight = diagBottomRight = diagBottomLeft = true;
            break;
        case DiagonalMode::Never:
            break;
        case DiagonalMode::AtLeastOneWalkable:
            diagTopLeft = cardinalLeft || cardinalTop;
            diagTopRight = cardinalTop || cardinalRight;
            diagBottomRight = cardinalRight || cardinalBottom;
            diagBottomLeft = cardinalBottom || cardinalLeft;
            break;
        case DiagonalMode::OnlyIfNoObstacles:
            diagTopLeft = cardinalLeft && cardinalTop;
            diagTopRight = cardinalTop && cardinalRight;
            diagBottomRight = cardinalRight && cardinalBottom;
            diagBottomLeft = cardinalBottom && cardinalLeft;
            break;
        }

        if (diagTopLeft && hasLeft && hasTop && !IsSolid(x - 1, y - 1))
            outNeighbors[outCount++] = glm::ivec2(x - 1, y - 1);
        if (diagTopRight && hasRight && hasTop && !IsSolid(x + 1, y - 1))
            outNeighbors[outCount++] = glm::ivec2(x + 1, y - 1);
        if (diagBottomRight && hasRight && hasBottom && !IsSolid(x + 1, y + 1))
            outNeighbors[outCount++] = glm::ivec2(x + 1, y + 1);
        if (diagBottomLeft && hasLeft && hasBottom && !IsSolid(x - 1, y + 1))
            outNeighbors[outCount++] = glm::ivec2(x - 1, y + 1);
    }

    bool AStarGrid2D::Solve(const glm::ivec2 &begin, const glm::ivec2 &end, bool allowPartialPath)
    {
        mLastClosestIndex = -1;
        ++mPass;

        if (!IsInBounds(begin.x, begin.y) || !IsInBounds(end.x, end.y))
            return false;
        if (IsSolid(begin.x, begin.y))
            return false;
        if (begin == end)
            return true;
        if (IsSolid(end.x, end.y) && !allowPartialPath)
            return false;

        int beginIndex = ToIndex(begin.x, begin.y);
        int endIndex = ToIndex(end.x, end.y);
        mEndIndex = endIndex;

        Point &beginPoint = mPoints[(size_t)beginIndex];
        beginPoint.gScore = 0.0f;
        beginPoint.fScore = EstimateCost(begin, end);
        beginPoint.openPass = mPass;

        auto worse = [this](int a, int b)
        {
            const Point &pa = mPoints[(size_t)a];
            const Point &pb = mPoints[(size_t)b];
            if (pa.fScore != pb.fScore)
                return pa.fScore > pb.fScore;
            return pa.gScore < pb.gScore;
        };

        mOpenHeap.clear();
        mOpenHeap.push_back(beginIndex);

        ct::Vector<float> absG(mPoints.size(), 0.0f);
        ct::Vector<float> absF(mPoints.size(), 0.0f);
        absG[(size_t)beginIndex] = 0.0f;
        absF[(size_t)beginIndex] = beginPoint.fScore;

        glm::ivec2 neighbors[8];
        bool foundRoute = false;

        while (!mOpenHeap.empty())
        {
            int currentIndex = mOpenHeap.front();
            Point &current = mPoints[(size_t)currentIndex];

            if (mLastClosestIndex < 0 ||
                absF[(size_t)mLastClosestIndex] > absF[(size_t)currentIndex] ||
                (absF[(size_t)mLastClosestIndex] >= absF[(size_t)currentIndex] &&
                 absG[(size_t)mLastClosestIndex] > absG[(size_t)currentIndex]))
            {
                mLastClosestIndex = currentIndex;
            }

            if (currentIndex == endIndex)
            {
                foundRoute = true;
                break;
            }

            std::pop_heap(mOpenHeap.begin(), mOpenHeap.end(), worse);
            mOpenHeap.pop_back();
            current.closedPass = mPass;

            int neighborCount = 0;
            GetNeighbors(current, neighbors, neighborCount);

            for (int i = 0; i < neighborCount; ++i)
            {
                int neighborIndex = ToIndex(neighbors[i].x, neighbors[i].y);
                Point &neighbor = mPoints[(size_t)neighborIndex];
                if (neighbor.closedPass == mPass)
                    continue;

                float tentativeG = current.gScore + ComputeCost(current.id, neighbor.id) * neighbor.weightScale;
                bool newPoint = false;
                if (neighbor.openPass != mPass)
                {
                    neighbor.openPass = mPass;
                    newPoint = true;
                }
                else if (tentativeG >= neighbor.gScore)
                {
                    continue;
                }

                neighbor.prevIndex = currentIndex;
                neighbor.gScore = tentativeG;
                neighbor.fScore = tentativeG + EstimateCost(neighbor.id, end);
                absG[(size_t)neighborIndex] = tentativeG;
                absF[(size_t)neighborIndex] = neighbor.fScore - neighbor.gScore;

                if (newPoint)
                {
                    mOpenHeap.push_back(neighborIndex);
                    std::push_heap(mOpenHeap.begin(), mOpenHeap.end(), worse);
                }
                else
                {
                    std::push_heap(mOpenHeap.begin(),
                                   std::find(mOpenHeap.begin(), mOpenHeap.end(), neighborIndex) + 1, worse);
                }
            }
        }

        return foundRoute;
    }

    bool AStarGrid2D::GetIdPath(glm::ivec2 from, glm::ivec2 to, ct::Vector<glm::ivec2> &outPath,
                                bool allowPartialPath)
    {
        outPath.clear();
        bool found = Solve(from, to, allowPartialPath);
        if (!found && !(allowPartialPath && mLastClosestIndex >= 0))
            return false;

        int walkIndex = found ? mEndIndex : mLastClosestIndex;
        ct::Vector<glm::ivec2> reversed;
        while (walkIndex >= 0)
        {
            reversed.push_back(mPoints[(size_t)walkIndex].id);
            walkIndex = mPoints[(size_t)walkIndex].prevIndex;
        }
        for (size_t i = reversed.size(); i-- > 0;)
            outPath.push_back(reversed[i]);
        return true;
    }

    bool AStarGrid2D::GetPointPath(glm::ivec2 from, glm::ivec2 to, ct::Vector<glm::vec2> &outPath,
                                   bool allowPartialPath)
    {
        outPath.clear();
        ct::Vector<glm::ivec2> idPath;
        if (!GetIdPath(from, to, idPath, allowPartialPath))
            return false;
        for (size_t i = 0; i < idPath.size(); ++i)
            outPath.push_back(GetPointPosition(idPath[i].x, idPath[i].y));
        return true;
    }

}