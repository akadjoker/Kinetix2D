#pragma once

#include "core/EditorFileSystem.h"
#include "core/EditorPanel.h"

#include <ct/hashmap.hpp>
#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <cstring>

namespace k2d
{
class Texture;
}

namespace k2d::editor
{

constexpr const char *kPrefabDragDropPayload = "K2D_PREFAB_FILE";
constexpr const char *kTextureDragDropPayload = "K2D_TEXTURE_FILE";
constexpr const char *kSpriteRegionDragDropPayload = "K2D_SPRITE_REGION";
constexpr const char *kScriptDragDropPayload = "K2D_SCRIPT_FILE";

// Editor-only transfer data.  A region is copied into an Animation2D frame;
// the scene never depends on the Sprite Editor panel being open afterwards.
struct SpriteRegionDragDropData
{
    Texture *texture = nullptr;
    char texturePath[512] = {};
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

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
    void drawCreationMenu(const ct::String &directory);
    void drawNewScriptPopup();
    void drawNewFolderPopup();
    void requestNewScript(const ct::String &directory);
    void requestNewFolder(const ct::String &directory);

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
    ct::String mCreateDirectory;
    char mNewScriptName[64] = "new_script";
    int mNewScriptTemplate = 0;
    char mNewFolderName[64] = "new_folder";
};

}
