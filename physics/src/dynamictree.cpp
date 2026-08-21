#include "kx/dynamictree.h"

#include <cassert>
#include <cstring>

namespace kx
{

    DynamicTree::DynamicTree()
    {
        mRoot = kNullNode;

        mNodeCapacity = 16;
        mNodeCount = 0;
        mNodes = new TreeNode[mNodeCapacity];
        std::memset(mNodes, 0, mNodeCapacity * sizeof(TreeNode));

        for (int32_t i = 0; i < mNodeCapacity - 1; ++i)
        {
            mNodes[i].next = i + 1;
            mNodes[i].height = -1;
        }
        mNodes[mNodeCapacity - 1].next = kNullNode;
        mNodes[mNodeCapacity - 1].height = -1;
        mFreeList = 0;

        mInsertionCount = 0;
    }

    DynamicTree::~DynamicTree()
    {
        delete[] mNodes;
    }

    int32_t DynamicTree::AllocateNode()
    {
        if (mFreeList == kNullNode)
        {
            assert(mNodeCount == mNodeCapacity);

            TreeNode *oldNodes = mNodes;
            mNodeCapacity *= 2;
            mNodes = new TreeNode[mNodeCapacity];
            std::memcpy(mNodes, oldNodes, mNodeCount * sizeof(TreeNode));
            delete[] oldNodes;

            for (int32_t i = mNodeCount; i < mNodeCapacity - 1; ++i)
            {
                mNodes[i].next = i + 1;
                mNodes[i].height = -1;
            }
            mNodes[mNodeCapacity - 1].next = kNullNode;
            mNodes[mNodeCapacity - 1].height = -1;
            mFreeList = mNodeCount;
        }

        int32_t nodeId = mFreeList;
        mFreeList = mNodes[nodeId].next;
        mNodes[nodeId].parent = kNullNode;
        mNodes[nodeId].child1 = kNullNode;
        mNodes[nodeId].child2 = kNullNode;
        mNodes[nodeId].height = 0;
        mNodes[nodeId].userData = nullptr;
        mNodes[nodeId].moved = false;
        ++mNodeCount;
        return nodeId;
    }

    void DynamicTree::FreeNode(int32_t nodeId)
    {
        assert(0 <= nodeId && nodeId < mNodeCapacity);
        assert(0 < mNodeCount);
        mNodes[nodeId].next = mFreeList;
        mNodes[nodeId].height = -1;
        mFreeList = nodeId;
        --mNodeCount;
    }

    int32_t DynamicTree::CreateProxy(const AABB &aabb, void *userData)
    {
        int32_t proxyId = AllocateNode();

        glm::vec2 r(kAabbExtension, kAabbExtension);
        mNodes[proxyId].aabb.lowerBound = aabb.lowerBound - r;
        mNodes[proxyId].aabb.upperBound = aabb.upperBound + r;
        mNodes[proxyId].userData = userData;
        mNodes[proxyId].height = 0;
        mNodes[proxyId].moved = true;

        InsertLeaf(proxyId);

        return proxyId;
    }

    void DynamicTree::DestroyProxy(int32_t proxyId)
    {
        assert(0 <= proxyId && proxyId < mNodeCapacity);
        assert(mNodes[proxyId].IsLeaf());

        RemoveLeaf(proxyId);
        FreeNode(proxyId);
    }

