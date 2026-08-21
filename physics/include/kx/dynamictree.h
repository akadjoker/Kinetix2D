#pragma once

#include <glm/glm.hpp>
#include <cstdint>

#include <ct/vector.hpp>

#include "shapes.h"

namespace kx
{

    constexpr int32_t kNullNode = -1;

    struct TreeNode
    {
        bool IsLeaf() const { return child1 == kNullNode; }

        AABB aabb;
        void *userData;

        union
        {
            int32_t parent;
            int32_t next;
        };

        int32_t child1;
        int32_t child2;

        int32_t height;
        bool moved;
    };

    class DynamicTree
    {
    public:
        DynamicTree();
        ~DynamicTree();

        DynamicTree(const DynamicTree &) = delete;
        DynamicTree &operator=(const DynamicTree &) = delete;

        int32_t CreateProxy(const AABB &aabb, void *userData);
        void DestroyProxy(int32_t proxyId);

        bool MoveProxy(int32_t proxyId, const AABB &aabb, const glm::vec2 &displacement);

        void *GetUserData(int32_t proxyId) const { return mNodes[proxyId].userData; }
        bool WasMoved(int32_t proxyId) const { return mNodes[proxyId].moved; }
        void ClearMoved(int32_t proxyId) { mNodes[proxyId].moved = false; }
        const AABB &GetFatAABB(int32_t proxyId) const { return mNodes[proxyId].aabb; }

        template <typename T>
        void Query(T *callback, const AABB &aabb) const
        {
            ct::Vector<int32_t> stack;
            stack.push_back(mRoot);

            while (stack.size() > 0)
            {
                int32_t nodeId = stack.back();
                stack.pop_back();
                if (nodeId == kNullNode)
                    continue;

                const TreeNode &node = mNodes[nodeId];
                if (TestOverlap(node.aabb, aabb))
                {
                    if (node.IsLeaf())
                    {
                        if (!callback->QueryCallback(nodeId))
                            return;
                    }
                    else
                    {
                        stack.push_back(node.child1);
                        stack.push_back(node.child2);
                    }
                }
            }
        }

        int32_t GetHeight() const;

        void Validate() const;

    private:
        int32_t AllocateNode();
        void FreeNode(int32_t node);

        void InsertLeaf(int32_t node);
        void RemoveLeaf(int32_t node);

        int32_t Balance(int32_t index);

        int32_t ComputeHeight() const;
        int32_t ComputeHeight(int32_t nodeId) const;

        void ValidateStructure(int32_t index) const;
        void ValidateMetrics(int32_t index) const;

        int32_t mRoot;

        TreeNode *mNodes;
        int32_t mNodeCount;
        int32_t mNodeCapacity;

        int32_t mFreeList;
        int32_t mInsertionCount;
    };

} // namespace kx
