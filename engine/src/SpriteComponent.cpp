#include "k2d/SpriteComponent.h"

#include "k2d/Batch.h"
#include "k2d/GameObject.h"
#include "k2d/Texture.h"

namespace k2d
{

    SpriteComponent::SpriteComponent(Texture *texture)
        : Component(Type, ComponentEventRender), mTexture(nullptr), mSize(0.0f, 0.0f),
          mPivot(0.5f, 0.5f), mColorR(255), mColorG(255), mColorB(255), mColorA(255), mSourceX(0.0f),
          mSourceY(0.0f), mSourceW(0.0f), mSourceH(0.0f), mFlipX(false), mFlipY(false)
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

    void SpriteComponent::onRender(BatchRenderer &batch)
    {
        if (!mTexture)
            return;

        batch.SetColor(mColorR, mColorG, mColorB, mColorA);
        batch.DrawTexture(mTexture->Id(), owner()->globalTransform(),
                          mSize.x, mSize.y, mTexture->Width(), mTexture->Height(),
                          mPivot.x, mPivot.y,
                          mSourceX, mSourceY, mSourceW, mSourceH,
                          mFlipX, mFlipY);
    }

}
