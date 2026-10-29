#include "SpriteEditorPanel.h"

#include "Icons.h"
#include "PropertyFields.h"
#include "SpriteCanvas.h"
#include "core/EditorFileSystem.h"

#include "BasicWidgets.hpp"
#include "ScrollWidgets.hpp"
#include "SpinSlider.hpp"
#include "TextInputWidgets.hpp"
#include "ToolBar.hpp"
#include "WidgetApp.hpp"

#include <k2d/Assets.h>
#include <k2d/Pixmap.h>
#include <k2d/Texture.h>

#include <cmath>
#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

const char* const kSpriteRegionPayload = "k2d-sprite-region";

// The selected region at a glance, and the handle to drag it out by.
class RegionPreview : public rt::Widget
{
public:
    IG_WIDGET_CLASS(RegionPreview, rt::Widget);

    explicit RegionPreview(SpriteEditorPanel& panel) : mPanel(panel) {}

    void setTexture(rt::TextureHandle texture, int width, int height)
    {
        mTexture = texture;
        mWidth = width;
        mHeight = height;
        markDirty();
    }

    Vec2f sizeHint() const override { return {200.0f, 120.0f}; }

    void paint(rt::PaintContext& ctx) override
    {
        if (!visible_)
            return;
        const rt::Rect abs = absoluteRect();
        ctx.pushClip(abs);
        ctx.fill.SetColor(rt::Color(28, 30, 34, 255));
        ctx.fillRect(abs.x, abs.y, abs.w, abs.h);
        SpriteRegionDrag region;
        const auto& theme = rt::Theme::instance();
        ctx.font.SetFontSize(theme.fontSize);
        ctx.font.SetBatch(&ctx.text);
        if (mTexture && mWidth > 0 && mPanel.currentRegion(region))
        {
            const Math::Vec4& r = region.rect;
            const float room = abs.h - 22.0f;
            const float fit = std::fmin((abs.w - 8.0f) / r.z, room / r.w);
            const float scale = fit >= 1.0f ? std::floor(fit) : fit;
            const float w = r.z * scale, h = r.w * scale;
            const rt::Rect dst{abs.x + (abs.w - w) * 0.5f, abs.y + 4.0f + (room - h) * 0.5f, w, h};
            const rt::Rect uv{r.x / mWidth, r.y / mHeight, r.z / mWidth, r.w / mHeight};
            ctx.drawImage(mTexture, dst, uv);
            ctx.font.SetColor(theme.textDisabled);
            ctx.font.Print("Drag into the scene", abs.x + 6.0f, abs.y + abs.h - 6.0f);
        }
        else
        {
            ctx.font.SetColor(theme.textDisabled);
            ctx.font.Print("No region selected", abs.x + 6.0f, abs.y + abs.h * 0.5f);
        }
        ctx.popClip();
    }

    bool isDragSource() const override
    {
        SpriteRegionDrag region;
        return mPanel.currentRegion(region);
    }

    rt::DragPayload onDragBegin() override
    {
        rt::DragPayload payload;
        SpriteRegionDrag region;
        if (mPanel.currentRegion(region))
        {
            payload.type = kSpriteRegionPayload;
            payload.data = region;
        }
        return payload;
    }

private:
    SpriteEditorPanel& mPanel;
    rt::TextureHandle mTexture;
    int mWidth = 0;
    int mHeight = 0;
};

namespace
{

// A column of fixed width beside the canvas.
class SideColumn : public rt::BoxLayout
{
public:
    IG_WIDGET_CLASS(SideColumn, rt::BoxLayout);

    SideColumn() : rt::BoxLayout(rt::LayoutDir::Vertical) {}

