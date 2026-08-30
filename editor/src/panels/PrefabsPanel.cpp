#include "PrefabsPanel.h"

#include "core/EditorApplication.h"

namespace k2d::editor
{

void PrefabsPanel::drawContents()
{
    const ct::String &path = app().previewPrefabPath();
    if (path.empty())
    {
        ImGui::TextDisabled("Click a prefab in Assets to preview its hierarchy here.");
        return;
    }

    if (path != mLoadedPath)
    {
        if (mPrefab.Load(path.c_str()))
        {
            mLoadedPath = path;
        }
        else
        {
            ImGui::TextColored(ImVec4(0.9f, 0.45f, 0.45f, 1.0f), "Could not load: %s", path.c_str());
            mLoadedPath.clear();
            return;
        }
    }

    if (!mPrefab.valid())
        return;

    ImGui::TextDisabled("%s", mLoadedPath.c_str());
    ImGui::BeginDisabled(app().prefabModeActive());
    if (ImGui::Button("Edit"))
    {
        app().enterPrefabMode(mLoadedPath.c_str());
        return;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Clear Preview"))
    {
        app().previewPrefab("");
        mLoadedPath.clear();
        return;
    }
    ImGui::Separator();
    drawNode(mPrefab.data(), 0);
}

void PrefabsPanel::drawNode(const ct::Json &node, int index)
{
    const char *name = node["name"].as_cstr("GameObject");
    const ct::Json &children = node["children"];
    const bool hasChildren = children.is_array() && children.size() > 0;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_DefaultOpen;
    if (!hasChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    ImGui::PushID(index);
    const bool open = ImGui::TreeNodeEx(name, flags, "%s", name);
    if (open && hasChildren)
    {
        for (size_t i = 0; i < children.size(); ++i)
            drawNode(children[i], static_cast<int>(i));
        ImGui::TreePop();
    }
    ImGui::PopID();
}

}
