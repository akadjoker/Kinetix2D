#pragma once

#include <ct/hashmap.hpp>
#include <ct/hashset.hpp>
#include <ct/vector.hpp>
#include <glm/glm.hpp>

namespace k2d
{

    class AStar2D
    {
    public:
        void AddPoint(int id, const glm::vec2 &pos, float weightScale = 1.0f);
        void RemovePoint(int id);
        bool HasPoint(int id) const;
        void SetPointPosition(int id, const glm::vec2 &pos);
        glm::vec2 GetPointPosition(int id) const;
        void SetPointWeightScale(int id, float weightScale);
        float GetPointWeightScale(int id) const;
        void SetPointDisabled(int id, bool disabled = true);
        bool IsPointDisabled(int id) const;

        void ConnectPoints(int id, int withId, bool bidirectional = true);
        void DisconnectPoints(int id, int withId, bool bidirectional = true);
        bool ArePointsConnected(int id, int withId, bool bidirectional = true) const;

        void Clear();
        int GetPointCount() const;

        int GetClosestPoint(const glm::vec2 &point, bool includeDisabled = false) const;

        bool GetIdPath(int fromId, int toId, ct::Vector<int> &outPath, bool allowPartialPath = false);
        bool GetPointPath(int fromId, int toId, ct::Vector<glm::vec2> &outPath, bool allowPartialPath = false);

    private:
        struct Point
        {
            glm::vec2 pos{0.0f, 0.0f};
            float weightScale = 1.0f;
            bool disabled = false;
            ct::HashSet<int> neighbors;

            int prevId = -1;
            float gScore = 0.0f;
            float fScore = 0.0f;
            unsigned int openPass = 0;
            unsigned int closedPass = 0;
            float absG = 0.0f;
            float absF = 0.0f;
        };

        bool Solve(int fromId, int toId, bool allowPartialPath);

        ct::HashMap<int, Point> mPoints;
        unsigned int mPass = 1;
        int mEndId = -1;
        int mLastClosestId = -1;
    };

}