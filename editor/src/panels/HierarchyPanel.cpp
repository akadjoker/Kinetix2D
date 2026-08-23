#include "HierarchyPanel.h"

#include "core/EditorApplication.h"

#include <k2d/GameObject.h>
#include <k2d/Scene.h>
#include <k2d/SpriteComponent.h>
#include <k2d/Texture.h>

#include <ct/string.hpp>

#include <IconsMaterialDesignIcons.h>

namespace k2d::editor
{

namespace
{
constexpr int kPlaceholderSize = 32;

Texture *placeholderSpriteTexture(EditorApplication &application)
{
    constexpr const char *kName = "__editor_sprite_placeholder";
    Texture *texture = application.assets().GetTexture(kName);
    if (texture)
        return texture;

    unsigned char pixels[kPlaceholderSize * kPlaceholderSize * 4];
    for (int i = 0; i < kPlaceholderSize * kPlaceholderSize; ++i)
    {
        pixels[i * 4 + 0] = 255;
        pixels[i * 4 + 1] = 255;
        pixels[i * 4 + 2] = 255;
        pixels[i * 4 + 3] = 255;
    }
    return application.assets().CreateTexture(kName, kPlaceholderSize, kPlaceholderSize, pixels, true, false);
}
}

void HierarchyPanel::drawContents()
{
    if (ImGui::Button(ICON_MDI_PLUS))
        ImGui::OpenPopup("##hierarchy_create");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Create GameObject");
    if (ImGui::BeginPopup("##hierarchy_create"))
    {
        if (ImGui::MenuItem("Empty"))
            createEmpty();
        if (ImGui::MenuItem("Sprite"))
            createSprite();
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    GameObject *selected = app().selection().resolve(app().scene());
    ImGui::BeginDisabled(!selected || selected == &app().scene().root());
    if (ImGui::Button(ICON_MDI_TRASH_CAN_OUTLINE))
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
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Delete selected GameObject");

    ImGui::Separator();
    drawObject(app().scene().root());
}

void HierarchyPanel::createEmpty()
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

void HierarchyPanel::createSprite()
{
    const EditorApplication::SceneChange before = app().beginChange();
    static unsigned int number = 1;
    ct::String name("Sprite ");
    name.append_number(number++);
    GameObject *parent = app().selection().resolve(app().scene());
    if (!parent)
        parent = &app().scene().root();
    GameObject *created = app().scene().createObject(name.c_str(), parent);
    if (created)
        created->addComponent<SpriteComponent>(placeholderSpriteTexture(app()));
    app().selection().select(created);
    app().commitChange("Create Sprite", before);
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
