#include "ImageEditorPanel.h"

#include "AssetsPanel.h"
#include "Icons.h"
#include "PropertyFields.h"
#include "core/EditorFileSystem.h"

#include "BasicWidgets.hpp"
#include "SpinSlider.hpp"
#include "Theme.hpp"
#include "ToolBar.hpp"
#include "TreePropertyColorWidgets.hpp"
#include "WidgetApp.hpp"

#include <k2d/Assets.h>
#include <k2d/Texture.h>

#include <platform.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{
constexpr std::size_t kUndoSteps = 40;
constexpr int kRecentColours = 12;
} // namespace

// The image, zoomed and panned like the Sprite Editor's canvas, painted with
// the panel's tool. Line and rectangle preview while dragged.
class ImageCanvas : public rt::Widget
{
public:
    IG_WIDGET_CLASS(ImageCanvas, rt::Widget);

    explicit ImageCanvas(ImageEditorPanel& panel) : mPanel(panel)
    {
        acceptsFocus_ = true;
        setTabStop(false);
        cursor_ = rt::CursorType::Crosshair;
    }

    void setTexture(rt::TextureHandle texture, int width, int height)
    {
        mTexture = texture;
        mWidth = width;
        mHeight = height;
        mFitPending = true;
        markDirty();
    }

    void fit()
    {
        if (mWidth <= 0 || rect_.w <= 1.0f)
            return;
        mFitPending = false;
        const float zoom = std::fmin((rect_.w - 24.0f) / mWidth, (rect_.h - 24.0f) / mHeight);
        mZoom = zoom >= 1.0f ? std::floor(zoom) : std::fmax(0.1f, zoom);
        mPanX = (rect_.w - mWidth * mZoom) * 0.5f;
        mPanY = (rect_.h - mHeight * mZoom) * 0.5f;
        markDirty();
    }

    rt::Signal<const ct::String&> imageDropped;

    Vec2f sizeHint() const override { return {400.0f, 300.0f}; }

