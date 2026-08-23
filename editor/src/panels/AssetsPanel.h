#pragma once

#include "core/EditorFileSystem.h"
#include "core/EditorPanel.h"

#include <ct/hashmap.hpp>
#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{
class Texture;
}

namespace k2d::editor
{

constexpr const char *kPrefabDragDropPayload = "K2D_PREFAB_FILE";
constexpr const char *kTextureDragDropPayload = "K2D_TEXTURE_FILE";
constexpr const char *kScriptDragDropPayload = "K2D_SCRIPT_FILE";

class AssetsPanel final : public EditorPanel
{
public:
    explicit AssetsPanel(EditorApplication &application);
    ~AssetsPanel();

private:
    enum class ViewMode
    {
        Grid,
        List
    };

    void drawContents() override;
    void drawToolbar();
    void drawBreadcrumb();
    void drawSidebar();
    void drawBookmark(const char *label, const ct::String &root);
    void drawDirectoryTree(const ct::String &directory);
    void drawGrid();
    void drawList();
    void handleEntryInteraction(const EditorFileEntry &entry, bool clicked, bool doubleClicked);
    void drawEntryContextMenu(const EditorFileEntry &entry);
    void generateBumpMap(const EditorFileEntry &entry);

    void navigateTo(const ct::String &directory);
    void refreshEntries();
    Texture *thumbnailFor(const EditorFileEntry &entry);

    ct::String mRoot;
    ct::Vector<EditorFileEntry> mEntries;
    ct::String mCachedDirectory;
    bool mEntriesDirty = true;
    bool mEntriesValid = true;

    ct::Vector<ct::String> mHistory;
    size_t mHistoryPosition = 0;

    ViewMode mViewMode = ViewMode::Grid;
    float mThumbnailSize = 96.0f;
    float mSidebarWidth = 180.0f;

    ct::HashMap<ct::String, Texture *> mThumbnailCache;
};

}
