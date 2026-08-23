#pragma once

#include "k2d/Component.h"

#include <ct/vector.hpp>
#include <glm/glm.hpp>

namespace k2d
{

    class Texture;

    // Godot's Line2D: a polyline with width, rendered as a ribbon of quads
    // (one per segment, extended by half the width at each end so
    // consecutive segments overlap and hide the joint gap -- a plain butt
    // join, not Godot's full miter/bevel/round LineJointMode, but visually
    // close for typical widths).
    class Line2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::LinePath;

        Line2D();

        void setPoints(const glm::vec2 *points, int count);
        const ct::Vector<glm::vec2> &points() const { return mPoints; }
        void setWidth(float width);
        float width() const { return mWidth; }
        void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
        void setTexture(Texture *texture) { mTexture = texture; }
        Texture *texture() const { return mTexture; }
        void setClosed(bool closed);
        bool closed() const { return mClosed; }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        void rebuild();

        ct::Vector<glm::vec2> mPoints;
        ct::Vector<glm::vec2> mTriangles;
        float mWidth;
        unsigned int mColor;
        Texture *mTexture;
        bool mClosed;
    };

}
