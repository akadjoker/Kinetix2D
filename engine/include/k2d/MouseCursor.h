#pragma once

#include <mathc.h>

namespace k2d
{

    class CanvasRenderer;
    class Texture;

    class MouseCursor
    {
    public:
        MouseCursor();

        void setEnabled(bool enabled);
        bool enabled() const { return mEnabled; }

        void setTexture(Texture *texture) { mTexture = texture; }
        Texture *texture() const { return mTexture; }

        // Pixels from the image's top-left corner to the point that actually
        // clicks, so a crosshair centres and an arrow keeps its tip.
        void setOffset(const Math::Vec2 &offset) { mOffset = offset; }
        const Math::Vec2 &offset() const { return mOffset; }

        void setPosition(const Math::Vec2 &position) { mPosition = position; }
        const Math::Vec2 &position() const { return mPosition; }

        void clear();

        // Screen-space overlay: call after the scene has rendered, like ScreenFade.
        // Drawing only - hiding the OS pointer is the host's call, because the
        // editor must keep it for its own panels while the game view draws this.
        void draw(CanvasRenderer &canvas, float screenWidth, float screenHeight) const;

    private:
        Texture *mTexture;
        Math::Vec2 mOffset;
        Math::Vec2 mPosition;
        bool mEnabled;
    };

    // Process-wide cursor used by the runner, the editor's Game view and the
    // scripting API, cleared when a game session ends.
    MouseCursor &GetMouseCursor();

}
