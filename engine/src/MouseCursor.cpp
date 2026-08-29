#include "k2d/MouseCursor.h"

#include "k2d/CanvasRenderer.h"
#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"

namespace k2d
{

    MouseCursor::MouseCursor() : mTexture(nullptr), mOffset(0.0f, 0.0f), mPosition(0.0f, 0.0f), mEnabled(false)
    {
    }

    void MouseCursor::setEnabled(bool enabled)
    {
        mEnabled = enabled;
    }

    void MouseCursor::clear()
    {
        mEnabled = false;
        mTexture = nullptr;
        mOffset = Math::Vec2(0.0f, 0.0f);
        mPosition = Math::Vec2(0.0f, 0.0f);
    }

    void MouseCursor::draw(CanvasRenderer &canvas, float screenWidth, float screenHeight) const
    {
        if (!mEnabled || !mTexture || screenWidth <= 0.0f || screenHeight <= 0.0f)
            return;

        canvas.SetOrtho(screenWidth, screenHeight);
        RenderQueue overlay;
        RenderItem &item = overlay.AddItem(0);
        RenderCommand command =
            RenderCommand::MakeRect(mTexture->Id(), mPosition.x - mOffset.x, mPosition.y - mOffset.y,
                                    (float)mTexture->Width(), (float)mTexture->Height());
        command.texWidth = mTexture->Width();
        command.texHeight = mTexture->Height();
        command.pivotX = 0.0f;
        command.pivotY = 0.0f;
        item.commands.push_back(command);
        overlay.Flush(canvas);
    }

    MouseCursor &GetMouseCursor()
    {
        static MouseCursor cursor;
        return cursor;
    }

}
