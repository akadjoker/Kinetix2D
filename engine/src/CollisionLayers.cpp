#include "k2d/CollisionLayers.h"

namespace k2d
{

    namespace
    {
        ct::String &layerName(int layer)
        {
            static ct::String names[kCollisionLayers];
            return names[layer];
        }
    }

    void SetCollisionLayerName(int layer, const char *name)
    {
        if (layer >= 0 && layer < kCollisionLayers)
            layerName(layer) = name ? name : "";
    }

    const ct::String &CollisionLayerName(int layer)
    {
        static const ct::String none;
        return layer >= 0 && layer < kCollisionLayers ? layerName(layer) : none;
    }

    int CollisionLayerIndex(const char *name)
    {
        if (!name || !name[0])
            return -1;
        for (int layer = 0; layer < kCollisionLayers; ++layer)
            if (layerName(layer) == name)
                return layer;
        return -1;
    }

    void ClearCollisionLayerNames()
    {
        for (int layer = 0; layer < kCollisionLayers; ++layer)
            layerName(layer).clear();
    }

}
