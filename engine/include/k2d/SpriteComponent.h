#pragma once

#include "k2d/CanvasTypes.h"
#include "k2d/Component.h"
#include "k2d/Material2D.h"

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
        Material2D &material() { return mMaterial; }
        const Material2D &material() const { return mMaterial; }

        const glm::vec2 &size() const;
        void setSize(const glm::vec2 &size);

        const glm::vec2 &pivot() const;
        void setPivot(const glm::vec2 &pivot);

        void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);

        void setSourceRect(float x, float y, float width, float height);
        void clearSourceRect();
        void setFlip(bool flipX, bool flipY);
        bool flipX() const { return mMaterial.flipX(); }
        bool flipY() const { return mMaterial.flipY(); }
        void setYSort(bool ySort);
        bool ySort() const { return mYSort; }
        void setBlendMode(BlendMode mode);
        BlendMode blendMode() const { return mMaterial.blendMode(); }

    protected:
        void onRender(RenderQueue &queue) override;

    private:
        Material2D mMaterial;
        glm::vec2 mSize;
        bool mYSort;
    };

}
