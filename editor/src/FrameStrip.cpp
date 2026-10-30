#include "FrameStrip.h"

#include "Theme.hpp"
#include "WidgetApp.hpp"

#include <k2d/Animation2D.h>
#include <k2d/Texture.h>

#include <cmath>
#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{
constexpr float kPad = 8.0f;
constexpr float kGap = 6.0f;
} // namespace

int clipFrameCount(const AnimationClip& clip)
{
    return clip.frames.empty() ? clip.frameCount : static_cast<int>(clip.frames.size());
}

bool clipFrameImage(const AnimationClip& clip, int frame, Texture*& texture, Math::Vec4& rect)
{
    if (frame < 0 || frame >= clipFrameCount(clip))
        return false;
    if (!clip.frames.empty())
    {
        const AnimationFrame& f = clip.frames[static_cast<std::size_t>(frame)];
        texture = f.texture;
        rect = f.rect;
        if (texture && (rect.z <= 0.0f || rect.w <= 0.0f))
            rect = Math::Vec4(0.0f, 0.0f, static_cast<float>(texture->Width()), static_cast<float>(texture->Height()));
        return texture != nullptr;
    }
    // A sprite sheet: cells of frameWidth x frameHeight, row by row.
    texture = clip.texture;
    if (!texture || clip.frameWidth <= 0 || clip.frameHeight <= 0)
        return false;
    const float stepX = clip.frameWidth + clip.atlasGap.x;
    const float stepY = clip.frameHeight + clip.atlasGap.y;
    int columns = static_cast<int>((texture->Width() - clip.atlasPadding.x + clip.atlasGap.x) / stepX);
    columns = columns > 0 ? columns : 1;
    rect = Math::Vec4(clip.atlasPadding.x + (frame % columns) * stepX, clip.atlasPadding.y + (frame / columns) * stepY,
                      static_cast<float>(clip.frameWidth), static_cast<float>(clip.frameHeight));
    return true;
}

FrameStrip::FrameStrip()
{
    acceptsFocus_ = true;
    setTabStop(false);
}

void FrameStrip::setClip(AnimationClip* clip)
{
    if (clip != mClip)
    {
        mScroll = 0.0f;
        mSelected = -1;
    }
    mClip = clip;
    mDragging = -1;
    mDropSlot = -1;
    markDirty();
}

void FrameStrip::setSelected(int frame)
{
    mSelected = mClip && frame >= 0 && frame < clipFrameCount(*mClip) ? frame : -1;
    markDirty();
}

void FrameStrip::setPlayhead(int frame)
{
    if (frame != mPlayhead)
    {
        mPlayhead = frame;
        markDirty();
    }
}

float FrameStrip::cellSize() const
{
    const float size = rect_.h - kPad * 2.0f - 16.0f;
    return size > 24.0f ? size : 24.0f;
}

int FrameStrip::frameAt(float localX) const
{
    if (!mClip)
        return -1;
    const float step = cellSize() + kGap;
    const float x = localX - kPad + mScroll;
    const int index = static_cast<int>(std::floor(x / step));
    if (x < 0.0f || index >= clipFrameCount(*mClip) || x - index * step > cellSize())
        return -1;
    return index;
}

int FrameStrip::slotAt(float localX) const
{
    const int count = mClip ? clipFrameCount(*mClip) : 0;
    const float step = cellSize() + kGap;
    const int slot = static_cast<int>(std::floor((localX - kPad + mScroll + step * 0.5f) / step));
    return slot < 0 ? 0 : (slot > count ? count : slot);
}

