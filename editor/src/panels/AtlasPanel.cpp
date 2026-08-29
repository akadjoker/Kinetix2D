#include "AtlasPanel.h"

#include "AssetsPanel.h"
#include "core/EditorApplication.h"

#define CT_RECTPACKER_IMPLEMENTATION
#include <ct/rectpacker.hpp>

#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>
#include <k2d/Pixmap.h>
#include <k2d/Serializer.h>

#include <IconsMaterialDesignIcons.h>

#include <cstdio>
#include <filesystem>

namespace k2d::editor
{
namespace
{
ct::String startDirectory(EditorApplication& app)
{
    return app.project().valid() ? app.project().assetsDirectory() : EditorFileSystem::currentDirectory();
}
} // namespace

AtlasPanel::AtlasPanel(EditorApplication& application) : EditorPanel("Atlas", application, false)
{
}

bool AtlasPanel::loadPixelInfo(const ct::String& absolutePath, int& outWidth, int& outHeight, int& outTrimX,
                               int& outTrimY, int& outTrimWidth, int& outTrimHeight, bool& outEmpty) const
{
    Pixmap pixmap;
    if (!pixmap.Load(absolutePath.c_str()))
        return false;

    outWidth = pixmap.Width();
    outHeight = pixmap.Height();

    if (!mTrim)
    {
        outTrimX = 0;
        outTrimY = 0;
        outTrimWidth = outWidth;
        outTrimHeight = outHeight;
        outEmpty = false;
        return true;
    }

    if (!pixmap.ComputeTrimBounds(static_cast<unsigned char>(mAlphaThreshold), outTrimX, outTrimY, outTrimWidth,
                                  outTrimHeight))
    {
        outTrimX = 0;
        outTrimY = 0;
        outTrimWidth = 0;
        outTrimHeight = 0;
        outEmpty = true;
        return true;
    }
    outEmpty = false;
    return true;
}

void AtlasPanel::recomputeEntry(AtlasEntry& entry) const
{
    int width = 0, height = 0, trimX = 0, trimY = 0, trimWidth = 0, trimHeight = 0;
    bool empty = false;
    if (!loadPixelInfo(entry.sourcePath, width, height, trimX, trimY, trimWidth, trimHeight, empty))
    {
        entry.missing = true;
        return;
    }
    entry.missing = false;
    entry.sourceWidth = width;
    entry.sourceHeight = height;
    entry.trimX = trimX;
    entry.trimY = trimY;
    entry.trimWidth = trimWidth;
    entry.trimHeight = trimHeight;
    entry.empty = empty;
}

void AtlasPanel::recomputeAll()
{
    for (AtlasEntry& entry : mEntries)
        recomputeEntry(entry);
    mHasPacked = false;
}

void AtlasPanel::addImage(const ct::String& absolutePath)
{
    const ct::String relative = EditorFileSystem::relativeTo(app().project().root(), absolutePath);
    for (const AtlasEntry& existing : mEntries)
    {
        if (existing.relativePath == relative)
        {
            app().toasts().error("Image already in the atlas");
            return;
        }
    }

    AtlasEntry entry;
    entry.sourcePath = absolutePath;
    entry.relativePath = relative;
    entry.name = EditorFileSystem::withoutExtension(EditorFileSystem::fileName(absolutePath));

    if (!loadPixelInfo(entry.sourcePath, entry.sourceWidth, entry.sourceHeight, entry.trimX, entry.trimY,
                       entry.trimWidth, entry.trimHeight, entry.empty))
    {
        app().toasts().error("Could not read image");
        return;
    }
    entry.missing = false;
    mEntries.push_back(entry);
    mHasPacked = false;
}

void AtlasPanel::removeEntry(std::size_t index)
{
    if (index >= mEntries.size())
        return;
    mEntries.erase(mEntries.begin() + static_cast<std::ptrdiff_t>(index));
    mHasPacked = false;
}

bool AtlasPanel::pack()
{
    recomputeAll();

    ct::Vector<ct::RectPacker::Input> inputs;
    for (std::size_t i = 0; i < mEntries.size(); ++i)
    {
        const AtlasEntry& entry = mEntries[i];
        if (entry.missing || entry.empty)
            continue;
        ct::RectPacker::Input input;
        input.id = static_cast<int32_t>(i);
        input.width = entry.trimWidth;
        input.height = entry.trimHeight;
        inputs.push_back(input);
    }

    const ct::Vector<ct::RectPacker::Page> packed = ct::RectPacker::pack_pages(inputs, mMaxWidth, mMaxHeight, mPadding);

    ct::Vector<bool> placed(mEntries.size(), false);
    mPages.clear();
    for (const ct::RectPacker::Page& page : packed)
    {
        AtlasPageResult result;
        result.width = page.atlas_width;
        result.height = page.atlas_height;
        for (const ct::RectPacker::PlacedRect& rect : page.placed)
        {
            AtlasPageResult::Region region;
            region.entryIndex = rect.id;
            region.x = rect.x;
            region.y = rect.y;
            region.width = rect.width;
            region.height = rect.height;
            result.regions.push_back(region);
            if (rect.id >= 0 && static_cast<std::size_t>(rect.id) < placed.size())
                placed[static_cast<std::size_t>(rect.id)] = true;
        }
        mPages.push_back(result);
    }

    mSkippedCount = 0;
    for (std::size_t i = 0; i < mEntries.size(); ++i)
        if (!mEntries[i].missing && !mEntries[i].empty && !placed[i])
            ++mSkippedCount;

    mHasPacked = true;
    return true;
}

void AtlasPanel::save(const ct::String& destinationPath)
{
    pack();

    if (mEntries.empty())
    {
        app().toasts().error("Add at least one image before saving");
        return;
    }

    bool anyMissing = false;
    for (const AtlasEntry& entry : mEntries)
        if (entry.missing)
            anyMissing = true;
    if (anyMissing)
    {
        app().toasts().error("Cannot save: some source images are missing. Remove or restore them first.");
        return;
    }

    const ct::String basePath = EditorFileSystem::withoutExtension(destinationPath);
    const ct::String jsonPath = basePath + ".json";
    const ct::String outputDirectory = EditorFileSystem::parentPath(basePath);
    const ct::String baseName = EditorFileSystem::fileName(basePath);

    ct::Json settings = ct::Json::object();
    settings.set("maxWidth", ct::Json(static_cast<int64_t>(mMaxWidth)));
    settings.set("maxHeight", ct::Json(static_cast<int64_t>(mMaxHeight)));
    settings.set("padding", ct::Json(static_cast<int64_t>(mPadding)));
    settings.set("trim", ct::Json(mTrim));
    settings.set("alphaThreshold", ct::Json(static_cast<int64_t>(mAlphaThreshold)));

    ct::Json pages = ct::Json::array();
    for (std::size_t p = 0; p < mPages.size(); ++p)
    {
        const AtlasPageResult& page = mPages[p];

        Pixmap canvas;
        canvas.Create(page.width, page.height);

        ct::Json regions = ct::Json::array();
        for (const AtlasPageResult::Region& region : page.regions)
        {
            const AtlasEntry& entry = mEntries[static_cast<std::size_t>(region.entryIndex)];

            Pixmap source;
            if (source.Load(entry.sourcePath.c_str()))
            {
                Pixmap trimmed;
                if (source.CopyRect(entry.trimX, entry.trimY, entry.trimWidth, entry.trimHeight, trimmed))
                    canvas.Blit(trimmed, region.x, region.y);
            }

            ct::Json regionJson = ct::Json::object();
            regionJson.set("name", ct::Json(entry.name.c_str()));
            regionJson.set("path", ct::Json(entry.relativePath.c_str()));
            regionJson.set("x", ct::Json(static_cast<int64_t>(region.x)));
            regionJson.set("y", ct::Json(static_cast<int64_t>(region.y)));
            regionJson.set("width", ct::Json(static_cast<int64_t>(region.width)));
            regionJson.set("height", ct::Json(static_cast<int64_t>(region.height)));
            regionJson.set("offsetX", ct::Json(static_cast<int64_t>(entry.trimX)));
            regionJson.set("offsetY", ct::Json(static_cast<int64_t>(entry.trimY)));
            regionJson.set("sourceWidth", ct::Json(static_cast<int64_t>(entry.sourceWidth)));
            regionJson.set("sourceHeight", ct::Json(static_cast<int64_t>(entry.sourceHeight)));
            regions.push_back(regionJson);
        }

        char suffix[32];
        std::snprintf(suffix, sizeof(suffix), "_%d.png", static_cast<int>(p));
        ct::String pageFileName = baseName;
        pageFileName += suffix;
        const ct::String pagePath = EditorFileSystem::join(outputDirectory, pageFileName.c_str());
        canvas.Save(pagePath.c_str());

        ct::Json pageJson = ct::Json::object();
        pageJson.set("file", ct::Json(pageFileName.c_str()));
        pageJson.set("width", ct::Json(static_cast<int64_t>(page.width)));
        pageJson.set("height", ct::Json(static_cast<int64_t>(page.height)));
        pageJson.set("regions", regions);
        pages.push_back(pageJson);
    }

    ct::Json root = ct::Json::object();
    root.set("settings", settings);
    root.set("pages", pages);

    if (!FileSystem::Instance().SaveTextFile(jsonPath.c_str(), root.dump(2)))
    {
        app().toasts().error("Could not write atlas JSON");
        return;
    }
    app().toasts().success("Atlas saved");
}

bool AtlasPanel::loadAtlas(const ct::String& jsonPath)
{
    FileBuffer buffer;
    if (!FileSystem::Instance().LoadFile(jsonPath.c_str(), buffer, true))
    {
        app().toasts().error("Could not read atlas file");
        return false;
    }

    ct::Json::Error err;
    const ct::Json document = ct::Json::parse(buffer.Text(), &err);
    if (err)
    {
        app().toasts().error("Invalid atlas JSON");
        return false;
    }

    const ct::Json* pages = document.find("pages");
    if (!pages || !pages->is_array())
    {
        app().toasts().error("Atlas file has no pages");
        return false;
    }

    if (const ct::Json* settings = document.find("settings"))
    {
        mMaxWidth = static_cast<int>((*settings)["maxWidth"].as_int(2048));
        mMaxHeight = static_cast<int>((*settings)["maxHeight"].as_int(2048));
        mPadding = static_cast<int>((*settings)["padding"].as_int(1));
        mTrim = (*settings)["trim"].as_bool(true);
        mAlphaThreshold = static_cast<int>((*settings)["alphaThreshold"].as_int(0));
    }

    mEntries.clear();
    mPages.clear();
    mHasPacked = false;

    const ct::String root = app().project().root();
    for (std::size_t p = 0; p < pages->size(); ++p)
    {
        const ct::Json& page = (*pages)[p];
        const ct::Json* regions = page.find("regions");
        if (!regions || !regions->is_array())
            continue;
        for (std::size_t r = 0; r < regions->size(); ++r)
        {
            const ct::Json& region = (*regions)[r];
            AtlasEntry entry;
            entry.name = region["name"].as_cstr("");
            entry.relativePath = region["path"].as_cstr("");
            entry.sourcePath = EditorFileSystem::resolve(root, entry.relativePath);
            entry.sourceWidth = static_cast<int>(region["sourceWidth"].as_int(0));
            entry.sourceHeight = static_cast<int>(region["sourceHeight"].as_int(0));
            entry.trimX = static_cast<int>(region["offsetX"].as_int(0));
            entry.trimY = static_cast<int>(region["offsetY"].as_int(0));
            entry.trimWidth = static_cast<int>(region["width"].as_int(0));
            entry.trimHeight = static_cast<int>(region["height"].as_int(0));
            entry.empty = false;
            entry.missing = false;
            recomputeEntry(entry);
            mEntries.push_back(entry);
        }
    }

    app().toasts().success("Atlas loaded");
    return true;
}

void AtlasPanel::drawToolbar()
{
    if (ImGui::Button(ICON_MDI_FOLDER_OPEN "  Open Atlas..."))
    {
        mFileDialog.Open(ImGuiFileDialog::Mode::OpenFile, std::filesystem::path(startDirectory(app()).c_str()),
                         "atlas.json");
        mPendingAction = PendingAction::OpenAtlas;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_PACKAGE_VARIANT "  Pack"))
        pack();
    ImGui::SameLine();

