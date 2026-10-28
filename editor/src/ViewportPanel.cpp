#include "ViewportPanel.h"

#include "Theme.hpp"
#include "WidgetApp.hpp"

namespace rt = ig::retained;

namespace k2d::editor
{

ViewportPanel::ViewportPanel()
{
    acceptsFocus_ = true;
    setTabStop(false);
}

void ViewportPanel::setTexture(rt::TextureHandle texture)
{
    mTexture = texture;
    markDirty();
}

ViewportPanel::Vec2f ViewportPanel::sizeHint() const
{
    return {640.0f, 360.0f};
}

void ViewportPanel::paint(rt::PaintContext& ctx)
{
    if (!visible_)
        return;
    const rt::Rect abs = absoluteRect();
    if (ctx.isClipped(abs))
        return;
    ctx.pushClip(abs);
    if (mTexture)
        ctx.drawImage(mTexture, abs, rt::Rect{0.0f, 1.0f, 1.0f, -1.0f});
    else
    {
        ctx.fill.SetColor(rt::Theme::instance().bgColor);
        ctx.fill.Rectangle(abs.x, abs.y, abs.w, abs.h, true);
    }
    if (overlay)
        overlay(ctx, abs);
    for (rt::Widget* child : children_)
        child->paint(ctx);
    ctx.popClip();
}

void ViewportPanel::onMousePress(rt::MouseEvent& e)
{
    rt::Widget::onMousePress(e);
    if (rt::WidgetApp* app = rt::WidgetApp::existing())
        app->setFocused(this);
    mPanning = e.button != 0;
    mDragging = e.button == 0;
    if (mDragging)
        pressed.emit(e.localX, e.localY, e.clickCount, e.ctrl);
    e.consumed = true;
}

void ViewportPanel::onMouseRelease(rt::MouseEvent& e)
{
    rt::Widget::onMouseRelease(e);
    mPanning = false;
    if (mDragging)
        released.emit(e.localX, e.localY);
    mDragging = false;
    e.consumed = true;
}

void ViewportPanel::onMouseMove(rt::MouseEvent& e)
{
    rt::Widget::onMouseMove(e);
    if (mPanning)
        panned.emit(e.relX, e.relY);
    else if (mDragging)
        dragged.emit(e.localX, e.localY);
    else
        hovered.emit(e.localX, e.localY);
}

void ViewportPanel::onMouseScroll(rt::MouseEvent& e)
{
    zoomed.emit(e.scrollY, e.localX, e.localY);
    e.consumed = true;
}

bool ViewportPanel::acceptsDrop(const rt::DragPayload& payload)
{
    return acceptDrop && acceptDrop(payload);
}

void ViewportPanel::onDropReceive(const rt::DragPayload& payload)
{
    const rt::Rect abs = absoluteRect();
    if (dropped)
        dropped(payload, payload.dropX - abs.x, payload.dropY - abs.y);
}

} // namespace k2d::editor
