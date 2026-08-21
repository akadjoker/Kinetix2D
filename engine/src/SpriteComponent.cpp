#include "k2d/SpriteComponent.h"

#include "k2d/Batch.h"
#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"

namespace k2d
{

    SpriteComponent::SpriteComponent(Texture *texture)
        : Component(Type, ComponentEventRender), mTexture(nullptr), mSize(0.0f, 0.0f),
          mPivot(0.5f, 0.5f), mColorR(255), mColorG(255), mColorB(255), mColorA(255), mSourceX(0.0f),
          mSourceY(0.0f), mSourceW(0.0f), mSourceH(0.0f), mFlipX(false), mFlipY(false), mYSort(false),
          mBlendMode(BLEND_MIX)
    {
        setTexture(texture);
    }

    Texture *SpriteComponent::texture() const
    {
        return mTexture;
    }

    void SpriteComponent::setTexture(Texture *texture)
    {
        mTexture = texture;
        if (mTexture && mSize.x == 0.0f && mSize.y == 0.0f)
            mSize = glm::vec2(static_cast<float>(mTexture->Width()), static_cast<float>(mTexture->Height()));
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
        return mPivot;
    }

    void SpriteComponent::setPivot(const glm::vec2 &pivot)
    {
        mPivot = pivot;
    }

    void SpriteComponent::setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        mColorR = r;
        mColorG = g;
        mColorB = b;
        mColorA = a;
    }

    void SpriteComponent::setSourceRect(float x, float y, float width, float height)
    {
        mSourceX = x;
        mSourceY = y;
        mSourceW = width;
        mSourceH = height;
    }

    void SpriteComponent::clearSourceRect()
    {
        mSourceX = 0.0f;
        mSourceY = 0.0f;
        mSourceW = 0.0f;
        mSourceH = 0.0f;
    }

    void SpriteComponent::setFlip(bool flipX, bool flipY)
    {
        mFlipX = flipX;
        mFlipY = flipY;
    }

    void SpriteComponent::onRender(RenderQueue &queue)
    {
        if (!mTexture)
            return;

        RenderItem &item = queue.AddItem(owner()->zIndex(), mYSort);
        if (mYSort)
            item.y = owner()->globalPosition().y;
        item.xform = owner()->globalTransform();

        RenderCommand rect = RenderCommand::MakeRect(mTexture->Id(), 0.0f, 0.0f, mSize.x, mSize.y);
        rect.srcX = mSourceX;
        rect.srcY = mSourceY;
        rect.srcW = mSourceW;
        rect.srcH = mSourceH;
        rect.texWidth = mTexture->Width();
        rect.texHeight = mTexture->Height();
        rect.pivotX = mPivot.x;
        rect.pivotY = mPivot.y;
        rect.color = BatchRenderer::PackColor(mColorR, mColorG, mColorB, mColorA);
        rect.flags = (unsigned char)((mFlipX ? 1 : 0) | (mFlipY ? 2 : 0));
        item.blendMode = mBlendMode;
        item.commands.push_back(rect);
    }

    void SpriteComponent::setYSort(bool ySort)
    {
        mYSort = ySort;
    }

    void SpriteComponent::setBlendMode(BlendMode mode)
    {
        mBlendMode = mode;
    }

}
