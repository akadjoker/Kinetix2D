#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"
#include "ThumbnailGrid.hpp"
#include "ToolBar.hpp"

#include <ct/function.hpp>
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
// A folder dragged within the panel, to move it; nothing else takes it.
extern const char* const kAssetFolderPayload;

// What a project path holds, by its extension.
bool isImagePath(const ct::String& path);
bool isScriptPath(const ct::String& path);
bool isPrefabPath(const ct::String& path);
// The project path an asset payload carries, or nullptr for another payload.
const ct::String* assetPath(const ig::retained::DragPayload& payload);

// The project's files as thumbnails: folders open with a double click, images
// show themselves, everything else its icon. Items drag out as kAssetPayload,
// and onto a folder (or "..") to move there. The right button, F2 and Delete
// make folders and scripts, rename and delete; the editor does the moves and
// deletes, which the files naming them follow.
class AssetsPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(AssetsPanel, ig::retained::BoxLayout);

    explicit AssetsPanel(Assets& assets);

    // The project root; paths in payloads and signals are relative to it.
    void setRoot(const ct::String& root);
    void open(const ct::String& directory);
    void refresh();

    void onKeyPress(ig::retained::KeyEvent& e) override;

    // A file was opened with a double click, by its path relative to the root.
    ig::retained::Signal<const ct::String&> fileOpened;
    // Paths relative to the root: a file or folder to move or rename (from, to),
    // one to delete.
    ig::retained::Signal<const ct::String&, const ct::String&> moveRequested;
    ig::retained::Signal<const ct::String&> deleteRequested;
    // A new script, by its absolute path.
    ig::retained::Signal<const ct::String&> scriptRequested;

private:
    struct Entry
    {
        ct::String name;
        ct::String path;
        bool directory = false;
        bool up = false;   // "..": the folder above
    };

    const Entry* entryAt(int index) const;
    ct::String relative(const ct::String& path) const;
    void showMenu(int index, float x, float y);
    void askName(const char* title, const ct::String& name, ct::Function<void(const ct::String&)> done);
    void rename(const Entry& entry);
    void moveInto(const ct::String& from, const ct::String& folder);

    Assets& mAssets;
    ig::retained::ToolBar* mTools = nullptr;
    ig::retained::Label* mPath = nullptr;
    ig::retained::ThumbnailGrid* mGrid = nullptr;
    ct::String mRoot;
    ct::String mDirectory;
    ct::Vector<Entry> mEntries;
};

} // namespace k2d::editor
