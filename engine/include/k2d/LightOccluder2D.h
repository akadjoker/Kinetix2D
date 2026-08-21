#pragma once

#include "k2d/Component.h"

#include <ct/vector.hpp>
#include <glm/glm.hpp>

namespace k2d
{

    // Godot's LightOccluder2D. A polygon that casts shadows from Light2D with
    // shadow enabled. Registers itself into the router each frame; the canvas
    // renderer bakes it into the shadow atlas.
    class LightOccluder2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Occluder;

        LightOccluder2D();

        void setPolygon(const glm::vec2 *points, int count);
        const ct::Vector<glm::vec2> &points() const { return mPoints; }
        unsigned int version() const { return mVersion; }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        ct::Vector<glm::vec2> mPoints;
        unsigned int mVersion;
    };

}
