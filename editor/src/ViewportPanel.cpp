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
    e.consumed = true;
}

void ViewportPanel::onMouseRelease(rt::MouseEvent& e)
{
    rt::Widget::onMouseRelease(e);
    mPanning = false;
    e.consumed = true;
}

void ViewportPanel::onMouseMove(rt::MouseEvent& e)
{
    rt::Widget::onMouseMove(e);
    if (mPanning)
        panned.emit(e.relX, e.relY);
}

void ViewportPanel::onMouseScroll(rt::MouseEvent& e)
{
    zoomed.emit(e.scrollY, e.localX, e.localY);
    e.consumed = true;
}

} // namespace k2d::editor
