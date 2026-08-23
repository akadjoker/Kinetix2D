#include "k2d/RenderQueue.h"

#include "k2d/CanvasRenderer.h"

#include <ct/sort.hpp>

namespace k2d
{

    namespace
    {

        bool ItemLess(const RenderItem &a, const RenderItem &b)
        {
            if (a.zIndex != b.zIndex)
                return a.zIndex < b.zIndex;
            if (a.ySort && b.ySort)
            {
                if (a.y != b.y)
                    return a.y < b.y;
            }
            return a.seq < b.seq;
        }
    } 

    RenderQueue::RenderQueue() : mItems(), mLights(), mDirectionalLights(), mOccluders(), mSeq(0) {}

    RenderQueue::~RenderQueue() {}

    void RenderQueue::Clear()
    {
        mItems.clear();
        mLights.clear();
        mDirectionalLights.clear();
        mOccluders.clear();
        mSeq = 0;
    }

    void RenderQueue::AddLight(const PointLight &light)
    {
        mLights.push_back(light);
    }

    void RenderQueue::AddDirectionalLight(const DirectionalLight &light)
    {
        if (mDirectionalLights.size() < kMaxDirectionalLights)
            mDirectionalLights.push_back(light);
    }

    void RenderQueue::AddOccluder(const Occluder &occluder)
    {
        mOccluders.push_back(occluder);
    }

    RenderItem &RenderQueue::AddItem(int zIndex, bool ySort)
    {
        RenderItem item;
        item.zIndex = zIndex;
        item.ySort = ySort;
        item.seq = mSeq++;
        mItems.push_back(item);
        return mItems.back();
    }

    std::size_t RenderQueue::CommandCount() const
    {
        std::size_t count = 0;
        for (std::size_t i = 0; i < mItems.size(); ++i)
            count += mItems[i].commands.size();
        return count;
    }

    void RenderQueue::Flush(CanvasRenderer &canvas)
    {
        if (mItems.empty())
            return;

        if (mItems.size() > 1)
            ct::sort(mItems.data(), mItems.data() + mItems.size(), ItemLess);

        canvas.DrawItems(mItems.data(), mItems.size(),
                         mLights.data(), mLights.size(),
                         mDirectionalLights.data(), mDirectionalLights.size(),
                         mOccluders.data(), mOccluders.size());
    }

} 