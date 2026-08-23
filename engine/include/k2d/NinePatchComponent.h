#pragma once

#include "k2d/Component.h"
#include "k2d/CanvasTypes.h"

#include <glm/glm.hpp>

namespace k2d
{

    class Texture;

    class NinePatchComponent : public Component
    {
    public:
        static const ComponentType Type = ComponentType::NinePatch;

        NinePatchComponent();

        void setTexture(Texture *texture) { mTexture = texture; }
        Texture *texture() const { return mTexture; }
        void setSize(const glm::vec2 &size) { mSize = size; }
        const glm::vec2 &size() const { return mSize; }

        void setMargins(float left, float top, float right, float bottom);

        glm::vec4 margins() const { return glm::vec4(mMarginLeft, mMarginTop, mMarginRight, mMarginBottom); }
        void setPivot(const glm::vec2 &pivot) { mPivot = pivot; }
        const glm::vec2 &pivot() const { return mPivot; }
        void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);
        unsigned int color() const { return mColor; } 
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