    bool DynamicTree::MoveProxy(int32_t proxyId, const AABB &aabb, const glm::vec2 &displacement)
    {
        assert(0 <= proxyId && proxyId < mNodeCapacity);
        assert(mNodes[proxyId].IsLeaf());

        AABB fatAABB;
        glm::vec2 r(kAabbExtension, kAabbExtension);
        fatAABB.lowerBound = aabb.lowerBound - r;
        fatAABB.upperBound = aabb.upperBound + r;

        glm::vec2 d = kAabbMultiplier * displacement;

        if (d.x < 0.0f)
            fatAABB.lowerBound.x += d.x;
        else
            fatAABB.upperBound.x += d.x;

        if (d.y < 0.0f)
            fatAABB.lowerBound.y += d.y;
        else
            fatAABB.upperBound.y += d.y;

        const AABB &treeAABB = mNodes[proxyId].aabb;
        if (treeAABB.Contains(aabb))
        {
            AABB hugeAABB;
            hugeAABB.lowerBound = fatAABB.lowerBound - 4.0f * r;
            hugeAABB.upperBound = fatAABB.upperBound + 4.0f * r;

            if (hugeAABB.Contains(treeAABB))
                return false;
        }

        RemoveLeaf(proxyId);
        mNodes[proxyId].aabb = fatAABB;
        InsertLeaf(proxyId);
        mNodes[proxyId].moved = true;

        return true;
    }

    void DynamicTree::InsertLeaf(int32_t leaf)
    {
        ++mInsertionCount;

        if (mRoot == kNullNode)
        {
            mRoot = leaf;
            mNodes[mRoot].parent = kNullNode;
            return;
        }

        AABB leafAABB = mNodes[leaf].aabb;
        int32_t index = mRoot;
        while (!mNodes[index].IsLeaf())
        {
            int32_t child1 = mNodes[index].child1;
            int32_t child2 = mNodes[index].child2;

            float area = mNodes[index].aabb.GetPerimeter();

            AABB combinedAABB;
            combinedAABB.Combine(mNodes[index].aabb, leafAABB);
            float combinedArea = combinedAABB.GetPerimeter();

            float cost = 2.0f * combinedArea;
            float inheritanceCost = 2.0f * (combinedArea - area);

            float cost1;
            {
                AABB aabb;
                aabb.Combine(leafAABB, mNodes[child1].aabb);
                if (mNodes[child1].IsLeaf())
                {
                    cost1 = aabb.GetPerimeter() + inheritanceCost;
                }
                else
                {
                    float oldArea = mNodes[child1].aabb.GetPerimeter();
                    float newArea = aabb.GetPerimeter();
                    cost1 = (newArea - oldArea) + inheritanceCost;
                }
            }

            float cost2;
            {
                AABB aabb;
                aabb.Combine(leafAABB, mNodes[child2].aabb);
                if (mNodes[child2].IsLeaf())
                {
                    cost2 = aabb.GetPerimeter() + inheritanceCost;
                }
                else
                {
                    float oldArea = mNodes[child2].aabb.GetPerimeter();
                    float newArea = aabb.GetPerimeter();
                    cost2 = (newArea - oldArea) + inheritanceCost;
                }
            }

            if (cost < cost1 && cost < cost2)
                break;

            index = (cost1 < cost2) ? child1 : child2;
        }

        int32_t sibling = index;

        int32_t oldParent = mNodes[sibling].parent;
        int32_t newParent = AllocateNode();
        mNodes[newParent].parent = oldParent;
        mNodes[newParent].userData = nullptr;
        mNodes[newParent].aabb.Combine(leafAABB, mNodes[sibling].aabb);
        mNodes[newParent].height = mNodes[sibling].height + 1;

        if (oldParent != kNullNode)
        {
            if (mNodes[oldParent].child1 == sibling)
                mNodes[oldParent].child1 = newParent;
            else
                mNodes[oldParent].child2 = newParent;

            mNodes[newParent].child1 = sibling;
            mNodes[newParent].child2 = leaf;
            mNodes[sibling].parent = newParent;
            mNodes[leaf].parent = newParent;
        }
        else
        {
            mNodes[newParent].child1 = sibling;
            mNodes[newParent].child2 = leaf;
            mNodes[sibling].parent = newParent;
            mNodes[leaf].parent = newParent;
            mRoot = newParent;
        }

        index = mNodes[leaf].parent;
        while (index != kNullNode)
        {
            index = Balance(index);

            int32_t child1 = mNodes[index].child1;
            int32_t child2 = mNodes[index].child2;

            mNodes[index].height = 1 + (mNodes[child1].height > mNodes[child2].height ? mNodes[child1].height : mNodes[child2].height);
            mNodes[index].aabb.Combine(mNodes[child1].aabb, mNodes[child2].aabb);

            index = mNodes[index].parent;
        }
    }

