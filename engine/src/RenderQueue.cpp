#include "k2d/RenderQueue.h"

#include "k2d/CanvasRenderer.h"
#include "k2d/Profiler.h"

#include <ct/sort.hpp>

namespace k2d
{

namespace
{

bool ItemLess(const RenderItem& a, const RenderItem& b)
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
} // namespace

RenderQueue::RenderQueue()
    : mItems(), mLights(), mDirectionalLights(), mOccluders(), mSeq(0), mItemCount(0), mFirstZIndex(0),
      mHasFirstZIndex(false), mNeedsSort(false)
{
}

RenderQueue::~RenderQueue()
{
}

void RenderQueue::Clear()
{
    // Keep RenderItems (and, importantly, each item's command capacity) alive.
    // A sprite-only scene otherwise performs one small allocation per sprite,
    // per frame, while rebuilding the queue.
    for (std::size_t i = 0; i < mItemCount; ++i)
        mItems[i].commands.clear();
    mItemCount = 0;
    mLights.clear();
    mDirectionalLights.clear();
    mOccluders.clear();
    mSeq = 0;
    mHasFirstZIndex = false;
    mNeedsSort = false;
}

void RenderQueue::AddLight(const PointLight& light)
{
    mLights.push_back(light);
}

void RenderQueue::AddDirectionalLight(const DirectionalLight& light)
{
    if (mDirectionalLights.size() < kMaxDirectionalLights)
        mDirectionalLights.push_back(light);
}

void RenderQueue::AddOccluder(const Occluder& occluder)
{
    mOccluders.push_back(occluder);
}

RenderItem& RenderQueue::AddItem(int zIndex, bool ySort)
{
    RenderItem* item = nullptr;
    if (mItemCount < mItems.size())
        item = &mItems[mItemCount];
    else
        item = &mItems.emplace_back();
    ++mItemCount;

    item->commands.clear();
    item->zIndex = zIndex;
    item->ySort = ySort;
    item->y = 0.0f;
    item->xform = Matrix2D();
    item->blendMode = BLEND_MIX;
    item->seq = mSeq++;

    if (!mHasFirstZIndex)
    {
        mFirstZIndex = zIndex;
        mHasFirstZIndex = true;
    }
    else if (zIndex != mFirstZIndex)
    {
        mNeedsSort = true;
    }

    // Y order depends on the current transform and consequently must be
    // evaluated every frame for any queue that uses it.
    if (ySort)
        mNeedsSort = true;

    return *item;
}

std::size_t RenderQueue::CommandCount() const
{
    std::size_t count = 0;
    for (std::size_t i = 0; i < mItemCount; ++i)
        count += mItems[i].commands.size();
    return count;
}

void RenderQueue::Flush(CanvasRenderer& canvas)
{
    if (mItemCount == 0)
        return;

    if (mNeedsSort && mItemCount > 1)
    {
        ProfileScope scope("render.sort");
        ct::sort(mItems.data(), mItems.data() + mItemCount, ItemLess);
    }

    ProfileScope scope("render.submit");
    canvas.DrawItems(mItems.data(), mItemCount, mLights.data(), mLights.size(), mDirectionalLights.data(),
                     mDirectionalLights.size(), mOccluders.data(), mOccluders.size());
}

} // namespace k2d
