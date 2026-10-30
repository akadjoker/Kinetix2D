#include "SpriteCanvas.h"

#include "AssetsPanel.h"
#include "core/EditorFileSystem.h"

#include "Theme.hpp"
#include "WidgetApp.hpp"

#include <k2d/SpriteAtlas.h>

#include <cmath>
#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{

constexpr float kHandleSize = 8.0f;
constexpr float kMinZoom = 0.1f;
constexpr float kMaxZoom = 64.0f;

const rt::Color kRegionColor(100, 200, 255, 200);
const rt::Color kHoverColor(160, 225, 255, 255);
const rt::Color kSelectedColor(255, 205, 65, 255);
const rt::Color kCreateColor(255, 140, 60, 255);

void outline(rt::PaintContext& ctx, float x, float y, float w, float h, float thickness)
{
    ctx.fillRect(x, y, w, thickness);
    ctx.fillRect(x, y + h - thickness, w, thickness);
    ctx.fillRect(x, y, thickness, h);
    ctx.fillRect(x + w - thickness, y, thickness, h);
}

// Which edges a handle moves: left, top, right, bottom. Handles go round from
// the top-left corner, clockwise.
const bool kHandleEdges[8][4] = {
    {true, true, false, false},  {false, true, false, false}, {false, true, true, false},
    {false, false, true, false}, {false, false, true, true},  {false, false, false, true},
    {true, false, false, true},  {true, false, false, false},
};

} // namespace

SpriteCanvas::SpriteCanvas()
{
    acceptsFocus_ = true;
    setTabStop(false);
}

void SpriteCanvas::setTexture(rt::TextureHandle texture, int width, int height)
{
    mTexture = texture;
    mWidth = width;
    mHeight = height;
    mSelected = -1;
    mHovered = -1;
    mDrag = Drag::None;
    mFitPending = true;
    markDirty();
}

void SpriteCanvas::setSelected(int region)
{
    const int count = mAtlas ? static_cast<int>(mAtlas->regionCount()) : 0;
    mSelected = region >= 0 && region < count ? region : -1;
    markDirty();
}

void SpriteCanvas::fit()
{
    if (mWidth <= 0 || mHeight <= 0 || rect_.w <= 1.0f || rect_.h <= 1.0f)
    {
        mFitPending = true;
        return;
    }
    mFitPending = false;
    const float zoom = std::fmin((rect_.w - 24.0f) / mWidth, (rect_.h - 24.0f) / mHeight);
    // Whole steps above 1 keep pixel art crisp.
    mZoom = zoom >= 1.0f ? std::floor(zoom) : std::fmax(kMinZoom, zoom);
    mPan = Math::Vec2((rect_.w - mWidth * mZoom) * 0.5f, (rect_.h - mHeight * mZoom) * 0.5f);
    markDirty();
}

Math::Vec2 SpriteCanvas::toPixel(float localX, float localY) const
{
    return Math::Vec2((localX - mPan.x) / mZoom, (localY - mPan.y) / mZoom);
}

Math::Vec2 SpriteCanvas::toLocal(float pixelX, float pixelY) const
{
    return Math::Vec2(mPan.x + pixelX * mZoom, mPan.y + pixelY * mZoom);
}

int SpriteCanvas::regionAt(const Math::Vec2& pixel) const
{
    // The smallest region under the point, so one inside another can be picked.
    int best = -1;
    float bestArea = 0.0f;
    for (std::size_t i = 0; mAtlas && i < mAtlas->regionCount(); ++i)
    {
        const Math::Vec4& r = mAtlas->regionAt(i)->rect;
        if (pixel.x < r.x || pixel.y < r.y || pixel.x >= r.x + r.z || pixel.y >= r.y + r.w)
            continue;
        const float area = r.z * r.w;
        if (best < 0 || area < bestArea)
        {
            best = static_cast<int>(i);
            bestArea = area;
        }
    }
    return best;
}

