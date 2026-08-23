#include "k2d/NinePatchComponent.h"

#include "k2d/GameObject.h"
#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"

namespace k2d
{

    NinePatchComponent::NinePatchComponent()
        : Component(Type, ComponentEventRender), mTexture(nullptr), mSize(64.0f, 64.0f),
          mMarginLeft(8.0f), mMarginTop(8.0f), mMarginRight(8.0f), mMarginBottom(8.0f),
          mPivot(0.0f, 0.0f), mColor(0xFFFFFFFFu)
    {
    }

    void NinePatchComponent::setMargins(float left, float top, float right, float bottom)
    {
        mMarginLeft = left;
        mMarginTop = top;
        mMarginRight = right;
        mMarginBottom = bottom;
    }

    void NinePatchComponent::setColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {
        mColor = (unsigned int)r | ((unsigned int)g << 8) |
                 ((unsigned int)b << 16) | ((unsigned int)a << 24);
    }

    void NinePatchComponent::onRender(RenderQueue &queue)
    {
        if (!mTexture)
            return;

        float texW = (float)mTexture->Width();
        float texH = (float)mTexture->Height();

        float dstColW[3] = {mMarginLeft, mSize.x - mMarginLeft - mMarginRight, mMarginRight};
        float dstRowH[3] = {mMarginTop, mSize.y - mMarginTop - mMarginBottom, mMarginBottom};
        float srcColW[3] = {mMarginLeft, texW - mMarginLeft - mMarginRight, mMarginRight};
        float srcRowH[3] = {mMarginTop, texH - mMarginTop - mMarginBottom, mMarginBottom};

        RenderItem &item = queue.AddItem(owner()->zIndex());
        item.xform = owner()->globalTransform();

        glm::vec2 origin = -mPivot * mSize;
        float dstY = origin.y;
        float srcY = 0.0f;
        for (int row = 0; row < 3; ++row)
        {
            float dstX = origin.x;
            float srcX = 0.0f;
            for (int col = 0; col < 3; ++col)
            {
                if (dstColW[col] > 0.0f && dstRowH[row] > 0.0f &&
                    srcColW[col] > 0.0f && srcRowH[row] > 0.0f)
                {
                    RenderCommand rect = RenderCommand::MakeRect(mTexture->Id(), dstX, dstY,
                                                                  dstColW[col], dstRowH[row]);
                    rect.srcX = srcX;
                    rect.srcY = srcY;
                    rect.srcW = srcColW[col];
                    rect.srcH = srcRowH[row];
                    rect.texWidth = (int)texW;
                    rect.texHeight = (int)texH;
                    rect.pivotX = 0.0f;
                    rect.pivotY = 0.0f;
                    rect.color = mColor;
                    item.commands.push_back(rect);
                }
                dstX += dstColW[col];
                srcX += srcColW[col];
            }
            dstY += dstRowH[row];
            srcY += srcRowH[row];
        }
    }

} // namespace k2d