void FrameStrip::paint(rt::PaintContext& ctx)
{
    if (!visible_)
        return;
    const rt::Rect abs = absoluteRect();
    if (ctx.isClipped(abs))
        return;
    ctx.pushClip(abs);
    ctx.fill.SetColor(rt::Color(28, 30, 34, 255));
    ctx.fillRect(abs.x, abs.y, abs.w, abs.h);
    const auto& theme = rt::Theme::instance();
    ctx.font.SetFontSize(theme.fontSize);
    ctx.font.SetBatch(&ctx.text);
    const float asc = ctx.font.GetAscender();

    const int count = mClip ? clipFrameCount(*mClip) : 0;
    if (count == 0)
    {
        ctx.font.SetColor(theme.textDisabled);
        ctx.font.Print(mClip ? "Drag sprite regions or images here to add frames" : "No clip",
                       abs.x + 16.0f, abs.y + abs.h * 0.5f);
    }
    const float cell = cellSize();
    const float step = cell + kGap;
    for (int i = 0; i < count; ++i)
    {
        const float x = abs.x + kPad + i * step - mScroll;
        if (x + cell < abs.x || x > abs.x + abs.w)
            continue;
        const float y = abs.y + kPad;
        // Checkerboard behind each frame, so transparency reads as such.
        ctx.fill.SetColor(rt::Color(60, 62, 68, 255));
        ctx.fillRect(x, y, cell, cell);
        ctx.fill.SetColor(rt::Color(78, 80, 86, 255));
        const float half = cell * 0.5f;
        ctx.fillRect(x, y, half, half);
        ctx.fillRect(x + half, y + half, half, half);

        Texture* texture = nullptr;
        Math::Vec4 r;
        if (clipFrameImage(*mClip, i, texture, r) && r.z > 0.0f && r.w > 0.0f)
        {
            const float fit = std::fmin(cell / r.z, cell / r.w);
            const float w = r.z * fit, h = r.w * fit;
            rt::TextureHandle handle;
            handle.value = texture->Id();
            const float tw = static_cast<float>(texture->Width()), th = static_cast<float>(texture->Height());
            ctx.drawImage(handle, rt::Rect{x + (cell - w) * 0.5f, y + (cell - h) * 0.5f, w, h},
                          rt::Rect{r.x / tw, r.y / th, r.z / tw, r.w / th});
        }
        const bool selected = i == mSelected;
        ctx.fill.SetColor(selected ? rt::Color(255, 205, 65, 255) : rt::Color(55, 58, 66, 255));
        const float t = selected ? 2.0f : 1.0f;
        ctx.fillRect(x, y, cell, t);
        ctx.fillRect(x, y + cell - t, cell, t);
        ctx.fillRect(x, y, t, cell);
        ctx.fillRect(x + cell - t, y, t, cell);

        char number[16];
        std::snprintf(number, sizeof number, "%d", i);
        ctx.font.SetColor(i == mPlayhead ? rt::Color(120, 220, 140, 255) : theme.textDisabled);
        ctx.font.Print(number, x + 2.0f, y + cell + 2.0f + asc);
        if (i == mPlayhead)
        {
            ctx.fill.SetColor(rt::Color(120, 220, 140, 255));
            ctx.fillRect(x + 16.0f, y + cell + 8.0f, cell - 16.0f, 2.0f);
        }
    }

    // Where a dragged frame, or a drop, would go.
    const int slot = mDragging >= 0 ? mDropSlot : -1;
    if (slot >= 0)
    {
        ctx.fill.SetColor(rt::Color(100, 170, 255, 255));
        ctx.fillRect(abs.x + kPad + slot * step - mScroll - kGap * 0.5f - 1.0f, abs.y + kPad - 2.0f, 2.0f,
                     cell + 4.0f);
    }
    ctx.popClip();
}

void FrameStrip::onMousePress(rt::MouseEvent& e)
{
    rt::Widget::onMousePress(e);
    if (rt::WidgetApp* app = rt::WidgetApp::existing())
        app->setFocused(this);
    e.consumed = true;
    if (e.button != 0)
        return;
    const int hit = frameAt(e.localX);
    if (hit != mSelected)
    {
        mSelected = hit;
        selectionChanged.emit(hit);
    }
    mDragging = hit;
    mDropSlot = -1;
    mPressX = e.localX;
    markDirty();
}

void FrameStrip::onMouseMove(rt::MouseEvent& e)
{
    rt::Widget::onMouseMove(e);
    if (mDragging >= 0 && std::fabs(e.localX - mPressX) > 4.0f)
    {
        mDropSlot = slotAt(e.localX);
        markDirty();
    }
}

void FrameStrip::onMouseRelease(rt::MouseEvent& e)
{
    rt::Widget::onMouseRelease(e);
    const int from = mDragging;
    const int slot = mDropSlot;
    mDragging = -1;
    mDropSlot = -1;
    markDirty();
    // A slot right before or after the frame leaves it where it is.
    if (from >= 0 && slot >= 0 && slot != from && slot != from + 1)
        moved.emit(from, slot > from ? slot - 1 : slot);
}

void FrameStrip::onMouseScroll(rt::MouseEvent& e)
{
    if (!mClip)
        return;
    const float content = kPad * 2.0f + clipFrameCount(*mClip) * (cellSize() + kGap);
    const float most = content > rect_.w ? content - rect_.w : 0.0f;
    mScroll -= (e.scrollY != 0.0f ? e.scrollY : e.scrollX) * (cellSize() + kGap);
    mScroll = mScroll < 0.0f ? 0.0f : (mScroll > most ? most : mScroll);
    e.consumed = true;
    markDirty();
}

void FrameStrip::onKeyPress(rt::KeyEvent& e)
{
    if (!mClip || mSelected < 0)
        return;
    const int count = clipFrameCount(*mClip);
    if (e.key == rt::Key::Delete || e.key == rt::Key::Backspace)
        removeRequested.emit(mSelected);
    else if (e.key == rt::Key::Left && mSelected > 0)
        selectionChanged.emit(--mSelected);
    else if (e.key == rt::Key::Right && mSelected + 1 < count)
        selectionChanged.emit(++mSelected);
    else
        return;
    e.consumed = true;
    markDirty();
}

bool FrameStrip::acceptsDrop(const rt::DragPayload& payload)
{
    return mClip && acceptDrop && acceptDrop(payload);
}

void FrameStrip::onDropReceive(const rt::DragPayload& payload)
{
    if (dropped)
        dropped(payload, slotAt(payload.dropX - absoluteRect().x));
}

} // namespace k2d::editor
