#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"
#include "ThumbnailGrid.hpp"
#include "ToolBar.hpp"

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{
class Assets;
}

namespace k2d::editor
{

// The payload type of an asset dragged out of the panel; its data is the
// ct::String path relative to the project root.
extern const char* const kAssetPayload;

// The project's files as thumbnails: folders open with a double click, images
// show themselves, everything else its icon. Items drag out as kAssetPayload.
class AssetsPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(AssetsPanel, ig::retained::BoxLayout);

    explicit AssetsPanel(Assets& assets);

    // The project root; paths in payloads and signals are relative to it.
    void setRoot(const ct::String& root);
    void open(const ct::String& directory);
    void refresh();

    // A file was opened with a double click, by its path relative to the root.
    ig::retained::Signal<const ct::String&> fileOpened;

private:
    struct Entry
    {
        ct::String name;
        ct::String path;
        bool directory = false;
    };

    Assets& mAssets;
    ig::retained::ToolBar* mTools = nullptr;
    ig::retained::Label* mPath = nullptr;
    ig::retained::ThumbnailGrid* mGrid = nullptr;
    ct::String mRoot;
    ct::String mDirectory;
    ct::Vector<Entry> mEntries;
};

} // namespace k2d::editor
