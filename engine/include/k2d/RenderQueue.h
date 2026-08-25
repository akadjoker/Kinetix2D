#pragma once

#include "k2d/CanvasTypes.h"

#include <ct/vector.hpp>

namespace k2d
{

class CanvasRenderer;

class RenderQueue
{
  public:
    RenderQueue();
    ~RenderQueue();

    void Clear();
    RenderItem& AddItem(int zIndex, bool ySort = false);
    void AddLight(const PointLight& light);
    void AddDirectionalLight(const DirectionalLight& light);
    void AddOccluder(const Occluder& occluder);
    void Flush(CanvasRenderer& canvas);

    std::size_t ItemCount() const
    {
        return mItemCount;
    }
    std::size_t CommandCount() const;
    std::size_t LightCount() const
    {
        return mLights.size();
    }
    std::size_t DirectionalLightCount() const
    {
        return mDirectionalLights.size();
    }
    std::size_t OccluderCount() const
    {
        return mOccluders.size();
    }

  private:
    ct::Vector<RenderItem> mItems;
    ct::Vector<PointLight> mLights;
    ct::Vector<DirectionalLight> mDirectionalLights;
    ct::Vector<Occluder> mOccluders;
    unsigned int mSeq;
    std::size_t mItemCount;
    // The queue is rebuilt every frame, but the common case has a single
    // layer and no Y sorting.  Its insertion order is already the draw
    // order, so avoid an unnecessary O(n log n) sort in that case.
    int mFirstZIndex;
    bool mHasFirstZIndex;
    bool mNeedsSort;
};

} // namespace k2d