    void DynamicTree::RemoveLeaf(int32_t leaf)
    {
        if (leaf == mRoot)
        {
            mRoot = kNullNode;
            return;
        }

        int32_t parent = mNodes[leaf].parent;
        int32_t grandParent = mNodes[parent].parent;
        int32_t sibling = (mNodes[parent].child1 == leaf) ? mNodes[parent].child2 : mNodes[parent].child1;

        if (grandParent != kNullNode)
        {
            if (mNodes[grandParent].child1 == parent)
                mNodes[grandParent].child1 = sibling;
            else
                mNodes[grandParent].child2 = sibling;
            mNodes[sibling].parent = grandParent;
            FreeNode(parent);

            int32_t index = grandParent;
            while (index != kNullNode)
            {
                index = Balance(index);

                int32_t child1 = mNodes[index].child1;
                int32_t child2 = mNodes[index].child2;

                mNodes[index].aabb.Combine(mNodes[child1].aabb, mNodes[child2].aabb);
                mNodes[index].height = 1 + (mNodes[child1].height > mNodes[child2].height ? mNodes[child1].height : mNodes[child2].height);

                index = mNodes[index].parent;
            }
        }
        else
        {
            mRoot = sibling;
            mNodes[sibling].parent = kNullNode;
            FreeNode(parent);
        }
    }

    int32_t DynamicTree::Balance(int32_t iA)
    {
        assert(iA != kNullNode);

        TreeNode *A = mNodes + iA;
        if (A->IsLeaf() || A->height < 2)
            return iA;

        int32_t iB = A->child1;
        int32_t iC = A->child2;

        TreeNode *B = mNodes + iB;
        TreeNode *C = mNodes + iC;

        int32_t balance = C->height - B->height;

        if (balance > 1)
        {
            int32_t iF = C->child1;
            int32_t iG = C->child2;
            TreeNode *F = mNodes + iF;
            TreeNode *G = mNodes + iG;

            C->child1 = iA;
            C->parent = A->parent;
            A->parent = iC;

            if (C->parent != kNullNode)
            {
                if (mNodes[C->parent].child1 == iA)
                    mNodes[C->parent].child1 = iC;
                else
                    mNodes[C->parent].child2 = iC;
            }
            else
            {
                mRoot = iC;
            }

            if (F->height > G->height)
            {
                C->child2 = iF;
                A->child2 = iG;
                G->parent = iA;
                A->aabb.Combine(B->aabb, G->aabb);
                C->aabb.Combine(A->aabb, F->aabb);

                A->height = 1 + (B->height > G->height ? B->height : G->height);
                C->height = 1 + (A->height > F->height ? A->height : F->height);
            }
            else
            {
                C->child2 = iG;
                A->child2 = iF;
                F->parent = iA;
                A->aabb.Combine(B->aabb, F->aabb);
                C->aabb.Combine(A->aabb, G->aabb);

                A->height = 1 + (B->height > F->height ? B->height : F->height);
                C->height = 1 + (A->height > G->height ? A->height : G->height);
            }

            return iC;
        }

        if (balance < -1)
        {
            int32_t iD = B->child1;
            int32_t iE = B->child2;
            TreeNode *D = mNodes + iD;
            TreeNode *E = mNodes + iE;

            B->child1 = iA;
            B->parent = A->parent;
            A->parent = iB;

            if (B->parent != kNullNode)
            {
                if (mNodes[B->parent].child1 == iA)
                    mNodes[B->parent].child1 = iB;
                else
                    mNodes[B->parent].child2 = iB;
            }
            else
            {
                mRoot = iB;
            }

            if (D->height > E->height)
            {
                B->child2 = iD;
                A->child1 = iE;
                E->parent = iA;
                A->aabb.Combine(C->aabb, E->aabb);
                B->aabb.Combine(A->aabb, D->aabb);

                A->height = 1 + (C->height > E->height ? C->height : E->height);
                B->height = 1 + (A->height > D->height ? A->height : D->height);
            }
            else
            {
                B->child2 = iE;
                A->child1 = iD;
                D->parent = iA;
                A->aabb.Combine(C->aabb, D->aabb);
                B->aabb.Combine(A->aabb, E->aabb);

                A->height = 1 + (C->height > D->height ? C->height : D->height);
                B->height = 1 + (A->height > E->height ? A->height : E->height);
            }

            return iB;
        }

        return iA;
    }

