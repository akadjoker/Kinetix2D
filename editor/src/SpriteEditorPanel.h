#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"

#include <k2d/SpriteAtlas.h>

#include <ct/string.hpp>
#include <mathc.h>

namespace k2d
{
class Assets;
class Texture;
}

namespace ig::retained
{
class Label;
class ListBox;
class SpinSlider;
class TextInput;
class ToolBar;
}

namespace k2d::editor
{

class SpriteCanvas;
class RegionPreview;

// The payload type of a sprite region dragged out of the Sprite Editor; its
// data is a SpriteRegionDrag.
extern const char* const kSpriteRegionPayload;

struct SpriteRegionDrag
{
    ct::String texture;   // project path, the name the Assets know it by
    ct::String id;
    Math::Vec4 rect;
};

// Named regions of one texture, kept in a .sprite.json beside the image: drawn
// and edited on the canvas, made by alpha detection or a grid split, and
// dragged out as sprites.
class SpriteEditorPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(SpriteEditorPanel, ig::retained::BoxLayout);

    explicit SpriteEditorPanel(Assets& assets);

    void setRoot(const ct::String& root) { mRoot = root; }
    // Opens a texture by its project path; unsaved regions of the one open are
    // dropped, so the editor asks first.
    bool open(const ct::String& path);
    bool save();
    bool dirty() const { return mDirty; }
    const ct::String& path() const { return mPath; }
    // An image dropped on the canvas.
    ig::retained::Signal<const ct::String&> openRequested;

    // "Add to scene" on the selected region.
    ig::retained::Signal<const SpriteRegionDrag&> addToScene;
    // "Edit pixels": the open image, by its project path.
    ig::retained::Signal<const ct::String&> editPixels;

    bool currentRegion(SpriteRegionDrag& out) const;
    // Every region of the open image, in order; false when none is open.
    bool allRegions(ct::Vector<SpriteRegionDrag>& out) const;

private:
    void markEdited();
    void syncList();
    void syncFields();
    void select(int region);
    void detect();
    void split();
    void updateTitle();

    Assets& mAssets;
    ct::String mRoot;
    ct::String mPath;       // the texture's project path
    ct::String mMetadata;   // absolute path of the .sprite.json
    Texture* mTexture = nullptr;
    SpriteAtlas mAtlas;
    bool mDirty = false;
    bool mSyncing = false;

    ig::retained::Label* mTitle = nullptr;
    ig::retained::SpinSlider* mAlpha = nullptr;
    ig::retained::SpinSlider* mMinPixels = nullptr;
    ig::retained::SpinSlider* mColumns = nullptr;
    ig::retained::SpinSlider* mRows = nullptr;
    ig::retained::ListBox* mList = nullptr;
    ig::retained::TextInput* mId = nullptr;
    ig::retained::SpinSlider* mRect[4] = {nullptr, nullptr, nullptr, nullptr};
    RegionPreview* mPreview = nullptr;
    SpriteCanvas* mCanvas = nullptr;
};

} // namespace k2d::editor
