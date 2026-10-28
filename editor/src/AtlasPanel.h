#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{
class Assets;
}

namespace ig::retained
{
class CheckBox;
class ComboBox;
class Label;
class SpinSlider;
class WidgetApp;
}

namespace k2d::editor
{

class DropList;

// Images of the project packed into one texture whose regions are named after
// them, written as the Sprite Editor keeps a sheet (image.png and
// image.sprite.json), so the regions go to the scene and the Animator from
// there. The list and the settings are kept in image.atlas.json to build again.
class AtlasPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(AtlasPanel, ig::retained::BoxLayout);

    explicit AtlasPanel(Assets& assets);

    void setRoot(const ct::String& projectRoot);
    // An atlas made before, from its .atlas.json.
    bool open(const ct::String& recipe);

    // Project paths: images, folders of images, and the image to build.
    void clear();
    void addImage(const ct::String& path);
    void addFolder(const ct::String& folder);
    void setLayout(int maxSize, int padding, bool trim);
    bool build(const ct::String& imagePath);
    std::size_t imageCount() const { return mEntries.size(); }

    // The atlas image just written (a project path), to be opened.
    ig::retained::Signal<const ct::String&> built;

private:
    struct Entry
    {
        ct::String path;   // project path
        ct::String name;
    };

    void syncList();
    void showBuildDialog();
    ct::String absolute(const ct::String& path) const;

    Assets& mAssets;
    ct::String mRoot;
    ct::String mOutput;   // the image last built, a project path
    ct::Vector<Entry> mEntries;
    int mMaxSize = 2048;
    int mPadding = 2;
    bool mTrim = false;

    DropList* mList = nullptr;
    ig::retained::ComboBox* mSize = nullptr;
    ig::retained::SpinSlider* mPaddingField = nullptr;
    ig::retained::CheckBox* mTrimBox = nullptr;
    ig::retained::Label* mInfo = nullptr;
};

} // namespace k2d::editor
