#pragma once

#include "Retained.hpp"
#include "Signal.hpp"
#include "Widget.hpp"

#include <ct/function.hpp>

namespace k2d::editor
{

class ViewportPanel : public ig::retained::Widget
{
public:
    IG_WIDGET_CLASS(ViewportPanel, ig::retained::Widget);

    ViewportPanel();

    void setTexture(ig::retained::TextureHandle texture);

    ig::retained::Signal<float, float> panned;
    ig::retained::Signal<float, float, float> zoomed;
    // The left button, at points in the view: down, moved while down, up.
    ig::retained::Signal<float, float> pressed;
    ig::retained::Signal<float, float> dragged;
    ig::retained::Signal<float, float> released;

    // Drawn over the image, with the view's rectangle on screen.
    ct::Function<void(ig::retained::PaintContext&, const ig::retained::Rect&)> overlay;

    Vec2f sizeHint() const override;
    void paint(ig::retained::PaintContext& ctx) override;
    void onMousePress(ig::retained::MouseEvent& e) override;
    void onMouseRelease(ig::retained::MouseEvent& e) override;
    void onMouseMove(ig::retained::MouseEvent& e) override;
    void onMouseScroll(ig::retained::MouseEvent& e) override;

private:
    ig::retained::TextureHandle mTexture;
    bool mPanning = false;
    bool mDragging = false;
};

} // namespace k2d::editor