int SpriteCanvas::handleAt(float localX, float localY) const
{
    if (!mAtlas || mSelected < 0)
        return -1;
    const Math::Vec4& r = mAtlas->regionAt(static_cast<std::size_t>(mSelected))->rect;
    const Math::Vec2 a = toLocal(r.x, r.y);
    const Math::Vec2 b = toLocal(r.x + r.z, r.y + r.w);
    const float xs[3] = {a.x, (a.x + b.x) * 0.5f, b.x};
    const float ys[3] = {a.y, (a.y + b.y) * 0.5f, b.y};
    const int at[8][2] = {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}};
    for (int i = 0; i < 8; ++i)
        if (std::fabs(localX - xs[at[i][0]]) <= kHandleSize && std::fabs(localY - ys[at[i][1]]) <= kHandleSize)
            return i;
    return -1;
}

Math::Vec4 SpriteCanvas::clampRect(Math::Vec4 r) const
{
    r.z = std::fmax(1.0f, std::fmin(r.z, static_cast<float>(mWidth)));
    r.w = std::fmax(1.0f, std::fmin(r.w, static_cast<float>(mHeight)));
    r.x = std::fmax(0.0f, std::fmin(r.x, mWidth - r.z));
    r.y = std::fmax(0.0f, std::fmin(r.y, mHeight - r.w));
    return r;
}

ct::String SpriteCanvas::nextName() const
{
    char name[96];
    for (int n = static_cast<int>(mAtlas->regionCount()) + 1;; ++n)
    {
        std::snprintf(name, sizeof name, "%s_%03d", mBaseName.c_str(), n);
        if (!mAtlas->find(name))
            return ct::String(name);
    }
}

void SpriteCanvas::applyRect(const Math::Vec4& rect)
{
    if (!mAtlas || mSelected < 0)
        return;
    const ct::String id = mAtlas->regionAt(static_cast<std::size_t>(mSelected))->id;
    mAtlas->set(id.c_str(), id.c_str(), rect);
    markDirty();
}

