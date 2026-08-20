#pragma once

#include "k2d/Component.h"

#include <glm/glm.hpp>

namespace k2d
{

    class Texture;

    class SpriteComponent : public Component
    {
    public:
        static const ComponentType Type = ComponentType::Sprite;

        explicit SpriteComponent(Texture *texture = nullptr);

        Texture *texture() const;
        void setTexture(Texture *texture);

        const glm::vec2 &size() const;
        void setSize(const glm::vec2 &size);

        const glm::vec2 &pivot() const;
        void setPivot(const glm::vec2 &pivot);

        void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);

        void setSourceRect(float x, float y, float width, float height);
        void clearSourceRect();
        void setFlip(bool flipX, bool flipY);
        bool flipX() const { return mFlipX; }
        bool flipY() const { return mFlipY; }

    protected:
        void onRender(BatchRenderer &batch) override;

    private:
        Texture *mTexture;
        glm::vec2 mSize;
        glm::vec2 mPivot;
        unsigned char mColorR;
        unsigned char mColorG;
        unsigned char mColorB;
        unsigned char mColorA;
        float mSourceX;
        float mSourceY;
        float mSourceW;
        float mSourceH;
        bool mFlipX;
        bool mFlipY;
    };

}