    Vec2f sizeHint() const override { return {220.0f, rt::BoxLayout::sizeHint().y}; }
};

// Whole-number fields from the kit: a slider for a short range, the number
// alone otherwise.
rt::SpinSlider* integerField(rt::Widget* parent, const char* label, double value, double minimum, double maximum)
{
    NumberSpec spec;
    spec.integer = true;
    spec.step = 1.0;
    if (maximum - minimum <= 1000.0)
    {
        spec.minimum = minimum;
        spec.maximum = maximum;
    }
    spec.width = 96.0f;
    return addNumberField(parent, label, value, spec, nullptr);
}

} // namespace

SpriteEditorPanel::SpriteEditorPanel(Assets& assets) : rt::BoxLayout(rt::LayoutDir::Vertical), mAssets(assets)
{
    setPadding(0.0f);
    setSpacing(0.0f);

    auto* bar = createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    bar->setPadding(2.0f, 4.0f);
    bar->setSpacing(6.0f);
    auto* tools = bar->createChild<rt::ToolBar>();
    tools->setDraggable(false);
    const int saveItem = tools->addButton(Icons::get("Save"), "Save the regions");
    const int fitItem = tools->addButton(Icons::get("ZoomFit"), "Fit the image in view");
    const int pixelsItem = tools->addButton(rt::IconId::Edit, "Edit pixels: open the image in the Image Editor");
    tools->buttonClicked.connect([this, saveItem, fitItem, pixelsItem](int item)
    {
        if (item == saveItem)
            save();
        else if (item == fitItem)
            mCanvas->fit();
        else if (item == pixelsItem && mTexture)
            editPixels.emit(mPath);
    });
    mTitle = bar->createChild<rt::Label>("No image");
    mTitle->setStretch(1.0f);
    mTitle->setLayoutAlign(rt::Align::Center);

    mAlpha = integerField(bar, "alpha", 1, 1, 255);
    mMinPixels = integerField(bar, "min px", 4, 1, 1000000);
    iconButton(bar, "Search", "Detect: replace the regions with the islands of opaque pixels",
               [this]() { detect(); });
    mColumns = integerField(bar, "cols", 1, 1, 256);
    mRows = integerField(bar, "rows", 1, 1, 256);
    iconButton(bar, "Grid", "Split: replace the regions with a grid of cols x rows cells", [this]() { split(); });

    auto* body = createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    body->setStretch(1.0f);
    body->setPadding(0.0f);
    body->setSpacing(4.0f);

    auto* side = body->createChild<SideColumn>();
    side->setPadding(4.0f);
    side->setSpacing(4.0f);
    mList = side->createChild<rt::ListBox>();
    mList->setStretch(1.0f);
    mList->selectionChanged.connect([this](int index)
    {
        if (!mSyncing)
            select(index);
    });

    mId = side->createChild<rt::TextInput>("");
    mId->setPlaceholder("Region name");
    mId->submitted.connect([this](const rt::String& name)
    {
        const SpriteRegion* region = mCanvas->selected() >= 0 ? mAtlas.regionAt(mCanvas->selected()) : nullptr;
        if (!region || name.empty() || name == region->id)
            return;
        const ct::String old = region->id;
        const Math::Vec4 rect = region->rect;
        if (mAtlas.set(old.c_str(), name.c_str(), rect))
            markEdited();
        else
            mId->setText(old);   // the name is taken
    });

    const char* names[4] = {"x", "y", "w", "h"};
    for (int row = 0; row < 2; ++row)
    {
        auto* line = side->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
        line->setPadding(0.0f);
        line->setSpacing(4.0f);
        for (int column = 0; column < 2; ++column)
        {
            const int i = row * 2 + column;
            mRect[i] = integerField(line, names[i], 0, 0, 16384);
            mRect[i]->setStretch(1.0f);
            mRect[i]->valueChanged.connect([this, i](double value)
            {
                const int selected = mCanvas->selected();
                const SpriteRegion* region = selected >= 0 ? mAtlas.regionAt(selected) : nullptr;
                if (mSyncing || !region || !mTexture)
                    return;
                Math::Vec4 rect = region->rect;
                float* parts[4] = {&rect.x, &rect.y, &rect.z, &rect.w};
                *parts[i] = static_cast<float>(value);
                rect.z = std::fmax(1.0f, std::fmin(rect.z, static_cast<float>(mTexture->Width())));
                rect.w = std::fmax(1.0f, std::fmin(rect.w, static_cast<float>(mTexture->Height())));
                rect.x = std::fmax(0.0f, std::fmin(rect.x, mTexture->Width() - rect.z));
                rect.y = std::fmax(0.0f, std::fmin(rect.y, mTexture->Height() - rect.w));
                const ct::String id = region->id;
                mAtlas.set(id.c_str(), id.c_str(), rect);
                mCanvas->setSelected(selected);
                markEdited();
            });
        }
    }

    mPreview = side->createChild<RegionPreview>(*this);
    iconButton(side, "Add", "Add to scene: a sprite object showing this region, in the middle of the scene view",
               [this]()
               {
                   SpriteRegionDrag region;
                   if (currentRegion(region))
                       addToScene.emit(region);
               });

    mCanvas = body->createChild<SpriteCanvas>();
    mCanvas->setStretch(1.0f);
    mCanvas->setAtlas(&mAtlas);
    mCanvas->selectionChanged.connect([this](int) { syncFields(); });
    mCanvas->edited.connect([this]() { markEdited(); });
    mCanvas->imageDropped.connect([this](const ct::String& path) { openRequested.emit(path); });
}

