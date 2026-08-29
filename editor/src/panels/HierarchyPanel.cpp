#include "HierarchyPanel.h"

#include "core/EditorApplication.h"
#include "panels/AssetsPanel.h"

#include <k2d/CameraComponent.h>
#include <k2d/GameObject.h>
#include <k2d/Scene.h>
#include <k2d/Serializer.h>

#include <ct/string.hpp>
#include <ct/vector.hpp>

#include <IconsMaterialDesignIcons.h>

namespace k2d::editor
{

namespace
{
GameObject *findById(GameObject &object, uint64_t id)
{
    if (object.id() == id)
        return &object;
    for (size_t i = 0; i < object.childCount(); ++i)
        if (GameObject *found = findById(*object.child(i), id))
            return found;
    return nullptr;
}

ct::String toLower(const ct::String &value)
{
    ct::String result = value;
    for (size_t i = 0; i < result.size(); ++i)
        if (result[i] >= 'A' && result[i] <= 'Z')
            result[i] = static_cast<char>(result[i] - 'A' + 'a');
    return result;
}

bool containsCaseInsensitive(const ct::String &haystack, const char *needle)
{
    if (!needle || !needle[0])
        return true;
    return toLower(haystack).find(toLower(ct::String(needle))) != ct::String::npos;
}
}

void HierarchyPanel::drawContents()
{
    if (ImGui::Button(ICON_MDI_PLUS))
        createNode();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("New Node");
    ImGui::SameLine();

    GameObject *primary = app().selection().resolve(app().scene());
    const bool hasPrimary = primary && primary != &app().scene().root() && primary->parent();

    ImGui::BeginDisabled(app().selection().count() == 0);
    if (ImGui::Button(ICON_MDI_CONTENT_DUPLICATE))
        duplicateSelected();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Duplicate Selected");
    ImGui::SameLine();

    ImGui::BeginDisabled(!hasPrimary);
    if (ImGui::Button(ICON_MDI_ARROW_UP_BOLD))
        moveSelected(-1);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Move Up");
    ImGui::SameLine();

    ImGui::BeginDisabled(!hasPrimary);
    if (ImGui::Button(ICON_MDI_ARROW_DOWN_BOLD))
        moveSelected(1);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Move Down");
    ImGui::SameLine();

    ImGui::BeginDisabled(app().selection().count() == 0);
    if (ImGui::Button(ICON_MDI_TRASH_CAN_OUTLINE))
        deleteSelected();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Delete Selected (Ctrl+Click to multi-select)");

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##hierarchy_search", ICON_MDI_MAGNIFY " Search...", mSearchFilter,
                             sizeof(mSearchFilter));

    ImGui::Separator();
    drawObject(app().scene().root());

    const ImVec2 blank = ImGui::GetContentRegionAvail();
    if (blank.y > 0.0f)
    {
        ImGui::InvisibleButton("##hierarchy_blank", ImVec2(blank.x > 0.0f ? blank.x : 1.0f, blank.y));
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            app().selection().clear();
    }
}

bool HierarchyPanel::subtreeMatchesFilter(GameObject &object) const
{
    if (mSearchFilter[0] == '\0')
        return true;
    if (containsCaseInsensitive(object.name(), mSearchFilter))
        return true;
    for (size_t i = 0; i < object.childCount(); ++i)
        if (subtreeMatchesFilter(*object.child(i)))
            return true;
    return false;
}

void HierarchyPanel::createNode()
{
    const EditorApplication::SceneChange before = app().beginChange();
    static unsigned int number = 1;
    ct::String name("Node ");
    name.append_number(number++);
    GameObject *parent = app().selection().resolve(app().scene());
    if (!parent)
        parent = &app().scene().root();
    GameObject *created = app().scene().createObject(name.c_str(), parent);
    app().selection().select(created);
    app().commitChange("Create Node", before);
}

void HierarchyPanel::createCameraNode()
{
    const EditorApplication::SceneChange before = app().beginChange();
    GameObject *parent = app().selection().resolve(app().scene());
    if (!parent)
        parent = &app().scene().root();
    GameObject *created = app().scene().createObject("Camera", parent);
    if (created)
        created->addComponent<CameraComponent>();
    app().selection().select(created);
    app().commitChange("Create Camera", before);
}

void HierarchyPanel::duplicateSelected()
{
    const ct::Vector<uint64_t> ids = app().selection().ids();
    if (ids.empty())
        return;

    const EditorApplication::SceneChange before = app().beginChange();
    ct::Vector<GameObject *> duplicates;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        GameObject *object = findById(app().scene().root(), ids[i]);
        if (!object || object == &app().scene().root() || !object->parent())
            continue;
        const ct::Json data = Serializer::WriteObject(*object, &app().assets());
        GameObject *duplicate = Serializer::ReadObject(app().scene(), data, object->parent(), &app().assets());
        if (duplicate)
            duplicates.push_back(duplicate);
    }
    if (duplicates.empty())
        return;

    app().selection().clear();
    for (size_t i = 0; i < duplicates.size(); ++i)
        app().selection().toggle(duplicates[i]);
    app().commitChange("Duplicate Node", before);
    ct::String message("Duplicated ");
    message.append_number(static_cast<unsigned int>(duplicates.size()));
    message += " node(s)";
    app().log(message);
    app().toasts().success(message);
}

