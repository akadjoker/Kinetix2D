#pragma once

#include "Signal.hpp"
#include "Widget.hpp"

#include <ct/string.hpp>
#include <mathc.h>

namespace k2d
{
class SpriteAtlas;
}

namespace k2d::editor
{

// A texture with the regions of its atlas drawn on it, edited in place: a drag
// on empty pixels makes a region, a click picks one, a drag moves it, the eight
// handles resize it. The wheel zooms about the pointer, the middle or right
// button pans; Delete removes the selection and the arrows nudge it.
class SpriteCanvas : public ig::retained::Widget
{
public:
    IG_WIDGET_CLASS(SpriteCanvas, ig::retained::Widget);

    SpriteCanvas();

    void setAtlas(SpriteAtlas* atlas) { mAtlas = atlas; }
    void setTexture(ig::retained::TextureHandle texture, int width, int height);
    void setSelected(int region);
    int selected() const { return mSelected; }
    // Zoom so the whole texture shows, centred.
    void fit();

    // The region names new regions get: base_001, base_002, ...
    void setBaseName(const ct::String& base) { mBaseName = base; }

    ig::retained::Signal<int> selectionChanged;
    // A region was added, moved, resized or removed.
    ig::retained::Signal<> edited;
    // An image asset was dropped on the canvas, by its project path.
    ig::retained::Signal<const ct::String&> imageDropped;

    Vec2f sizeHint() const override { return {400.0f, 300.0f}; }
    void paint(ig::retained::PaintContext& ctx) override;
    void onMousePress(ig::retained::MouseEvent& e) override;
    void onMouseMove(ig::retained::MouseEvent& e) override;
    void onMouseRelease(ig::retained::MouseEvent& e) override;
    void onMouseScroll(ig::retained::MouseEvent& e) override;
    void onKeyPress(ig::retained::KeyEvent& e) override;
    bool acceptsDrop(const ig::retained::DragPayload& payload) override;
    void onDropReceive(const ig::retained::DragPayload& payload) override;

private:
    enum class Drag
    {
        None,
        Pan,
        Create,
        Move,
        Resize
    };

    Math::Vec2 toPixel(float localX, float localY) const;
    Math::Vec2 toLocal(float pixelX, float pixelY) const;
    int regionAt(const Math::Vec2& pixel) const;
    int handleAt(float localX, float localY) const;
    void applyRect(const Math::Vec4& rect);
    ct::String nextName() const;
    Math::Vec4 clampRect(Math::Vec4 rect) const;

    SpriteAtlas* mAtlas = nullptr;
    ig::retained::TextureHandle mTexture;
    int mWidth = 0;
    int mHeight = 0;
    ct::String mBaseName = "sprite";
    int mSelected = -1;
    int mHovered = -1;
    bool mFitPending = false;

    float mZoom = 1.0f;
    Math::Vec2 mPan;   // the texture's top-left corner in local points

    Drag mDrag = Drag::None;
    int mHandle = -1;
    Math::Vec2 mDragStart;   // in texture pixels
    Math::Vec4 mStartRect;
    Math::Vec4 mLiveRect;    // the rectangle being drawn or changed
};

} // namespace k2d::editor
