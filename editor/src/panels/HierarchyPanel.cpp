#include "HierarchyPanel.h"

#include "core/EditorApplication.h"

#include <k2d/GameObject.h>
#include <k2d/Scene.h>

#include <ct/string.hpp>

namespace k2d::editor
{

void HierarchyPanel::drawContents()
{
    if (ImGui::Button("+ Empty"))
    {
        const EditorApplication::SceneChange before = app().beginChange();
        static unsigned int number = 1;
        ct::String name("GameObject ");
        name.append_number(number++);
        GameObject *parent = app().selection().resolve(app().scene());
        if (!parent)
            parent = &app().scene().root();
        GameObject *created = app().scene().createObject(name.c_str(), parent);
        app().selection().select(created);
        app().commitChange("Create GameObject", before);
    }

    ImGui::SameLine();
    GameObject *selected = app().selection().resolve(app().scene());
    if (ImGui::Button("Delete") && selected && selected != &app().scene().root())
    {
        const EditorApplication::SceneChange before = app().beginChange();
        const ct::String name = selected->name();
        app().selection().clear();
        app().scene().destroy(selected);
        app().scene().update(0.0f);
        ct::String label("Delete ");
        label += name;
        app().commitChange(label.c_str(), before);
        ct::String message("Deleted selection: ");
        message += name;
        app().log(message);
    }

    ImGui::Separator();
    drawObject(app().scene().root());
}

void HierarchyPanel::drawObject(GameObject &object)
{
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (object.childCount() == 0)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (app().selection().hasSelection() && app().selection().objectId() == object.id())
        flags |= ImGuiTreeNodeFlags_Selected;

    const bool open = ImGui::TreeNodeEx(
        reinterpret_cast<void *>(static_cast<uintptr_t>(object.id())), flags,
        "%s", object.name().empty() ? "GameObject" : object.name().c_str());

    if (ImGui::IsItemClicked())
    {
        app().selection().select(&object);
        ct::String message("Selected: ");
        message += object.name();
        app().log(message);
    }

    if (open && object.childCount() != 0)
    {
        for (size_t i = 0; i < object.childCount(); ++i)
            drawObject(*object.child(i));
        ImGui::TreePop();
    }
}

}