void SpriteCanvas::paint(rt::PaintContext& ctx)
{
    if (!visible_)
        return;
    const rt::Rect abs = absoluteRect();
    if (ctx.isClipped(abs))
        return;
    if (mFitPending)
        fit();
    ctx.pushClip(abs);
    ctx.fill.SetColor(rt::Color(28, 30, 34, 255));
    ctx.fillRect(abs.x, abs.y, abs.w, abs.h);

    const auto& theme = rt::Theme::instance();
    ctx.font.SetFontSize(theme.fontSize);
    ctx.font.SetBatch(&ctx.text);
    if (!mTexture || mWidth <= 0)
    {
        ctx.font.SetColor(theme.textDisabled);
        ctx.font.Print("Double-click an image in Assets, or drop one here", abs.x + 16.0f, abs.y + 28.0f);
        ctx.popClip();
        return;
    }

    // The image over a checkerboard, so transparent pixels show as such.
    const float imageX = abs.x + mPan.x, imageY = abs.y + mPan.y;
    const float imageW = mWidth * mZoom, imageH = mHeight * mZoom;
    const float left = std::fmax(imageX, abs.x), top = std::fmax(imageY, abs.y);
    const float right = std::fmin(imageX + imageW, abs.x + abs.w), bottom = std::fmin(imageY + imageH, abs.y + abs.h);
    if (right > left && bottom > top)
    {
        ctx.pushClip(rt::Rect{left, top, right - left, bottom - top});
        const float cell = 8.0f;
        ctx.fill.SetColor(rt::Color(60, 62, 68, 255));
        ctx.fillRect(left, top, right - left, bottom - top);
        ctx.fill.SetColor(rt::Color(80, 82, 88, 255));
        const int firstX = static_cast<int>(std::floor((left - imageX) / cell));
        const int firstY = static_cast<int>(std::floor((top - imageY) / cell));
        for (int cy = firstY; imageY + cy * cell < bottom; ++cy)
            for (int cx = firstX; imageX + cx * cell < right; ++cx)
                if (((cx + cy) & 1) == 0)
                    ctx.fillRect(imageX + cx * cell, imageY + cy * cell, cell, cell);
        ctx.popClip();
    }
    ctx.drawImage(mTexture, rt::Rect{imageX, imageY, imageW, imageH}, rt::Rect{0.0f, 0.0f, 1.0f, 1.0f});

    // A pixel grid once pixels are big enough to aim at.
    if (mZoom >= 8.0f && right > left && bottom > top)
    {
        ctx.fill.SetColor(rt::Color(0, 0, 0, 50));
        for (float x = imageX + std::ceil((left - imageX) / mZoom) * mZoom; x < right; x += mZoom)
            ctx.fillRect(x, top, 1.0f, bottom - top);
        for (float y = imageY + std::ceil((top - imageY) / mZoom) * mZoom; y < bottom; y += mZoom)
            ctx.fillRect(left, y, right - left, 1.0f);
    }

    const float asc = ctx.font.GetAscender();
    for (std::size_t i = 0; mAtlas && i < mAtlas->regionCount(); ++i)
    {
        const SpriteRegion* region = mAtlas->regionAt(i);
        const Math::Vec2 a = toLocal(region->rect.x, region->rect.y);
        const float w = region->rect.z * mZoom, h = region->rect.w * mZoom;
        const bool selected = static_cast<int>(i) == mSelected;
        const bool hovered = static_cast<int>(i) == mHovered;
        ctx.fill.SetColor(selected ? kSelectedColor : (hovered ? kHoverColor : kRegionColor));
        outline(ctx, abs.x + a.x, abs.y + a.y, w, h, selected ? 2.0f : 1.0f);
        if (selected || hovered || w > 48.0f)
        {
            ctx.font.SetColor(selected ? kSelectedColor : rt::Color(220, 225, 235, 220));
            const ct::String shown = selected || hovered ? region->id : ctx.fitText(region->id, w);
            ctx.font.Print(shown.c_str(), abs.x + a.x + 2.0f, abs.y + a.y - 4.0f - theme.fontSize + asc);
        }
    }

    if (mDrag == Drag::Create)
    {
        const Math::Vec2 a = toLocal(mLiveRect.x, mLiveRect.y);
        ctx.fill.SetColor(rt::Color(255, 140, 60, 40));
        ctx.fillRect(abs.x + a.x, abs.y + a.y, mLiveRect.z * mZoom, mLiveRect.w * mZoom);
        ctx.fill.SetColor(kCreateColor);
        outline(ctx, abs.x + a.x, abs.y + a.y, mLiveRect.z * mZoom, mLiveRect.w * mZoom, 2.0f);
    }
    else if (mAtlas && mSelected >= 0)
    {
        const Math::Vec4& r = mAtlas->regionAt(static_cast<std::size_t>(mSelected))->rect;
        const Math::Vec2 a = toLocal(r.x, r.y);
        const Math::Vec2 b = toLocal(r.x + r.z, r.y + r.w);
        const float xs[3] = {a.x, (a.x + b.x) * 0.5f, b.x};
        const float ys[3] = {a.y, (a.y + b.y) * 0.5f, b.y};
        const int at[8][2] = {{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}};
        for (int i = 0; i < 8; ++i)
        {
            const float hx = abs.x + xs[at[i][0]] - kHandleSize * 0.5f;
            const float hy = abs.y + ys[at[i][1]] - kHandleSize * 0.5f;
            ctx.fill.SetColor(rt::Color(20, 24, 31, 255));
            ctx.fillRect(hx - 1.0f, hy - 1.0f, kHandleSize + 2.0f, kHandleSize + 2.0f);
            ctx.fill.SetColor(i == mHandle ? kCreateColor : kSelectedColor);
            ctx.fillRect(hx, hy, kHandleSize, kHandleSize);
        }
    }

    char status[96];
    std::snprintf(status, sizeof status, "%dx%d  %.0f%%", mWidth, mHeight, mZoom * 100.0f);
    ctx.font.SetColor(theme.textDisabled);
    ctx.font.Print(status, abs.x + 8.0f, abs.y + abs.h - 8.0f);
    ctx.popClip();
}

