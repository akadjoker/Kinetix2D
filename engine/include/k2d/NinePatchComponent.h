#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"

#include <glm/glm.hpp>

namespace k2d
{

    class Texture;

    // Godot's NinePatchRect: stretches a texture into a target size while
    // keeping the 4 corners at native pixel size -- for UI panels/borders
    // that must not look stretched at the corners. Splits the source texture
    // into a 3x3 grid by the margins (left/top/right/bottom, in source
    // texture pixels) and emits 9 plain kRect commands (EmitQuad,
    // CanvasRenderer.cpp): the 4 corners unstretched, the 4 edges stretched
    // along one axis, the center stretched both axes.
    class NinePatchComponent : public Component
    {
    public:
        static const ComponentType Type = ComponentType::NinePatch;

        NinePatchComponent();

        void setTexture(Texture *texture) { mTexture = texture; }
        Texture *texture() const { return mTexture; }
        void setSize(const glm::vec2 &size) { mSize = size; }
        const glm::vec2 &size() const { return mSize; }
        // Margins in SOURCE texture pixels -- the untouched corner/edge width.
        void setMargins(float left, float top, float right, float bottom);
        // x=left, y=top, z=right, w=bottom -- same order setMargins() takes.
        glm::vec4 margins() const { return glm::vec4(mMarginLeft, mMarginTop, mMarginRight, mMarginBottom); }
        void setPivot(const glm::vec2 &pivot) { mPivot = pivot; }
        const glm::vec2 &pivot() const { return mPivot; }
        void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
        unsigned int color() const { return mColor; } // packed r|g<<8|b<<16|a<<24, same layout setColor() writes
        void setBlendMode(BlendMode mode) { mBlendMode = mode; }
        BlendMode blendMode() const { return mBlendMode; }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        Texture *mTexture;
        glm::vec2 mSize;
        float mMarginLeft, mMarginTop, mMarginRight, mMarginBottom;
        glm::vec2 mPivot;
        unsigned int mColor;
        BlendMode mBlendMode;
    };

}