    int32_t DynamicTree::GetHeight() const
    {
        if (mRoot == kNullNode)
            return 0;
        return mNodes[mRoot].height;
    }

    int32_t DynamicTree::ComputeHeight(int32_t nodeId) const
    {
        assert(0 <= nodeId && nodeId < mNodeCapacity);
        TreeNode *node = mNodes + nodeId;

        if (node->IsLeaf())
            return 0;

        int32_t height1 = ComputeHeight(node->child1);
        int32_t height2 = ComputeHeight(node->child2);
        return 1 + (height1 > height2 ? height1 : height2);
    }

    int32_t DynamicTree::ComputeHeight() const
    {
        return ComputeHeight(mRoot);
    }

    void DynamicTree::ValidateStructure(int32_t index) const
    {
        if (index == kNullNode)
            return;

        if (index == mRoot)
            assert(mNodes[index].parent == kNullNode);

        const TreeNode &node = mNodes[index];
        int32_t child1 = node.child1;
        int32_t child2 = node.child2;

        if (node.IsLeaf())
        {
            assert(child1 == kNullNode);
            assert(child2 == kNullNode);
            assert(node.height == 0);
            return;
        }

        assert(0 <= child1 && child1 < mNodeCapacity);
        assert(0 <= child2 && child2 < mNodeCapacity);
        assert(mNodes[child1].parent == index);
        assert(mNodes[child2].parent == index);

        ValidateStructure(child1);
        ValidateStructure(child2);
    }

    void DynamicTree::ValidateMetrics(int32_t index) const
    {
        if (index == kNullNode)
            return;

        const TreeNode &node = mNodes[index];
        int32_t child1 = node.child1;
        int32_t child2 = node.child2;

        if (node.IsLeaf())
        {
            assert(child1 == kNullNode);
            assert(child2 == kNullNode);
            assert(node.height == 0);
            return;
        }

        assert(0 <= child1 && child1 < mNodeCapacity);
        assert(0 <= child2 && child2 < mNodeCapacity);

        int32_t height1 = mNodes[child1].height;
        int32_t height2 = mNodes[child2].height;
        int32_t height = 1 + (height1 > height2 ? height1 : height2);
        assert(node.height == height);
        (void)height;

        AABB aabb;
        aabb.Combine(mNodes[child1].aabb, mNodes[child2].aabb);
        assert(aabb.lowerBound == node.aabb.lowerBound);
        assert(aabb.upperBound == node.aabb.upperBound);

        ValidateMetrics(child1);
        ValidateMetrics(child2);
    }

    void DynamicTree::Validate() const
    {
        ValidateStructure(mRoot);
        ValidateMetrics(mRoot);

        int32_t freeCount = 0;
        int32_t freeIndex = mFreeList;
        while (freeIndex != kNullNode)
        {
            assert(0 <= freeIndex && freeIndex < mNodeCapacity);
            freeIndex = mNodes[freeIndex].next;
            ++freeCount;
        }

        assert(GetHeight() == ComputeHeight());
        assert(mNodeCount + freeCount == mNodeCapacity);
    }

} // namespace kx
