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
        RenderItem &AddItem(int zIndex, bool ySort = false);
        void AddLight(const PointLight &light);
        void AddDirectionalLight(const DirectionalLight &light);
        void AddOccluder(const Occluder &occluder);
        void Flush(CanvasRenderer &canvas);

        std::size_t ItemCount() const { return mItems.size(); }
        std::size_t CommandCount() const;
        std::size_t LightCount() const { return mLights.size(); }
        std::size_t DirectionalLightCount() const { return mDirectionalLights.size(); }
        std::size_t OccluderCount() const { return mOccluders.size(); }

    private:
        ct::Vector<RenderItem> mItems;
        ct::Vector<PointLight> mLights;
        ct::Vector<DirectionalLight> mDirectionalLights;
        ct::Vector<Occluder> mOccluders;
        unsigned int mSeq;
    };

}