void HierarchyPanel::moveSelected(int direction)
{
    GameObject *object = app().selection().resolve(app().scene());
    if (!object || !object->parent())
        return;

    const EditorApplication::SceneChange before = app().beginChange();
    const bool moved = direction < 0 ? object->parent()->moveChildUp(object)
                                     : object->parent()->moveChildDown(object);
    if (moved)
        app().commitChange(direction < 0 ? "Move Node Up" : "Move Node Down", before);
}

void HierarchyPanel::deleteSelected()
{
    const ct::Vector<uint64_t> ids = app().selection().ids();
    if (ids.empty())
        return;

    const EditorApplication::SceneChange before = app().beginChange();
    app().selection().clear();
    size_t deleted = 0;
    for (size_t i = 0; i < ids.size(); ++i)
    {
        GameObject *object = findById(app().scene().root(), ids[i]);
        if (!object || object == &app().scene().root())
            continue;
        if (app().scene().destroy(object))
            ++deleted;
    }
    app().scene().update(0.0f);
    if (deleted == 0)
        return;

    app().commitChange("Delete Selection", before);
    ct::String message("Deleted selection");
    app().log(message);
}

void HierarchyPanel::drawObject(GameObject &object)
{
    if (!subtreeMatchesFilter(object))
        return;

    const bool isRoot = &object == &app().scene().root();
    const bool filtering = mSearchFilter[0] != '\0';
    const bool selfMatches = !filtering || containsCaseInsensitive(object.name(), mSearchFilter);

    // A drop below may add a child mid-call; TreePop must follow what TreeNodeEx actually pushed.
    const bool hadChildren = object.childCount() != 0;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!hadChildren)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (app().selection().isSelected(object.id()))
        flags |= ImGuiTreeNodeFlags_Selected;
    if (filtering && hadChildren)
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);

    if (selfMatches && filtering)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.4f, 1.0f));
    const bool open = ImGui::TreeNodeEx(
        reinterpret_cast<void *>(static_cast<uintptr_t>(object.id())), flags,
        "%s", object.name().empty() ? "GameObject" : object.name().c_str());
    if (selfMatches && filtering)
        ImGui::PopStyleColor();

    // Selecting on press (IsItemClicked) would reassign app().selection() the
    // instant a drag starts, before ImGui even knows this press will become a
    // drag rather than a click - the Inspector then redraws for the newly
    // selected object and whatever drop target the user was dragging onto (a
    // Joint's target, the Camera's Follow field) vanishes mid-drag. Selecting
    // on release instead only fires for a plain click: a drag ends with the
    // mouse over the drop target, not back over this row, so IsItemHovered()
    // is false here on that release and the row's own selection never fires.
    if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        if (ImGui::GetIO().KeyCtrl)
            app().selection().toggle(&object);
        else
            app().selection().select(&object);
        ct::String message("Selected: ");
        message += object.name();
        app().log(message);
    }

    if (ImGui::BeginPopupContextItem())
    {
        if (!app().selection().isSelected(object.id()))
            app().selection().select(&object);

        if (ImGui::BeginMenu(ICON_MDI_PLUS " Create"))
        {
            if (ImGui::MenuItem("Empty"))
                createNode();
            if (ImGui::MenuItem(ICON_MDI_CAMERA " Camera"))
                createCameraNode();
            ImGui::EndMenu();
        }
        ImGui::BeginDisabled(isRoot);
        if (ImGui::MenuItem(ICON_MDI_CONTENT_DUPLICATE " Duplicate"))
            duplicateSelected();
        if (ImGui::MenuItem(object.locked() ? ICON_MDI_LOCK_OPEN " Unlock" : ICON_MDI_LOCK " Lock"))
        {
            const EditorApplication::SceneChange lockBefore = app().beginChange();
            object.setLocked(!object.locked());
            app().commitChange(object.locked() ? "Lock Node" : "Unlock Node", lockBefore);
        }
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_MDI_TRASH_CAN_OUTLINE " Delete"))
            deleteSelected();
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    if (!isRoot && ImGui::BeginDragDropSource())
    {
        const uint64_t id = object.id();
        ImGui::SetDragDropPayload(kNodeDragDropPayload, &id, sizeof(id));
        ImGui::TextUnformatted(object.name().empty() ? "GameObject" : object.name().c_str());
        ImGui::EndDragDropSource();
    }

    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kNodeDragDropPayload))
        {
            const uint64_t draggedId = *static_cast<const uint64_t *>(payload->Data);
            GameObject *dragged = findById(app().scene().root(), draggedId);
            if (dragged && dragged != &object)
            {
                const EditorApplication::SceneChange before = app().beginChange();
                if (app().scene().reparent(dragged, isRoot ? nullptr : &object))
                {
                    ct::String label("Reparent ");
                    label += dragged->name();
                    app().commitChange(label.c_str(), before);
                }
            }
        }
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kTextureDragDropPayload))
        {
            const char *path = static_cast<const char *>(payload->Data);
            app().createSpriteNodeFromImage(path, isRoot ? nullptr : &object);
        }
        ImGui::EndDragDropTarget();
    }

    if (open && hadChildren)
    {
        for (size_t i = 0; i < object.childCount(); ++i)
            drawObject(*object.child(i));
        ImGui::TreePop();
    }
}

}