void SpriteCanvas::onMousePress(rt::MouseEvent& e)
{
    rt::Widget::onMousePress(e);
    if (rt::WidgetApp* app = rt::WidgetApp::existing())
        app->setFocused(this);
    e.consumed = true;
    if (!mTexture || !mAtlas)
        return;
    if (e.button != 0)
    {
        mDrag = Drag::Pan;
        return;
    }
    const Math::Vec2 pixel = toPixel(e.localX, e.localY);
    mDragStart = Math::Vec2(std::floor(pixel.x), std::floor(pixel.y));
    mHandle = handleAt(e.localX, e.localY);
    if (mHandle >= 0)
    {
        mDrag = Drag::Resize;
        mStartRect = mAtlas->regionAt(static_cast<std::size_t>(mSelected))->rect;
        return;
    }
    const int hit = regionAt(pixel);
    if (hit >= 0)
    {
        if (hit != mSelected)
        {
            mSelected = hit;
            selectionChanged.emit(mSelected);
        }
        mDrag = Drag::Move;
        mStartRect = mAtlas->regionAt(static_cast<std::size_t>(hit))->rect;
        mLiveRect = mStartRect;
        markDirty();
        return;
    }
    mDrag = Drag::Create;
    mLiveRect = clampRect(Math::Vec4(mDragStart.x, mDragStart.y, 1.0f, 1.0f));
    markDirty();
}

void SpriteCanvas::onMouseMove(rt::MouseEvent& e)
{
    rt::Widget::onMouseMove(e);
    if (mDrag == Drag::Pan)
    {
        mPan.x += e.relX;
        mPan.y += e.relY;
        markDirty();
        return;
    }
    const Math::Vec2 pixel = toPixel(e.localX, e.localY);
    const float px = std::round(pixel.x), py = std::round(pixel.y);
    if (mDrag == Drag::Create)
    {
        const float x0 = std::fmin(mDragStart.x, px), y0 = std::fmin(mDragStart.y, py);
        const float x1 = std::fmax(mDragStart.x + 1.0f, px), y1 = std::fmax(mDragStart.y + 1.0f, py);
        const float cx0 = std::fmax(0.0f, x0), cy0 = std::fmax(0.0f, y0);
        const float cx1 = std::fmin(static_cast<float>(mWidth), x1), cy1 = std::fmin(static_cast<float>(mHeight), y1);
        mLiveRect = Math::Vec4(cx0, cy0, std::fmax(1.0f, cx1 - cx0), std::fmax(1.0f, cy1 - cy0));
        markDirty();
    }
    else if (mDrag == Drag::Move)
    {
        mLiveRect = clampRect(Math::Vec4(mStartRect.x + std::floor(pixel.x) - mDragStart.x,
                                         mStartRect.y + std::floor(pixel.y) - mDragStart.y, mStartRect.z,
                                         mStartRect.w));
        applyRect(mLiveRect);
    }
    else if (mDrag == Drag::Resize)
    {
        const bool* edges = kHandleEdges[mHandle];
        float l = mStartRect.x, t = mStartRect.y, r = mStartRect.x + mStartRect.z, b = mStartRect.y + mStartRect.w;
        if (edges[0])
            l = std::fmax(0.0f, std::fmin(px, r - 1.0f));
        if (edges[1])
            t = std::fmax(0.0f, std::fmin(py, b - 1.0f));
        if (edges[2])
            r = std::fmin(static_cast<float>(mWidth), std::fmax(px, l + 1.0f));
        if (edges[3])
            b = std::fmin(static_cast<float>(mHeight), std::fmax(py, t + 1.0f));
        mLiveRect = Math::Vec4(l, t, r - l, b - t);
        applyRect(mLiveRect);
    }
    else
    {
        const int hovered = mTexture && mAtlas ? regionAt(pixel) : -1;
        const bool onHandle = handleAt(e.localX, e.localY) >= 0;
        cursor_ = onHandle || hovered >= 0 ? rt::CursorType::SizeAll : rt::CursorType::Crosshair;
        if (hovered != mHovered)
        {
            mHovered = hovered;
            markDirty();
        }
    }
}