    bool anyMissing = false;
    for (const AtlasEntry& entry : mEntries)
        if (entry.missing)
            anyMissing = true;
    const bool canSave = !mEntries.empty() && !anyMissing;
    if (!canSave)
        ImGui::BeginDisabled();
    if (ImGui::Button(ICON_MDI_CONTENT_SAVE "  Save Atlas..."))
    {
        mFileDialog.Open(ImGuiFileDialog::Mode::SaveFile, std::filesystem::path(startDirectory(app()).c_str()),
                         "atlas.json");
        mPendingAction = PendingAction::SaveAtlas;
    }
    if (!canSave)
    {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(mEntries.empty() ? "Add images first" : "Some source images are missing");
    }
}

void AtlasPanel::drawSettings()
{
    ImGui::DragInt("Max Width", &mMaxWidth, 1.0f, 16, 8192);
    ImGui::DragInt("Max Height", &mMaxHeight, 1.0f, 16, 8192);
    ImGui::DragInt("Padding", &mPadding, 1.0f, 0, 64);
    if (ImGui::Checkbox("Trim Transparent Borders", &mTrim))
        recomputeAll();
    if (mTrim)
    {
        if (ImGui::SliderInt("Alpha Threshold", &mAlphaThreshold, 0, 255))
            recomputeAll();
        ImGui::TextDisabled("A pixel counts as content when its alpha is above this value.");
    }
}

