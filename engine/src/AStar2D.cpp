#include "k2d/AStar2D.h"

#include <algorithm>
#include <limits>

namespace k2d
{

    void AStar2D::AddPoint(int id, const glm::vec2 &pos, float weightScale)
    {
        Point &p = mPoints[id]; 
        p.pos = pos;
        p.weightScale = weightScale;
    }

    void AStar2D::RemovePoint(int id)
    {
        Point *p = mPoints.find(id);
        if (!p)
            return;

        for (auto it = p->neighbors.begin(); it != p->neighbors.end(); ++it)
        {
            if (Point *other = mPoints.find(*it))
                other->neighbors.erase(id);
        }
        mPoints.erase(id);
    }

    bool AStar2D::HasPoint(int id) const
    {
        return mPoints.contains(id);
    }

    void AStar2D::SetPointPosition(int id, const glm::vec2 &pos)
    {
        if (Point *p = mPoints.find(id))
            p->pos = pos;
    }

    glm::vec2 AStar2D::GetPointPosition(int id) const
    {
        const Point *p = mPoints.find(id);
        return p ? p->pos : glm::vec2(0.0f);
    }

    void AStar2D::SetPointWeightScale(int id, float weightScale)
    {
        if (Point *p = mPoints.find(id))
            p->weightScale = weightScale;
    }

    float AStar2D::GetPointWeightScale(int id) const
    {
        const Point *p = mPoints.find(id);
        return p ? p->weightScale : 1.0f;
    }

    void AStar2D::SetPointDisabled(int id, bool disabled)
    {
        if (Point *p = mPoints.find(id))
            p->disabled = disabled;
    }

    bool AStar2D::IsPointDisabled(int id) const
    {
        const Point *p = mPoints.find(id);
        return p ? p->disabled : false;
    }

    void AStar2D::ConnectPoints(int id, int withId, bool bidirectional)
    {
        Point *a = mPoints.find(id);
        Point *b = mPoints.find(withId);
        if (!a || !b || id == withId)
            return;
        a->neighbors.insert(withId);
        if (bidirectional)
            b->neighbors.insert(id);
    }

    void AStar2D::DisconnectPoints(int id, int withId, bool bidirectional)
    {
        if (Point *a = mPoints.find(id))
            a->neighbors.erase(withId);
        if (bidirectional)
        {
            if (Point *b = mPoints.find(withId))
                b->neighbors.erase(id);
        }
    }

    bool AStar2D::ArePointsConnected(int id, int withId, bool bidirectional) const
    {
        const Point *a = mPoints.find(id);
        if (a && a->neighbors.contains(withId))
            return true;
        if (bidirectional)
        {
            const Point *b = mPoints.find(withId);
            if (b && b->neighbors.contains(id))
                return true;
        }
        return false;
    }

    void AStar2D::Clear()
    {
        mPoints.clear();
    }

    int AStar2D::GetPointCount() const
    {
        return (int)mPoints.size();
    }

    int AStar2D::GetClosestPoint(const glm::vec2 &point, bool includeDisabled) const
    {
        int closestId = -1;
        float closestDistSq = std::numeric_limits<float>::max();

        auto &points = const_cast<ct::HashMap<int, Point> &>(mPoints);
        for (auto it = points.begin(); it != points.end(); ++it)
        {
            if (!includeDisabled && it->value.disabled)
                continue;
            glm::vec2 d = it->value.pos - point;
            float distSq = d.x * d.x + d.y * d.y;
            if (distSq < closestDistSq)
            {
                closestDistSq = distSq;
                closestId = it->key;
            }
        }
        return closestId;
    }