    void paint(rt::PaintContext& ctx) override
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
            ctx.font.Print("Drop an image here, or use Edit pixels in the Sprite Editor", abs.x + 16.0f, abs.y + 28.0f);
            ctx.popClip();
            return;
        }
        const float x = abs.x + mPanX, y = abs.y + mPanY, w = mWidth * mZoom, h = mHeight * mZoom;
        const float left = std::fmax(x, abs.x), top = std::fmax(y, abs.y);
        const float right = std::fmin(x + w, abs.x + abs.w), bottom = std::fmin(y + h, abs.y + abs.h);
        if (right > left && bottom > top)
        {
            ctx.pushClip(rt::Rect{left, top, right - left, bottom - top});
            const float cell = 8.0f;
            ctx.fill.SetColor(rt::Color(60, 62, 68, 255));
            ctx.fillRect(left, top, right - left, bottom - top);
            ctx.fill.SetColor(rt::Color(80, 82, 88, 255));
            for (int cy = static_cast<int>(std::floor((top - y) / cell)); y + cy * cell < bottom; ++cy)
                for (int cx = static_cast<int>(std::floor((left - x) / cell)); x + cx * cell < right; ++cx)
                    if (((cx + cy) & 1) == 0)
                        ctx.fillRect(x + cx * cell, y + cy * cell, cell, cell);
            ctx.popClip();
        }
        ctx.drawImage(mTexture, rt::Rect{x, y, w, h}, rt::Rect{0.0f, 0.0f, 1.0f, 1.0f});
        if (mZoom >= 8.0f && right > left && bottom > top)
        {
            ctx.fill.SetColor(rt::Color(0, 0, 0, 45));
            for (float gx = x + std::ceil((left - x) / mZoom) * mZoom; gx < right; gx += mZoom)
                ctx.fillRect(gx, top, 1.0f, bottom - top);
            for (float gy = y + std::ceil((top - y) / mZoom) * mZoom; gy < bottom; gy += mZoom)
                ctx.fillRect(left, gy, right - left, 1.0f);
        }

        // What the tool would touch: the brush under the pointer, or the shape being dragged.
        unsigned char c[4];
        mPanel.colour(c);
        const rt::Color shown(c[0], c[1], c[2], 255);
        auto box = [&](int px, int py, int pw, int ph, const rt::Color& colour) {
            ctx.fill.SetColor(colour);
            const float bx = x + px * mZoom, by = y + py * mZoom, bw = pw * mZoom, bh = ph * mZoom;
            ctx.fillRect(bx, by, bw, 1.0f);
            ctx.fillRect(bx, by + bh - 1.0f, bw, 1.0f);
            ctx.fillRect(bx, by, 1.0f, bh);
            ctx.fillRect(bx + bw - 1.0f, by, 1.0f, bh);
        };
        if (mDragging && (mPanel.tool() == PixelTool::Line || mPanel.tool() == PixelTool::Rectangle))
        {
            const int x0 = mStartX < mHoverX ? mStartX : mHoverX, x1 = mStartX < mHoverX ? mHoverX : mStartX;
            const int y0 = mStartY < mHoverY ? mStartY : mHoverY, y1 = mStartY < mHoverY ? mHoverY : mStartY;
            if (mPanel.tool() == PixelTool::Rectangle)
                box(x0, y0, x1 - x0 + 1, y1 - y0 + 1, shown);
            else
            {
                // The line's pixels, as DrawLine will set them.
                int lx = mStartX, ly = mStartY;
                const int dx = std::abs(mHoverX - lx), sx = lx < mHoverX ? 1 : -1;
                const int dy = -std::abs(mHoverY - ly), sy = ly < mHoverY ? 1 : -1;
                int error = dx + dy;
                ctx.fill.SetColor(rt::Color(c[0], c[1], c[2], 200));
                for (int guard = 0; guard < 100000; ++guard)
                {
                    ctx.fillRect(x + lx * mZoom, y + ly * mZoom, mZoom, mZoom);
                    if (lx == mHoverX && ly == mHoverY)
                        break;
                    const int e2 = 2 * error;
                    if (e2 >= dy) { error += dy; lx += sx; }
                    if (e2 <= dx) { error += dx; ly += sy; }
                }
            }
        }
        else if (mHovering)
        {
            const int size = brushed() ? mPanel.brushSize() : 1;
            box(mHoverX - size / 2, mHoverY - size / 2, size, size,
                mPanel.tool() == PixelTool::Eraser ? rt::Color(230, 80, 80, 255) : rt::Color(255, 205, 65, 255));
        }

        char status[96];
        std::snprintf(status, sizeof status, "%dx%d  %.0f%%  %d,%d", mWidth, mHeight, mZoom * 100.0f, mHoverX, mHoverY);
        ctx.font.SetColor(theme.textDisabled);
        ctx.font.Print(status, abs.x + 8.0f, abs.y + abs.h - 8.0f);
        ctx.popClip();
    }

    void onMousePress(rt::MouseEvent& e) override
    {
        rt::Widget::onMousePress(e);
        if (rt::WidgetApp* app = rt::WidgetApp::existing())
            app->setFocused(this);
        e.consumed = true;
        if (!mPanel.hasImage())
            return;
        if (e.button != 0)
        {
            mPanning = true;
            return;
        }
        pixelAt(e.localX, e.localY, mStartX, mStartY);
        mHoverX = mStartX;
        mHoverY = mStartY;
        mDragging = true;
        switch (mPanel.tool())
        {
        case PixelTool::Picker:
            pick(mStartX, mStartY);
            mDragging = false;
            break;
        case PixelTool::Fill:
            mPanel.beginEdit();
            fill(mStartX, mStartY);
            mPanel.pixelsChanged();
            mDragging = false;
            break;
        case PixelTool::Pencil:
        case PixelTool::Eraser:
            mPanel.beginEdit();
            stroke(mStartX, mStartY, mStartX, mStartY);
            mPanel.pixelsChanged();
            break;
        case PixelTool::Line:
        case PixelTool::Rectangle:
            break;
        }
        markDirty();
    }

    void onMouseMove(rt::MouseEvent& e) override
    {
        rt::Widget::onMouseMove(e);
        if (mPanning)
        {
            mPanX += e.relX;
            mPanY += e.relY;
            markDirty();
            return;
        }
        int px = 0, py = 0;
        pixelAt(e.localX, e.localY, px, py);
        mHovering = mPanel.hasImage();
        if (mDragging && (mPanel.tool() == PixelTool::Pencil || mPanel.tool() == PixelTool::Eraser) &&
            (px != mHoverX || py != mHoverY))
        {
            stroke(mHoverX, mHoverY, px, py);
            mPanel.pixelsChanged();
        }
        else if (mDragging && mPanel.tool() == PixelTool::Picker)
            pick(px, py);
        if (px != mHoverX || py != mHoverY)
        {
            mHoverX = px;
            mHoverY = py;
            markDirty();
        }
    }

    void onMouseRelease(rt::MouseEvent& e) override
    {
        rt::Widget::onMouseRelease(e);
        e.consumed = true;
        if (mPanning)
        {
            mPanning = false;
            return;
        }
        if (!mDragging)
            return;
        mDragging = false;
        unsigned char c[4];
        mPanel.colour(c);
        Pixmap& image = mPanel.image();
        if (mPanel.tool() == PixelTool::Line)
        {
            mPanel.beginEdit();
            image.DrawLine(mStartX, mStartY, mHoverX, mHoverY, c[0], c[1], c[2], c[3]);
            mPanel.pixelsChanged();
        }
        else if (mPanel.tool() == PixelTool::Rectangle)
        {
            mPanel.beginEdit();
            const int x0 = mStartX < mHoverX ? mStartX : mHoverX, x1 = mStartX < mHoverX ? mHoverX : mStartX;
            const int y0 = mStartY < mHoverY ? mStartY : mHoverY, y1 = mStartY < mHoverY ? mHoverY : mStartY;
            // Shift-less: an outline; the fill tool fills it after.
            image.DrawLine(x0, y0, x1, y0, c[0], c[1], c[2], c[3]);
            image.DrawLine(x0, y1, x1, y1, c[0], c[1], c[2], c[3]);
            image.DrawLine(x0, y0, x0, y1, c[0], c[1], c[2], c[3]);
            image.DrawLine(x1, y0, x1, y1, c[0], c[1], c[2], c[3]);
            mPanel.pixelsChanged();
        }
        markDirty();
    }

    void onMouseLeave() override
    {
        mHovering = false;
        markDirty();
        rt::Widget::onMouseLeave();
    }

    void onMouseScroll(rt::MouseEvent& e) override
    {
        e.consumed = true;
        const float px = (e.localX - mPanX) / mZoom, py = (e.localY - mPanY) / mZoom;
        mZoom = std::fmax(0.1f, std::fmin(64.0f, mZoom * std::pow(1.2f, e.scrollY)));
        mPanX = e.localX - px * mZoom;
        mPanY = e.localY - py * mZoom;
        markDirty();
    }

    bool acceptsDrop(const rt::DragPayload& payload) override
    {
        const ct::String* path = assetPath(payload);
        return path && isImagePath(*path);
    }

    void onDropReceive(const rt::DragPayload& payload) override
    {
        if (const ct::String* path = assetPath(payload))
            imageDropped.emit(*path);
    }

