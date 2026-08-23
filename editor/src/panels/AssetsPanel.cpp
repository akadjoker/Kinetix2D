#include "AssetsPanel.h"

#include "core/EditorApplication.h"

#include <k2d/FileSystem.h>
#include <k2d/Texture.h>
#include <IconsMaterialDesignIcons.h>

#include <cstdint>

namespace k2d::editor
{

namespace
{
bool isImage(const ct::String &ext)
{
    return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" ||
          ext == "tga" || ext == "gif" || ext == "webp";
}

bool isPrefab(const ct::String &ext) { return ext == "k2dprefab"; }
bool isScene(const ct::String &ext) { return ext == "k2dscene"; }

bool isCode(const ct::String &ext)
{
    return ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp" || ext == "glsl" ||
          ext == "vert" || ext == "frag" || ext == "lua" || ext == "py";
}

bool isDocument(const ct::String &ext)
{
    return ext == "txt" || ext == "md" || ext == "json" || ext == "xml" ||
          ext == "yaml" || ext == "yml" || ext == "ini" || ext == "cfg";
}

bool isAudio(const ct::String &ext)
{
    return ext == "wav" || ext == "mp3" || ext == "ogg" || ext == "flac";
}

const char *iconFor(bool directory, const ct::String &ext)
{
    if (directory)
        return ICON_MDI_FOLDER;
    if (isPrefab(ext))
        return ICON_MDI_CUBE_OUTLINE;
    if (isScene(ext))
        return ICON_MDI_SHAPE;
    if (isImage(ext))
        return ICON_MDI_FILE_IMAGE;
    if (isCode(ext))
        return ICON_MDI_CODE_BRACES;
    if (isAudio(ext))
        return ICON_MDI_FILE_MUSIC;
    if (isDocument(ext))
        return ICON_MDI_FILE_DOCUMENT;
    return ICON_MDI_FILE;
}

ImVec4 colorFor(bool directory, const ct::String &ext)
{
    if (directory)
        return ImVec4(0.95f, 0.78f, 0.35f, 1.0f);
    if (isPrefab(ext))
        return ImVec4(0.72f, 0.55f, 0.95f, 1.0f);
    if (isScene(ext))
        return ImVec4(0.55f, 0.70f, 0.95f, 1.0f);
    if (isImage(ext))
        return ImVec4(0.55f, 0.85f, 0.55f, 1.0f);
    if (isCode(ext))
        return ImVec4(0.6f, 0.65f, 0.95f, 1.0f);
    if (isAudio(ext))
        return ImVec4(0.9f, 0.55f, 0.65f, 1.0f);
    if (isDocument(ext))
        return ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
}
}

AssetsPanel::AssetsPanel(EditorApplication &application)
    : EditorPanel("Assets", application)
{
    const char *basePath = FileSystem::Instance().BasePath();
    mRoot = (basePath && basePath[0]) ? ct::String(basePath) : EditorFileSystem::currentDirectory();

    const ct::String &saved = application.settings().assetsDirectory;
    if (!saved.empty() && EditorFileSystem::isDirectory(saved))
        mRoot = saved;
    mHistory.push_back(mRoot);
    mHistoryPosition = 0;
}

AssetsPanel::~AssetsPanel()
{
    for (auto &entry : mThumbnailCache)
        delete entry.value;
    mThumbnailCache.clear();
}

void AssetsPanel::navigateTo(const ct::String &directory)
{
    if (directory == mRoot || !EditorFileSystem::isDirectory(directory))
        return;

    while (mHistory.size() > mHistoryPosition + 1)
        mHistory.pop_back();
    mHistory.push_back(directory);
    mHistoryPosition = mHistory.size() - 1;
    mRoot = directory;
    mEntriesDirty = true;
}

void AssetsPanel::refreshEntries()
{
    if (!mEntriesDirty && mCachedDirectory == mRoot)
        return;

    mEntriesValid = EditorFileSystem::listDirectory(mRoot, mEntries);
    if (!mEntriesValid)
        mEntries.clear();
    mCachedDirectory = mRoot;
    mEntriesDirty = false;
}

Texture *AssetsPanel::thumbnailFor(const EditorFileEntry &entry)
{
    Texture **cached = mThumbnailCache.find(entry.path);
    if (cached)
        return *cached;

    Texture *texture = new Texture();
    if (!texture->Load(entry.path.c_str(), true, false))
    {
        delete texture;
        texture = nullptr;
    }
    mThumbnailCache[entry.path] = texture;
    return texture;
}

void AssetsPanel::drawToolbar()
{
    ImGui::BeginDisabled(mHistoryPosition == 0);
    if (ImGui::Button(ICON_MDI_ARROW_LEFT))
    {
        --mHistoryPosition;
        mRoot = mHistory[mHistoryPosition];
        mEntriesDirty = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(mHistoryPosition + 1 >= mHistory.size());
    if (ImGui::Button(ICON_MDI_ARROW_RIGHT))
    {
        ++mHistoryPosition;
        mRoot = mHistory[mHistoryPosition];
        mEntriesDirty = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_ARROW_UP))
        navigateTo(EditorFileSystem::parentPath(mRoot));
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_REFRESH))
        mEntriesDirty = true;
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_VIEW_GRID))
        mViewMode = ViewMode::Grid;
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_VIEW_LIST))
        mViewMode = ViewMode::List;
    if (mViewMode == ViewMode::Grid)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("##assets_zoom", &mThumbnailSize, 48.0f, 220.0f, ICON_MDI_MAGNIFY " %.0f");
    }
}

