#include "k2d/LightOccluder2D.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"

namespace k2d
{

    LightOccluder2D::LightOccluder2D()
        : Component(Type, ComponentEventRender), mPoints(), mVersion(0)
    {
    }

    void LightOccluder2D::setPolygon(const glm::vec2 *points, int count)
    {
        mPoints.clear();
        for (int i = 0; i < count; ++i)
            mPoints.push_back(points[i]);
        mVersion++;
    }

    void LightOccluder2D::onRender(RenderQueue &queue)
    {
        if (mPoints.empty())
            return;

        Occluder occluder;
        occluder.xform = owner()->globalTransform();
        occluder.points = &mPoints;
        occluder.version = mVersion;
        queue.AddOccluder(occluder);
    }

} // namespace k2d