private:
    bool brushed() const { return mPanel.tool() == PixelTool::Pencil || mPanel.tool() == PixelTool::Eraser; }

    void pixelAt(float localX, float localY, int& px, int& py) const
    {
        px = static_cast<int>(std::floor((localX - mPanX) / mZoom));
        py = static_cast<int>(std::floor((localY - mPanY) / mZoom));
    }

    // The brush along a line, so a fast stroke leaves no gaps.
    void stroke(int x0, int y0, int x1, int y1)
    {
        unsigned char c[4];
        mPanel.colour(c);
        if (mPanel.tool() == PixelTool::Eraser)
            c[0] = c[1] = c[2] = c[3] = 0;
        const int size = mPanel.brushSize();
        Pixmap& image = mPanel.image();
        const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        for (int guard = 0; guard < 100000; ++guard)
        {
            image.FillRect(x0 - size / 2, y0 - size / 2, size, size, c[0], c[1], c[2], c[3]);
            if (x0 == x1 && y0 == y1)
                break;
            const int e2 = 2 * error;
            if (e2 >= dy) { error += dy; x0 += sx; }
            if (e2 <= dx) { error += dx; y0 += sy; }
        }
    }

    void pick(int px, int py)
    {
        Pixmap& image = mPanel.image();
        if (px < 0 || py < 0 || px >= image.Width() || py >= image.Height())
            return;
        mPanel.setColour(image.Pixels() + (static_cast<std::size_t>(py) * image.Width() + px) * 4);
    }

    // Every pixel of the clicked one's exact colour, joined to it side by side.
    void fill(int px, int py)
    {
        Pixmap& image = mPanel.image();
        const int w = image.Width(), h = image.Height();
        if (px < 0 || py < 0 || px >= w || py >= h)
            return;
        unsigned char c[4];
        mPanel.colour(c);
        unsigned char* pixels = image.Pixels();
        unsigned char target[4];
        std::memcpy(target, pixels + (static_cast<std::size_t>(py) * w + px) * 4, 4);
        if (std::memcmp(target, c, 4) == 0)
            return;
        ct::Vector<int> stack;
        stack.push_back(py * w + px);
        while (!stack.empty())
        {
            const int at = stack.back();
            stack.pop_back();
            unsigned char* p = pixels + static_cast<std::size_t>(at) * 4;
            if (std::memcmp(p, target, 4) != 0)
                continue;
            std::memcpy(p, c, 4);
            const int x = at % w, y = at / w;
            if (x > 0) stack.push_back(at - 1);
            if (x + 1 < w) stack.push_back(at + 1);
            if (y > 0) stack.push_back(at - w);
            if (y + 1 < h) stack.push_back(at + w);
        }
    }

    ImageEditorPanel& mPanel;
    rt::TextureHandle mTexture;
    int mWidth = 0, mHeight = 0;
    float mZoom = 1.0f, mPanX = 8.0f, mPanY = 8.0f;
    bool mFitPending = true;
    bool mPanning = false, mDragging = false, mHovering = false;
    int mStartX = 0, mStartY = 0, mHoverX = 0, mHoverY = 0;
};

