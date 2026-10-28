#pragma once

#include "Signal.hpp"
#include "Widget.hpp"

#include <ct/function.hpp>
#include <mathc.h>

namespace k2d
{
class Texture;
struct AnimationClip;
}

namespace k2d::editor
{

// The texture and the rectangle a clip shows at a frame, for an explicit frame
// list and for a sprite sheet clip alike; false when there is nothing to show.
bool clipFrameImage(const AnimationClip& clip, int frame, Texture*& texture, Math::Vec4& rect);
int clipFrameCount(const AnimationClip& clip);

// The frames of a clip in a row: a click selects, a drag along the row moves a
// frame, Delete removes the selected one. Sprite regions and images dropped on
// it become frames where they land.
class FrameStrip : public ig::retained::Widget
{
public:
    IG_WIDGET_CLASS(FrameStrip, ig::retained::Widget);

    FrameStrip();

    // The clip shown; the panel resolves it again whenever it can have moved.
    void setClip(AnimationClip* clip);
    void setSelected(int frame);
    int selected() const { return mSelected; }
    // The frame the preview shows, marked under its cell; -1 for none.
    void setPlayhead(int frame);

    ig::retained::Signal<int> selectionChanged;
    ig::retained::Signal<int, int> moved;     // from, to
    ig::retained::Signal<int> removeRequested;
    // A payload dropped at a slot: 0 before the first frame, count after the last.
    ct::Function<bool(const ig::retained::DragPayload&)> acceptDrop;
    ct::Function<void(const ig::retained::DragPayload&, int)> dropped;

    Vec2f sizeHint() const override { return {400.0f, 110.0f}; }
    void paint(ig::retained::PaintContext& ctx) override;
    void onMousePress(ig::retained::MouseEvent& e) override;
    void onMouseMove(ig::retained::MouseEvent& e) override;
    void onMouseRelease(ig::retained::MouseEvent& e) override;
    void onMouseScroll(ig::retained::MouseEvent& e) override;
    void onKeyPress(ig::retained::KeyEvent& e) override;
    bool acceptsDrop(const ig::retained::DragPayload& payload) override;
    void onDropReceive(const ig::retained::DragPayload& payload) override;

private:
    float cellSize() const;
    int frameAt(float localX) const;
    int slotAt(float localX) const;

    AnimationClip* mClip = nullptr;
    int mSelected = -1;
    int mPlayhead = -1;
    float mScroll = 0.0f;
    int mDragging = -1;
    int mDropSlot = -1;
    float mPressX = 0.0f;
};

} // namespace k2d::editor