    bool AStar2D::Solve(int fromId, int toId, bool allowPartialPath)
    {
        mLastClosestId = -1;
        ++mPass;

        Point *beginPoint = mPoints.find(fromId);
        Point *endPoint = mPoints.find(toId);
        if (!beginPoint || !endPoint)
            return false;
        if (beginPoint->disabled)
            return false;
        if (fromId == toId)
            return true;
        if (endPoint->disabled && !allowPartialPath)
            return false;

        mEndId = toId;

        auto estimate = [](const Point &a, const Point &b)
        {
            glm::vec2 d = a.pos - b.pos;
            return std::sqrt(d.x * d.x + d.y * d.y);
        };

        beginPoint->gScore = 0.0f;
        beginPoint->fScore = estimate(*beginPoint, *endPoint);
        beginPoint->absG = 0.0f;
        beginPoint->absF = beginPoint->fScore;
        beginPoint->openPass = mPass;

        auto worse = [this](int a, int b)
        {
            const Point *pa = mPoints.find(a);
            const Point *pb = mPoints.find(b);
            if (pa->fScore != pb->fScore)
                return pa->fScore > pb->fScore;
            return pa->gScore < pb->gScore;
        };

        ct::Vector<int> openHeap;
        openHeap.push_back(fromId);

        bool foundRoute = false;
        while (!openHeap.empty())
        {
            int currentId = openHeap.front();
            Point *current = mPoints.find(currentId);

            const Point *lastClosest = mLastClosestId >= 0 ? mPoints.find(mLastClosestId) : nullptr;
            if (!lastClosest || lastClosest->absF > current->absF ||
                (lastClosest->absF >= current->absF && lastClosest->absG > current->absG))
            {
                mLastClosestId = currentId;
            }

            if (currentId == toId)
            {
                foundRoute = true;
                break;
            }

            std::pop_heap(openHeap.begin(), openHeap.end(), worse);
            openHeap.pop_back();
            current->closedPass = mPass;

            for (auto it = current->neighbors.begin(); it != current->neighbors.end(); ++it)
            {
                int neighborId = *it;
                Point *neighbor = mPoints.find(neighborId);
                if (!neighbor || neighbor->disabled || neighbor->closedPass == mPass)
                    continue;

                float stepCost = estimate(*current, *neighbor) * neighbor->weightScale;
                float tentativeG = current->gScore + stepCost;
                bool newPoint = false;
                if (neighbor->openPass != mPass)
                {
                    neighbor->openPass = mPass;
                    newPoint = true;
                }
                else if (tentativeG >= neighbor->gScore)
                {
                    continue;
                }

                neighbor->prevId = currentId;
                neighbor->gScore = tentativeG;
                neighbor->fScore = tentativeG + estimate(*neighbor, *endPoint);
                neighbor->absG = tentativeG;
                neighbor->absF = neighbor->fScore - neighbor->gScore;

                if (newPoint)
                {
                    openHeap.push_back(neighborId);
                    std::push_heap(openHeap.begin(), openHeap.end(), worse);
                }
                else
                {
                    std::push_heap(openHeap.begin(),
                                   std::find(openHeap.begin(), openHeap.end(), neighborId) + 1, worse);
                }
            }
        }

        return foundRoute;
    }

    bool AStar2D::GetIdPath(int fromId, int toId, ct::Vector<int> &outPath, bool allowPartialPath)
    {
        outPath.clear();
        bool found = Solve(fromId, toId, allowPartialPath);
        if (!found && !(allowPartialPath && mLastClosestId >= 0))
            return false;

        int walkId = found ? mEndId : mLastClosestId;
        ct::Vector<int> reversed;
        while (walkId >= 0)
        {
            reversed.push_back(walkId);
            const Point *p = mPoints.find(walkId);
            walkId = p ? p->prevId : -1;
        }
        for (size_t i = reversed.size(); i-- > 0;)
            outPath.push_back(reversed[i]);
        return true;
    }

    bool AStar2D::GetPointPath(int fromId, int toId, ct::Vector<glm::vec2> &outPath, bool allowPartialPath)
    {
        outPath.clear();
        ct::Vector<int> idPath;
        if (!GetIdPath(fromId, toId, idPath, allowPartialPath))
            return false;
        for (size_t i = 0; i < idPath.size(); ++i)
            outPath.push_back(GetPointPosition(idPath[i]));
        return true;
    }

}