// The colours used last, beside the colour button; a click picks one.
class ColourSwatches : public rt::Widget
{
public:
    IG_WIDGET_CLASS(ColourSwatches, rt::Widget);

    explicit ColourSwatches(ImageEditorPanel& panel) : mPanel(panel) {}

    void remember(const unsigned char c[4])
    {
        for (std::size_t i = 0; i < mColours.size(); ++i)
            if (std::memcmp(mColours[i].rgba, c, 4) == 0)
            {
                mColours.erase(mColours.begin() + i);
                break;
            }
        Swatch s;
        std::memcpy(s.rgba, c, 4);
        mColours.insert(mColours.begin(), s);
        if (mColours.size() > static_cast<std::size_t>(kRecentColours))
            mColours.pop_back();
        markDirty();
    }

    Vec2f sizeHint() const override { return {16.0f * kRecentColours + 4.0f, 22.0f}; }

    void paint(rt::PaintContext& ctx) override
    {
        if (!visible_)
            return;
        const rt::Rect abs = absoluteRect();
        for (std::size_t i = 0; i < mColours.size(); ++i)
        {
            const float x = abs.x + 2.0f + i * 16.0f;
            ctx.fill.SetColor(rt::Color(mColours[i].rgba[0], mColours[i].rgba[1], mColours[i].rgba[2], 255));
            ctx.fillRect(x, abs.y + 5.0f, 13.0f, 13.0f);
        }
    }

    void onMousePress(rt::MouseEvent& e) override
    {
        e.consumed = true;
        const int index = static_cast<int>((e.localX - 2.0f) / 16.0f);
        if (e.localX >= 2.0f && index >= 0 && index < static_cast<int>(mColours.size()))
            mPanel.setColour(mColours[static_cast<std::size_t>(index)].rgba);
    }

private:
    struct Swatch
    {
        unsigned char rgba[4];
    };
    ImageEditorPanel& mPanel;
    ct::Vector<Swatch> mColours;
};

