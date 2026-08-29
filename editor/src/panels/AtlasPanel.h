#pragma once

#include "core/EditorPanel.h"

#include <ImGuiFileDialog.h>

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d::editor
{

struct AtlasEntry
{
    ct::String sourcePath;
    ct::String relativePath;
    ct::String name;
    int sourceWidth = 0;
    int sourceHeight = 0;
    int trimX = 0;
    int trimY = 0;
    int trimWidth = 0;
    int trimHeight = 0;
    bool missing = false;
    bool empty = false;
};

struct AtlasPageResult
{
    struct Region
    {
        int entryIndex = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    int width = 0;
    int height = 0;
    ct::Vector<Region> regions;
};

class AtlasPanel final : public EditorPanel
{
  public:
    explicit AtlasPanel(EditorApplication& application);

  private:
    enum class PendingAction
    {
        None,
        OpenAtlas,
        SaveAtlas
    };

    void drawContents() override;
    void drawToolbar();
    void drawSettings();
    void drawEntryList();
    void drawPreview();
    void drawFileDialogPopup();

    void addImage(const ct::String& absolutePath);
    void removeEntry(std::size_t index);
    bool loadPixelInfo(const ct::String& absolutePath, int& outWidth, int& outHeight, int& outTrimX, int& outTrimY,
                       int& outTrimWidth, int& outTrimHeight, bool& outEmpty) const;
    void recomputeEntry(AtlasEntry& entry) const;
    void recomputeAll();
    bool pack();
    void save(const ct::String& destinationPath);
    bool loadAtlas(const ct::String& jsonPath);

    ct::Vector<AtlasEntry> mEntries;
    ct::Vector<AtlasPageResult> mPages;

    int mMaxWidth = 2048;
    int mMaxHeight = 2048;
    int mPadding = 1;
    bool mTrim = true;
    int mAlphaThreshold = 0;

    bool mHasPacked = false;
    int mSkippedCount = 0;

    ImGuiFileDialog mFileDialog;
    PendingAction mPendingAction = PendingAction::None;
};

} // namespace k2d::editor