void SpriteCanvas::onMouseRelease(rt::MouseEvent& e)
{
    rt::Widget::onMouseRelease(e);
    const Drag drag = mDrag;
    mDrag = Drag::None;
    mHandle = -1;
    e.consumed = true;
    if (drag == Drag::Create)
    {
        // A click without a drag only clears the selection.
        if (mLiveRect.z * mZoom >= 3.0f && mLiveRect.w * mZoom >= 3.0f)
        {
            mAtlas->add(nextName().c_str(), mLiveRect);
            mSelected = static_cast<int>(mAtlas->regionCount()) - 1;
            edited.emit();
        }
        else
            mSelected = -1;
        selectionChanged.emit(mSelected);
    }
    else if ((drag == Drag::Move || drag == Drag::Resize) &&
             (mLiveRect.x != mStartRect.x || mLiveRect.y != mStartRect.y || mLiveRect.z != mStartRect.z ||
              mLiveRect.w != mStartRect.w))
        edited.emit();
    markDirty();
}

void SpriteCanvas::onMouseScroll(rt::MouseEvent& e)
{
    e.consumed = true;
    if (!mTexture)
        return;
    // About the pointer: the pixel under it stays under it.
    const Math::Vec2 pixel = toPixel(e.localX, e.localY);
    mZoom = std::fmax(kMinZoom, std::fmin(kMaxZoom, mZoom * std::pow(1.2f, e.scrollY)));
    mPan = Math::Vec2(e.localX - pixel.x * mZoom, e.localY - pixel.y * mZoom);
    markDirty();
}

void SpriteCanvas::onKeyPress(rt::KeyEvent& e)
{
    if (!mAtlas || mSelected < 0)
        return;
    if (e.key == rt::Key::Delete || e.key == rt::Key::Backspace)
    {
        const ct::String id = mAtlas->regionAt(static_cast<std::size_t>(mSelected))->id;
        mAtlas->remove(id.c_str());
        mSelected = -1;
        selectionChanged.emit(mSelected);
        edited.emit();
        e.consumed = true;
        markDirty();
        return;
    }
    const float step = e.shift ? 10.0f : 1.0f;
    float dx = 0.0f, dy = 0.0f;
    if (e.key == rt::Key::Left)
        dx = -step;
    else if (e.key == rt::Key::Right)
        dx = step;
    else if (e.key == rt::Key::Up)
        dy = -step;
    else if (e.key == rt::Key::Down)
        dy = step;
    else
        return;
    Math::Vec4 r = mAtlas->regionAt(static_cast<std::size_t>(mSelected))->rect;
    r.x += dx;
    r.y += dy;
    applyRect(clampRect(r));
    edited.emit();
    e.consumed = true;
}

bool SpriteCanvas::acceptsDrop(const rt::DragPayload& payload)
{
    const ct::String* path = std::any_cast<ct::String>(&payload.data);
    if (payload.type != kAssetPayload || !path)
        return false;
    const ct::String ext = EditorFileSystem::extension(*path);
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "tga";
}

void SpriteCanvas::onDropReceive(const rt::DragPayload& payload)
{
    if (const ct::String* path = std::any_cast<ct::String>(&payload.data))
        imageDropped.emit(*path);
}

} // namespace k2d::editor