ImageEditorPanel::ImageEditorPanel(Assets& assets) : rt::BoxLayout(rt::LayoutDir::Vertical), mAssets(assets)
{
    setPadding(0.0f);
    setSpacing(0.0f);
    auto* bar = createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    bar->setPadding(2.0f, 4.0f);
    bar->setSpacing(6.0f);

    mTools = bar->createChild<rt::ToolBar>();
    mTools->setDraggable(false);
    mToolItems[0] = mTools->addToggle(rt::IconId::PaintBrush, "Pencil", true);
    mToolItems[1] = mTools->addToggle(rt::IconId::Eraser, "Eraser");
    mToolItems[2] = mTools->addToggle(rt::IconId::PaintBucket, "Fill");
    mToolItems[3] = mTools->addToggle(Icons::get("Line2D"), "Line");
    mToolItems[4] = mTools->addToggle(rt::IconId::SelectRect, "Rectangle");
    mToolItems[5] = mTools->addToggle(rt::IconId::Eyedropper, "Picker");
    mTools->addSeparator();
    const int undoItem = mTools->addButton(Icons::get("Undo"), "Undo (Ctrl+Z)");
    const int redoItem = mTools->addButton(Icons::get("Redo"), "Redo (Ctrl+Y)");
    mTools->addSeparator();
    const int saveItem = mTools->addButton(Icons::get("Save"), "Save the PNG (Ctrl+S)");
    const int revertItem = mTools->addButton(Icons::get("Refresh"), "Revert to the saved PNG");
    const int normalItem = mTools->addButton(Icons::get("Light2D"), "Make a normal map beside the image");
    mTools->toggleChanged.connect([this](int item, bool)
    {
        for (int i = 0; i < 6; ++i)
            if (item == mToolItems[i])
                selectTool(static_cast<PixelTool>(i));
    });
    mTools->buttonClicked.connect([this, undoItem, redoItem, saveItem, revertItem, normalItem](int item)
    {
        if (item == undoItem)
            undo();
        else if (item == redoItem)
            redo();
        else if (item == saveItem)
            save();
        else if (item == revertItem)
            revert();
        else if (item == normalItem)
            normalMap();
    });

    NumberSpec sizeSpec;
    sizeSpec.integer = true;
    sizeSpec.step = 1.0;
    sizeSpec.minimum = 1.0;
    sizeSpec.maximum = 64.0;
    sizeSpec.width = 96.0f;
    mSize = addNumberField(bar, "size", 1.0, sizeSpec, nullptr);

    mColourButton = bar->createChild<rt::ColorButton>(rt::Color(255, 255, 255, 255));
    mColourButton->setShowAlpha(true);
    mColourButton->colorChanged.connect([this](rt::Color c)
    {
        mColour[0] = c.r;
        mColour[1] = c.g;
        mColour[2] = c.b;
        mColour[3] = c.a;
        mSwatches->markDirty();
    });
    mSwatches = bar->createChild<ColourSwatches>(*this);
    mTitle = bar->createChild<rt::Label>("");
    mTitle->setStretch(1.0f);
    mTitle->setLayoutAlign(rt::Align::Center);

    mCanvas = createChild<ImageCanvas>(*this);
    mCanvas->setStretch(1.0f);
    mCanvas->imageDropped.connect([this](const ct::String& path) { openRequested.emit(path); });
}

bool ImageEditorPanel::owns(const rt::Widget* focused) const
{
    for (const rt::Widget* w = focused; w; w = w->parent())
        if (w == this)
            return true;
    return false;
}

int ImageEditorPanel::brushSize() const
{
    return static_cast<int>(mSize->value());
}

void ImageEditorPanel::colour(unsigned char out[4]) const
{
    std::memcpy(out, mColour, 4);
}

void ImageEditorPanel::setColour(const unsigned char rgba[4])
{
    std::memcpy(mColour, rgba, 4);
    syncColourFields();
}

void ImageEditorPanel::syncColourFields()
{
    mColourButton->setColor(rt::Color(mColour[0], mColour[1], mColour[2], mColour[3]));
    mSwatches->markDirty();
}

void ImageEditorPanel::selectTool(PixelTool tool)
{
    mTool = tool;
    for (int i = 0; i < 6; ++i)
        mTools->setItemChecked(mToolItems[i], static_cast<int>(mTool) == i);
}

