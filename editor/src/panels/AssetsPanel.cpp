#include "AssetsPanel.h"

#include "core/EditorApplication.h"

#include <ct/vector.hpp>

namespace k2d::editor
{

namespace
{
bool isPrefabFile(const ct::String &path)
{
    return EditorFileSystem::extension(path) == "k2dprefab";
}

bool isSceneFile(const ct::String &path)
{
    return EditorFileSystem::extension(path) == "k2dscene";
}
}

AssetsPanel::AssetsPanel(EditorApplication &application)
    : EditorPanel("Assets", application), mRoot(EditorFileSystem::currentDirectory())
{
    const ct::String &saved = application.settings().assetsDirectory;
    if (!saved.empty() && EditorFileSystem::isDirectory(saved))
        mRoot = saved;
}

void AssetsPanel::drawContents()
{
    app().settings().assetsDirectory = mRoot;

    ImGui::TextDisabled("%s", mRoot.c_str());
    ImGui::TextDisabled("[P] prefab (drag into Scene)   [S] scene   [D] folder");
    ImGui::Separator();

    ct::Vector<EditorFileEntry> entries;
    if (!EditorFileSystem::listDirectory(mRoot, entries))
        ImGui::TextDisabled("Unable to read this folder.");

    for (const EditorFileEntry &entry : entries)
        drawEntry(entry);

    if (ImGui::Button("Up"))
    {
        mRoot = EditorFileSystem::parentPath(mRoot);
        ct::String message("Assets folder: ");
        message += mRoot;
        app().log(message);
    }
}

void AssetsPanel::drawEntry(const EditorFileEntry &entry)
{
    const bool prefab = !entry.directory && isPrefabFile(entry.path);
    const bool scene = !entry.directory && isSceneFile(entry.path);

    ct::String label(entry.directory ? "[D] " : prefab ? "[P] " : scene ? "[S] " : "    ");
    label += entry.name;

    const bool selected = prefab && app().previewPrefabPath() == entry.path;
    const bool clicked =
        ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick);

    if (prefab && ImGui::BeginDragDropSource())
    {
        ImGui::SetDragDropPayload(kPrefabDragDropPayload, entry.path.c_str(), entry.path.size() + 1);
        ImGui::TextUnformatted(entry.name.c_str());
        ImGui::EndDragDropSource();
    }

    const bool doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    if (clicked && entry.directory && doubleClicked)
    {
        mRoot = entry.path;
        ct::String message("Assets folder: ");
        message += mRoot;
        app().log(message);
    }
    else if (clicked && prefab && !doubleClicked)
    {
        app().previewPrefab(entry.path.c_str());
        ct::String message("Prefab preview: ");
        message += entry.name;
        app().log(message);
    }
}

}
