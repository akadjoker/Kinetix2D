#pragma once

#include "k2d/CanvasTypes.h"

#include <ct/vector.hpp>

namespace k2d
{

    class CanvasRenderer;

    // Collects canvas items and dispatches them to the canvas renderer, sorted
    // as Godot sorts canvas items (servers/rendering/renderer_canvas_cull.cpp,
    // ItemYSort): z_index ascending, then y-sort by world y, else submission
    // order. The router owns the data (items) and never touches GL.
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