bool SpriteEditorPanel::currentRegion(SpriteRegionDrag& out) const
{
    const int selected = mCanvas ? mCanvas->selected() : -1;
    const SpriteRegion* region = selected >= 0 ? mAtlas.regionAt(static_cast<std::size_t>(selected)) : nullptr;
    if (!region || !mTexture)
        return false;
    out.texture = mPath;
    out.id = region->id;
    out.rect = region->rect;
    return true;
}

bool SpriteEditorPanel::allRegions(ct::Vector<SpriteRegionDrag>& out) const
{
    out.clear();
    if (!mTexture)
        return false;
    for (std::size_t i = 0; i < mAtlas.regionCount(); ++i)
    {
        SpriteRegionDrag region;
        region.texture = mPath;
        region.id = mAtlas.regionAt(i)->id;
        region.rect = mAtlas.regionAt(i)->rect;
        out.push_back(region);
    }
    return true;
}

bool SpriteEditorPanel::open(const ct::String& path)
{
    const ct::String absolute = EditorFileSystem::resolve(mRoot, path);
    Texture* texture = mAssets.GetTexture(path.c_str());
    if (!texture)
        texture = mAssets.LoadTexture(path.c_str(), absolute.c_str());
    if (!texture)
        return false;

    mTexture = texture;
    mPath = path;
    mMetadata = EditorFileSystem::withoutExtension(absolute) + ".sprite.json";
    mAtlas.clear();
    mAtlas.setTexturePath(path.c_str());
    if (EditorFileSystem::exists(mMetadata))
    {
        mAtlas.load(mMetadata.c_str());
        mAtlas.setTexturePath(path.c_str());
    }
    mDirty = false;

    rt::TextureHandle handle;
    handle.value = texture->Id();
    mCanvas->setBaseName(EditorFileSystem::withoutExtension(EditorFileSystem::fileName(path)));
    mCanvas->setTexture(handle, texture->Width(), texture->Height());
    mPreview->setTexture(handle, texture->Width(), texture->Height());
    syncList();
    syncFields();
    updateTitle();
    return true;
}

bool SpriteEditorPanel::save()
{
    if (!mTexture || mMetadata.empty())
        return false;
    if (!mAtlas.save(mMetadata.c_str()))
        return false;
    mDirty = false;
    updateTitle();
    return true;
}

void SpriteEditorPanel::markEdited()
{
    mDirty = true;
    syncList();
    syncFields();
    updateTitle();
}

