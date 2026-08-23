#include "k2d/SpriteComponent.h"

#include "k2d/Batch.h"
#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"

namespace k2d
{

    SpriteComponent::SpriteComponent(Texture *texture)
        : Component(Type, ComponentEventRender), mMaterial(), mSize(0.0f, 0.0f), mYSort(false)
    {
        setTexture(texture);
    }

    Texture *SpriteComponent::texture() const
    {
        return mMaterial.texture();
    }

    void SpriteComponent::setTexture(Texture *texture)
    {
        mMaterial.setTexture(texture);
        if (texture && mSize.x == 0.0f && mSize.y == 0.0f)
            mSize = glm::vec2(static_cast<float>(texture->Width()), static_cast<float>(texture->Height()));
    }

    const glm::vec2 &SpriteComponent::size() const
    {
        return mSize;
    }

    void SpriteComponent::setSize(const glm::vec2 &size)
    {
        mSize = size;
    }

    const glm::vec2 &SpriteComponent::pivot() const
    {
        return mMaterial.pivot();
    }

    void SpriteComponent::setPivot(const glm::vec2 &pivot)
    {
        mMaterial.setPivot(pivot);
    }

    void SpriteComponent::setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        mMaterial.setColor(r, g, b, a);
    }

    void SpriteComponent::setSourceRect(float x, float y, float width, float height)
    {
        mMaterial.setSourceRect(x, y, width, height);
    }

    void SpriteComponent::clearSourceRect()
    {
        mMaterial.clearSourceRect();
    }

    void SpriteComponent::setTiling(float tileX, float tileY)
    {
        mMaterial.setTiling(tileX, tileY);
    }

    void SpriteComponent::setFlip(bool flipX, bool flipY)
    {
        mMaterial.setFlip(flipX, flipY);
    }

    void SpriteComponent::onRender(RenderQueue &queue)
    {
        if (!mMaterial.texture())
            return;

        RenderItem &item = queue.AddItem(owner()->zIndex(), mYSort);
        if (mYSort)
            item.y = owner()->globalPosition().y;
        item.xform = owner()->globalTransform();

        RenderCommand rect = RenderCommand::MakeRect(mMaterial.texture()->Id(), 0.0f, 0.0f, mSize.x, mSize.y);
        if (mMaterial.hasSourceRect())
        {
            rect.srcX = mMaterial.sourceRect().x;
            rect.srcY = mMaterial.sourceRect().y;
            rect.srcW = mMaterial.sourceRect().z;
            rect.srcH = mMaterial.sourceRect().w;
        }
        else if (mMaterial.tiling().x != 1.0f || mMaterial.tiling().y != 1.0f)
        {

            rect.srcX = 0.0f;
            rect.srcY = 0.0f;
            rect.srcW = mMaterial.texture()->Width() * mMaterial.tiling().x;
            rect.srcH = mMaterial.texture()->Height() * mMaterial.tiling().y;
        }
        rect.texWidth = mMaterial.texture()->Width();
        rect.texHeight = mMaterial.texture()->Height();
        rect.pivotX = mMaterial.pivot().x;
        rect.pivotY = mMaterial.pivot().y;
        const glm::vec4 &color = mMaterial.color();
        rect.color = BatchRenderer::PackColor((unsigned char)(color.r * 255.0f),
                                              (unsigned char)(color.g * 255.0f),
                                              (unsigned char)(color.b * 255.0f),
                                              (unsigned char)(color.a * 255.0f));
        rect.flags = (unsigned char)((mMaterial.flipX() ? 1 : 0) |
                                     (mMaterial.flipY() ? 2 : 0));
        rect.lightMask = mMaterial.lightMask();
        rect.normalTextureId = mMaterial.normalMap() ? mMaterial.normalMap()->Id() : 0;
        rect.customProgram = mMaterial.customShader();
        item.blendMode = mMaterial.blendMode();
        item.commands.push_back(rect);
    }

    void SpriteComponent::setYSort(bool ySort)
    {
        mYSort = ySort;
    }

    void SpriteComponent::setBlendMode(BlendMode mode)
    {
        mMaterial.setBlendMode(mode);
    }

}