void AtlasPanel::drawEntryList()
{
    ImGui::Text("%d image(s)", static_cast<int>(mEntries.size()));
    if (!ImGui::BeginTable("AtlasEntries", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders))
        return;
    ImGui::TableSetupColumn("Name");
    ImGui::TableSetupColumn("Source Size");
    ImGui::TableSetupColumn("Trimmed Size");
    ImGui::TableSetupColumn("");
    ImGui::TableHeadersRow();

    for (std::size_t i = 0; i < mEntries.size(); ++i)
    {
        const AtlasEntry& entry = mEntries[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (entry.missing)
            ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.35f, 1.0f), "%s (missing)", entry.name.c_str());
        else if (entry.empty)
            ImGui::TextColored(ImVec4(0.85f, 0.75f, 0.3f, 1.0f), "%s (fully transparent, skipped)",
                               entry.name.c_str());
        else
            ImGui::TextUnformatted(entry.name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%d x %d", entry.sourceWidth, entry.sourceHeight);
        ImGui::TableNextColumn();
        ImGui::Text("%d x %d", entry.trimWidth, entry.trimHeight);
        ImGui::TableNextColumn();
        const bool removed = ImGui::Button(ICON_MDI_CLOSE);
        ImGui::PopID();
        if (removed)
        {
            removeEntry(i);
            break;
        }
    }
    ImGui::EndTable();
}

