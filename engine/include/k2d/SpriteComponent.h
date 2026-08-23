#pragma once

#include "k2d/CanvasTypes.h"
#include "k2d/Component.h"
#include "k2d/Material2D.h"

#include <mathc.h>

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

        const Math::Vec2 &size() const;
        void setSize(const Math::Vec2 &size);

        const Math::Vec2 &pivot() const;
        void setPivot(const Math::Vec2 &pivot);

        void setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);

        void setSourceRect(float x, float y, float width, float height);
        void clearSourceRect();
        void setTiling(float tileX, float tileY);
        const Math::Vec2 &tiling() const { return mMaterial.tiling(); }
        void setLightMask(unsigned int mask) { mMaterial.setLightMask(mask); }
        unsigned int lightMask() const { return mMaterial.lightMask(); }
        void setNormalMap(Texture *normalMap) { mMaterial.setNormalMap(normalMap); }
        Texture *normalMap() const { return mMaterial.normalMap(); }
        void setCustomShader(unsigned int program) { mMaterial.setCustomShader(program); }
        unsigned int customShader() const { return mMaterial.customShader(); }
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
        Math::Vec2 mSize;
        bool mYSort;
    };

}