bool ImageEditorPanel::open(const ct::String& path)
{
    const ct::String absolute = EditorFileSystem::resolve(mRoot, path);
    Pixmap image;
    if (!image.Load(absolute.c_str()))
    {
        log_error("Could not read %s", path.c_str());
        return false;
    }
    Texture* texture = mAssets.GetTexture(path.c_str());
    if (!texture)
        texture = mAssets.LoadTexture(path.c_str(), absolute.c_str());
    if (!texture || texture->Width() != image.Width() || texture->Height() != image.Height())
        return false;
    mImage.Create(image.Width(), image.Height());
    std::memcpy(mImage.Pixels(), image.Pixels(), static_cast<std::size_t>(image.Width()) * image.Height() * 4);
    mTexture = texture;
    mPath = path;
    mDirty = false;
    mUndo.clear();
    mRedo.clear();
    rt::TextureHandle handle;
    handle.value = texture->Id();
    mCanvas->setTexture(handle, image.Width(), image.Height());
    syncTitle();
    return true;
}

void ImageEditorPanel::beginEdit()
{
    ct::Vector<unsigned char> pixels;
    pixels.resize(static_cast<std::size_t>(mImage.Width()) * mImage.Height() * 4);
    std::memcpy(pixels.data(), mImage.Pixels(), pixels.size());
    mUndo.push_back(ct::detail::move(pixels));
    if (mUndo.size() > kUndoSteps)
        mUndo.erase(mUndo.begin());
    mRedo.clear();
    if (mTool != PixelTool::Eraser && mTool != PixelTool::Picker)
        mSwatches->remember(mColour);
}

void ImageEditorPanel::pixelsChanged()
{
    if (!mTexture)
        return;
    // The texture the scene draws: sprites using it change as they are painted.
    mTexture->Update(mImage.Pixels());
    if (!mDirty)
    {
        mDirty = true;
        syncTitle();
    }
    mCanvas->markDirty();
}

void ImageEditorPanel::undo()
{
    if (mUndo.empty())
        return;
    ct::Vector<unsigned char> now;
    now.resize(mUndo.back().size());
    std::memcpy(now.data(), mImage.Pixels(), now.size());
    mRedo.push_back(ct::detail::move(now));
    std::memcpy(mImage.Pixels(), mUndo.back().data(), mUndo.back().size());
    mUndo.pop_back();
    pixelsChanged();
}

void ImageEditorPanel::redo()
{
    if (mRedo.empty())
        return;
    ct::Vector<unsigned char> now;
    now.resize(mRedo.back().size());
    std::memcpy(now.data(), mImage.Pixels(), now.size());
    mUndo.push_back(ct::detail::move(now));
    std::memcpy(mImage.Pixels(), mRedo.back().data(), mRedo.back().size());
    mRedo.pop_back();
    pixelsChanged();
}

bool ImageEditorPanel::save()
{
    if (!mTexture)
        return false;
    const ct::String absolute = EditorFileSystem::resolve(mRoot, mPath);
    if (!mImage.Save(absolute.c_str()))
    {
        log_error("Could not save %s", mPath.c_str());
        return false;
    }
    mDirty = false;
    syncTitle();
    log_info("Saved %s", mPath.c_str());
    return true;
}

void ImageEditorPanel::revert()
{
    if (!mTexture)
        return;
    Pixmap image;
    if (!image.Load(EditorFileSystem::resolve(mRoot, mPath).c_str()) || image.Width() != mImage.Width() ||
        image.Height() != mImage.Height())
        return;
    beginEdit();   // the revert itself can be undone
    std::memcpy(mImage.Pixels(), image.Pixels(), static_cast<std::size_t>(image.Width()) * image.Height() * 4);
    pixelsChanged();
    mDirty = false;
    syncTitle();
}

void ImageEditorPanel::normalMap()
{
    if (!mTexture)
        return;
    Pixmap* normals = mImage.GenerateNormalMap(2.0f);
    if (!normals)
        return;
    const ct::String path = EditorFileSystem::withoutExtension(mPath) + "_normal.png";
    if (normals->Save(EditorFileSystem::resolve(mRoot, path).c_str()))
        log_info("Made %s", path.c_str());
    else
        log_error("Could not save %s", path.c_str());
    delete normals;
}

void ImageEditorPanel::assetMoved(const AssetMove& move)
{
    ct::String moved;
    if (!move.apply(mRoot, mPath, moved))
        return;
    mPath = moved;
    syncTitle();
}

void ImageEditorPanel::syncTitle()
{
    if (!mTexture)
        return;
    mTitle->setText(mDirty ? mPath + " *" : mPath);
}

} // namespace k2d::editor