void AtlasPanel::drawPreview()
{
    if (!mHasPacked)
    {
        ImGui::TextDisabled("Press Pack to preview the layout.");
        return;
    }
    ImGui::Text("%d page(s)", static_cast<int>(mPages.size()));
    if (mSkippedCount > 0)
        ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "%d image(s) did not fit and were skipped",
                           mSkippedCount);

    for (std::size_t p = 0; p < mPages.size(); ++p)
    {
        const AtlasPageResult& page = mPages[p];
        ImGui::Text("Page %d: %d x %d, %d region(s)", static_cast<int>(p), page.width, page.height,
                   static_cast<int>(page.regions.size()));

        const float previewWidth = ImGui::GetContentRegionAvail().x;
        const float scale = page.width > 0 ? previewWidth / static_cast<float>(page.width) : 1.0f;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 size(page.width * scale, page.height * scale);

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(30, 30, 34, 255));
        for (const AtlasPageResult::Region& region : page.regions)
        {
            const ImVec2 rectMin(origin.x + region.x * scale, origin.y + region.y * scale);
            const ImVec2 rectMax(rectMin.x + region.width * scale, rectMin.y + region.height * scale);
            drawList->AddRectFilled(rectMin, rectMax, IM_COL32(70, 120, 200, 160));
            drawList->AddRect(rectMin, rectMax, IM_COL32(160, 200, 255, 255));
        }
        ImGui::Dummy(size);
        ImGui::Spacing();
    }
}

void AtlasPanel::drawFileDialogPopup()
{
    if (mPendingAction == PendingAction::None)
        return;
    const std::filesystem::path anchor(startDirectory(app()).c_str());
    if (!mFileDialog.Render(anchor, anchor, anchor))
        return;
    const ImGuiFileDialog::Result result = mFileDialog.ConsumeResult();
    const PendingAction action = mPendingAction;
    mPendingAction = PendingAction::None;
    if (!result.accepted)
        return;
    const ct::String path(result.path.string().c_str());
    if (action == PendingAction::OpenAtlas)
        loadAtlas(path);
    else if (action == PendingAction::SaveAtlas)
        save(path);
}

void AtlasPanel::drawContents()
{
    drawToolbar();
    ImGui::Separator();
    drawSettings();
    ImGui::Separator();

    ImGui::Button(ICON_MDI_IMAGE_MULTIPLE "  Drop Images Here", ImVec2(-1.0f, 32.0f));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kTextureDragDropPayload))
            addImage(static_cast<const char*>(payload->Data));
        ImGui::EndDragDropTarget();
    }

    drawEntryList();
    ImGui::Separator();
    drawPreview();
    drawFileDialogPopup();
}

} // namespace k2d::editor