void SpriteEditorPanel::assetMoved(const AssetMove& move)
{
    ct::String moved;
    if (move.apply(mRoot, mMetadata, moved))
        mMetadata = moved;
    if (!move.apply(mRoot, mPath, moved))
        return;
    mPath = moved;
    mAtlas.setTexturePath(mPath.c_str());
    updateTitle();
}

void SpriteEditorPanel::updateTitle()
{
    if (!mTexture)
    {
        mTitle->setText("No image");
        return;
    }
    char text[512];
    std::snprintf(text, sizeof text, "%s%s   %d regions", mPath.c_str(), mDirty ? " *" : "",
                  static_cast<int>(mAtlas.regionCount()));
    mTitle->setText(text);
}

void SpriteEditorPanel::syncList()
{
    mSyncing = true;
    mList->clearItems();
    for (std::size_t i = 0; i < mAtlas.regionCount(); ++i)
        mList->addItem(mAtlas.regionAt(i)->id);
    mList->setSelectedIndex(mCanvas->selected());
    mSyncing = false;
}

void SpriteEditorPanel::syncFields()
{
    mSyncing = true;
    const int selected = mCanvas->selected();
    const SpriteRegion* region = selected >= 0 ? mAtlas.regionAt(static_cast<std::size_t>(selected)) : nullptr;
    mList->setSelectedIndex(selected);
    mId->setText(region ? region->id : ct::String());
    const float values[4] = {region ? region->rect.x : 0.0f, region ? region->rect.y : 0.0f,
                             region ? region->rect.z : 0.0f, region ? region->rect.w : 0.0f};
    for (int i = 0; i < 4; ++i)
    {
        mRect[i]->setValueNoSignal(values[i]);
        mRect[i]->setEnabled(region != nullptr);
    }
    mPreview->markDirty();
    mSyncing = false;
}

void SpriteEditorPanel::select(int region)
{
    mCanvas->setSelected(region);
    syncFields();
}

void SpriteEditorPanel::detect()
{
    if (!mTexture)
        return;
    Pixmap image;
    if (!image.Load(EditorFileSystem::resolve(mRoot, mPath).c_str()))
        return;
    mAtlas.detectTransparentRegions(image, static_cast<unsigned char>(mAlpha->value()),
                                    static_cast<int>(mMinPixels->value()));
    // Named after the image, in the order they were found.
    const ct::String base = EditorFileSystem::withoutExtension(EditorFileSystem::fileName(mPath));
    for (std::size_t i = 0; i < mAtlas.regionCount(); ++i)
    {
        const SpriteRegion* region = mAtlas.regionAt(i);
        const ct::String old = region->id;
        const Math::Vec4 rect = region->rect;
        char id[96];
        std::snprintf(id, sizeof id, "%s_%03d", base.c_str(), static_cast<int>(i) + 1);
        mAtlas.set(old.c_str(), id, rect);
    }
    mCanvas->setSelected(-1);
    markEdited();
}

void SpriteEditorPanel::split()
{
    if (!mTexture)
        return;
    const int columns = static_cast<int>(mColumns->value());
    const int rows = static_cast<int>(mRows->value());
    const float cellW = static_cast<float>(mTexture->Width()) / columns;
    const float cellH = static_cast<float>(mTexture->Height()) / rows;
    const ct::String base = EditorFileSystem::withoutExtension(EditorFileSystem::fileName(mPath));
    mAtlas.clear();
    mAtlas.setTexturePath(mPath.c_str());
    int number = 0;
    for (int y = 0; y < rows; ++y)
        for (int x = 0; x < columns; ++x)
        {
            char id[96];
            std::snprintf(id, sizeof id, "%s_%03d", base.c_str(), ++number);
            mAtlas.add(id, Math::Vec4(std::floor(x * cellW), std::floor(y * cellH), std::floor(cellW),
                                      std::floor(cellH)));
        }
    mCanvas->setSelected(mAtlas.regionCount() > 0 ? 0 : -1);
    markEdited();
}

} // namespace k2d::editor
