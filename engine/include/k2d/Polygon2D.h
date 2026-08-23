#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"
#include "k2d/Color.h"

#include <ct/vector.hpp>
#include <glm/glm.hpp>

namespace k2d
{

    class Texture;

    class Polygon2D : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Polygon2D;

        Polygon2D();

        void setPolygon(const glm::vec2 *points, int count);
        const ct::Vector<glm::vec2> &polygon() const { return mPolygon; }
        const ct::Vector<glm::vec2> &triangles() const { return mTriangles; }
        void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
        const Color &color() const { return mColor; }
        void setTexture(Texture *texture);
        Texture *texture() const { return mTexture; }
        void setBlendMode(BlendMode mode) { mBlendMode = mode; }
        BlendMode blendMode() const { return mBlendMode; }
        bool valid() const { return !mTriangles.empty(); }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        ct::Vector<glm::vec2> mPolygon;
        ct::Vector<glm::vec2> mTriangles;
        Texture *mTexture;
        Color mColor;
        BlendMode mBlendMode;
    };

}