void AssetsPanel::drawBreadcrumb()
{
    ct::Vector<ct::String> segments;
    ct::String current;
    for (size_t i = 0; i < mRoot.size(); ++i)
    {
        if (mRoot[i] == '/' || mRoot[i] == '\\')
        {
            if (!current.empty())
            {
                segments.push_back(current);
                current.clear();
            }
        }
        else
        {
            current.push_back(mRoot[i]);
        }
    }
    if (!current.empty())
        segments.push_back(current);

    ct::String accumulated = "/";
    ImGui::PushID("assets_crumb_root");
    if (ImGui::Selectable("/", false, ImGuiSelectableFlags_None, ImVec2(ImGui::CalcTextSize("/").x, 0.0f)))
        navigateTo(accumulated);
    ImGui::PopID();

    for (size_t i = 0; i < segments.size(); ++i)
    {
        accumulated = EditorFileSystem::join(accumulated, segments[i].c_str());
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::TextUnformatted("/");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(segments[i].c_str(), false, ImGuiSelectableFlags_None,
                              ImVec2(ImGui::CalcTextSize(segments[i].c_str()).x, 0.0f)))
            navigateTo(accumulated);
        ImGui::PopID();
    }
}

void AssetsPanel::drawDirectoryTree(const ct::String &directory)
{
    ct::Vector<EditorFileEntry> children;
    EditorFileSystem::listDirectory(directory, children);
    for (size_t i = 0; i < children.size(); ++i)
    {
        const EditorFileEntry &child = children[i];
        if (!child.directory)
            continue;

        ImGui::PushID(child.path.c_str());
        ImGuiStorage *storage = ImGui::GetStateStorage();
        const ImGuiID openId = ImGui::GetID("##open");
        bool open = storage->GetBool(openId, false);
        if (ImGui::ArrowButton("##expand", open ? ImGuiDir_Down : ImGuiDir_Right))
        {
            open = !open;
            storage->SetBool(openId, open);
        }
        ImGui::SameLine(0.0f, 4.0f);
        ct::String rowLabel(ICON_MDI_FOLDER);
        rowLabel += " ";
        rowLabel += child.name;
        if (ImGui::Selectable(rowLabel.c_str(), child.path == mRoot))
            navigateTo(child.path);
        if (open)
        {
            ImGui::Indent();
            drawDirectoryTree(child.path);
            ImGui::Unindent();
        }
        ImGui::PopID();
    }
}

void AssetsPanel::drawBookmark(const char *label, const ct::String &root)
{
    if (root.empty() || !EditorFileSystem::isDirectory(root))
        return;

    ImGui::PushID(root.c_str());
    ImGuiStorage *storage = ImGui::GetStateStorage();
    const ImGuiID openId = ImGui::GetID("##open");
    bool open = storage->GetBool(openId, false);
    if (ImGui::ArrowButton("##expand", open ? ImGuiDir_Down : ImGuiDir_Right))
    {
        open = !open;
        storage->SetBool(openId, open);
    }
    ImGui::SameLine(0.0f, 4.0f);
    ct::String rowLabel(ICON_MDI_FOLDER);
    rowLabel += " ";
    rowLabel += label;
    if (ImGui::Selectable(rowLabel.c_str(), root == mRoot))
        navigateTo(root);
    if (open)
    {
        ImGui::Indent();
        drawDirectoryTree(root);
        ImGui::Unindent();
    }
    ImGui::PopID();
}

void AssetsPanel::drawSidebar()
{
    ImGui::BeginChild("##assets_tree", ImVec2(mSidebarWidth, 0.0f), true,
                      ImGuiWindowFlags_HorizontalScrollbar);
    if (app().project().valid())
        drawBookmark("Project", app().project().assetsDirectory());
    drawBookmark("Filesystem", "/");
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::InvisibleButton("##assets_tree_splitter", ImVec2(4.0f, ImGui::GetContentRegionAvail().y));
    if (ImGui::IsItemActive())
    {
        mSidebarWidth += ImGui::GetIO().MouseDelta.x;
        if (mSidebarWidth < 120.0f)
            mSidebarWidth = 120.0f;
        if (mSidebarWidth > 420.0f)
            mSidebarWidth = 420.0f;
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    ImGui::SameLine();
}

void AssetsPanel::handleEntryInteraction(const EditorFileEntry &entry, bool clicked, bool doubleClicked)
{
    if (!clicked)
        return;

    if (entry.directory)
    {
        if (doubleClicked)
        {
            navigateTo(entry.path);
            ct::String message("Assets folder: ");
            message += entry.path;
            app().log(message);
        }
        return;
    }

    const ct::String ext = EditorFileSystem::extension(entry.path);
    if (isPrefab(ext) && !doubleClicked)
    {
        app().previewPrefab(entry.path.c_str());
        ct::String message("Prefab preview: ");
        message += entry.name;
        app().log(message);
    }
}

void AssetsPanel::drawGrid()
{
    const float cell = mThumbnailSize;
    const float panelWidth = ImGui::GetContentRegionAvail().x;
    int columns = static_cast<int>(panelWidth / (cell + 16.0f));
    if (columns < 1)
        columns = 1;

    if (!ImGui::BeginTable("##assets_grid", columns, ImGuiTableFlags_None))
        return;

    for (size_t i = 0; i < mEntries.size(); ++i)
    {
        ImGui::TableNextColumn();
        const EditorFileEntry &entry = mEntries[i];
        const ct::String ext = entry.directory ? ct::String() : EditorFileSystem::extension(entry.path);

        ImGui::PushID(static_cast<int>(i));
        ImGui::BeginGroup();

        ImGui::InvisibleButton("##cell", ImVec2(cell, cell));
        const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        const bool doubleClicked = clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 cellMin = ImGui::GetItemRectMin();
        const ImVec2 cellMax = ImGui::GetItemRectMax();
        ImDrawList *drawList = ImGui::GetWindowDrawList();

        if (hovered)
            drawList->AddRectFilled(cellMin, cellMax, IM_COL32(60, 66, 78, 120), 4.0f);

        const bool selected = isPrefab(ext) && app().previewPrefabPath() == entry.path;
        if (selected)
            drawList->AddRect(cellMin, cellMax, IM_COL32(255, 205, 65, 220), 4.0f, 0, 2.0f);

        Texture *texture = !entry.directory && isImage(ext) ? thumbnailFor(entry) : nullptr;
        if (texture)
        {
            drawList->AddImage((ImTextureID)(intptr_t)texture->Id(), cellMin, cellMax);
        }
        else
        {
            const char *icon = iconFor(entry.directory, ext);
            const ImVec4 color = colorFor(entry.directory, ext);
            const ImVec2 textSize = ImGui::CalcTextSize(icon);
            const ImVec2 iconPos((cellMin.x + cellMax.x - textSize.x) * 0.5f,
                                 (cellMin.y + cellMax.y - textSize.y) * 0.5f);
            drawList->AddText(iconPos, ImGui::ColorConvertFloat4ToU32(color), icon);
        }

        if (!entry.directory && isPrefab(ext) && ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(kPrefabDragDropPayload, entry.path.c_str(), entry.path.size() + 1);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::EndDragDropSource();
        }

        handleEntryInteraction(entry, clicked, doubleClicked);

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + cell);
        ImGui::TextWrapped("%s", entry.name.c_str());
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();
        ImGui::PopID();
    }

    ImGui::EndTable();
}

void AssetsPanel::drawList()
{
    for (size_t i = 0; i < mEntries.size(); ++i)
    {
        const EditorFileEntry &entry = mEntries[i];
        const ct::String ext = entry.directory ? ct::String() : EditorFileSystem::extension(entry.path);
        const bool selected = isPrefab(ext) && app().previewPrefabPath() == entry.path;

        ImGui::PushID(static_cast<int>(i));
        const char *icon = iconFor(entry.directory, ext);
        const ImVec4 color = colorFor(entry.directory, ext);
        ImGui::TextColored(color, "%s", icon);
        ImGui::SameLine();

        const bool clicked =
            ImGui::Selectable(entry.name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick);
        const bool doubleClicked = clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

        if (!entry.directory && isPrefab(ext) && ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload(kPrefabDragDropPayload, entry.path.c_str(), entry.path.size() + 1);
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::EndDragDropSource();
        }

        handleEntryInteraction(entry, clicked, doubleClicked);
        ImGui::PopID();
    }
}

void AssetsPanel::drawContents()
{
    app().settings().assetsDirectory = mRoot;

    drawToolbar();
    ImGui::Separator();
    drawBreadcrumb();
    ImGui::Separator();

    refreshEntries();
    drawSidebar();

    ImGui::BeginChild("##assets_content", ImVec2(0.0f, 0.0f), false);
    if (!mEntriesValid)
        ImGui::TextDisabled("Unable to read this folder.");
    else if (mEntries.empty())
        ImGui::TextDisabled("Empty folder.");
    else if (mViewMode == ViewMode::Grid)
        drawGrid();
    else
        drawList();
    ImGui::EndChild();
}

}
