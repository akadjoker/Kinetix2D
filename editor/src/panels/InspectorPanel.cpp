#include "InspectorPanel.h"

#include "core/EditorApplication.h"

#include <k2d/Component.h>
#include <k2d/GameObject.h>

namespace k2d::editor
{

namespace
{
const char *componentName(ComponentType type)
{
    static const char *names[] = {
        "Sprite", "Script", "Camera", "TileMap", "SpriteBatch", "Polygon2D",
        "Animation2D", "Light2D", "LightOccluder2D", "Line2D", "NinePatch",
        "Particle"
    };
    const unsigned int index = static_cast<unsigned int>(type);
    return index < static_cast<unsigned int>(ComponentType::Count) ? names[index] : "Unknown";
}
}

void InspectorPanel::drawContents()
{
    GameObject *object = app().selection().resolve(app().scene());
    if (!object)
    {
        ImGui::TextDisabled("Select an object in the Hierarchy or Scene view.");
        return;
    }

    char name[256];
    size_t nameLength = object->name().size();
    if (nameLength >= sizeof(name))
        nameLength = sizeof(name) - 1;
    for (size_t i = 0; i < nameLength; ++i)
        name[i] = object->name()[i];
    name[nameLength] = '\0';
    EditorApplication::SceneChange before = app().beginChange();
    const bool nameChanged = ImGui::InputText("Name", name, sizeof(name));
    if (ImGui::IsItemActivated())
        app().beginTransaction("Rename GameObject", before);
    if (nameChanged)
        object->setName(name);
    if (ImGui::IsItemDeactivatedAfterEdit())
        app().commitTransaction();

    bool active = object->active();
    before = app().beginChange();
    if (ImGui::Checkbox("Active", &active))
    {
        object->setActive(active);
        app().commitChange(active ? "Enable GameObject" : "Disable GameObject", before);
    }
    ImGui::SameLine();
    bool visible = object->visible();
    before = app().beginChange();
    if (ImGui::Checkbox("Visible", &visible))
    {
        object->setVisible(visible);
        app().commitChange(visible ? "Show GameObject" : "Hide GameObject", before);
    }

    Math::Vec2 position = object->position();
    float positionValues[2] = {position.x, position.y};
    before = app().beginChange();
    const bool positionChanged = ImGui::DragFloat2("Position", positionValues, 0.25f);
    if (ImGui::IsItemActivated())
        app().beginTransaction("Move GameObject", before);
    if (positionChanged)
        object->setPosition(Math::Vec2(positionValues[0], positionValues[1]));
    if (ImGui::IsItemDeactivatedAfterEdit())
        app().commitTransaction();

    float rotation = object->rotationDegrees();
    before = app().beginChange();
    const bool rotationChanged = ImGui::DragFloat("Rotation", &rotation, 0.25f);
    if (ImGui::IsItemActivated())
        app().beginTransaction("Rotate GameObject", before);
    if (rotationChanged)
        object->setRotationDegrees(rotation);
    if (ImGui::IsItemDeactivatedAfterEdit())
        app().commitTransaction();

    Math::Vec2 scale = object->scale();
    float scaleValues[2] = {scale.x, scale.y};
    before = app().beginChange();
    const bool scaleChanged = ImGui::DragFloat2("Scale", scaleValues, 0.01f);
    if (ImGui::IsItemActivated())
        app().beginTransaction("Scale GameObject", before);
    if (scaleChanged)
        object->setScale(Math::Vec2(scaleValues[0], scaleValues[1]));
    if (ImGui::IsItemDeactivatedAfterEdit())
        app().commitTransaction();

    int zIndex = object->zIndex();
    before = app().beginChange();
    const bool zChanged = ImGui::DragInt("Z Index", &zIndex);
    if (ImGui::IsItemActivated())
        app().beginTransaction("Change Z Index", before);
    if (zChanged)
        object->setZIndex(zIndex);
    if (ImGui::IsItemDeactivatedAfterEdit())
        app().commitTransaction();

    ImGui::SeparatorText("Components");
    bool any = false;
    for (unsigned int value = 0; value < static_cast<unsigned int>(ComponentType::Count); ++value)
    {
        const ComponentType type = static_cast<ComponentType>(value);
        const size_t count = object->rawComponentCount(type);
        for (size_t index = 0; index < count; ++index)
        {
            Component *component = object->rawComponent(type, index);
            if (!component)
                continue;
            any = true;
            ImGui::PushID(static_cast<int>(component->id()));
            bool componentActive = component->active();
            before = app().beginChange();
            if (ImGui::Checkbox("##active", &componentActive))
            {
                component->setActive(componentActive);
                app().commitChange(componentActive ? "Enable Component" : "Disable Component", before);
            }
            ImGui::SameLine();
            ImGui::Text("%s  #%u", componentName(type), component->id());
            ImGui::PopID();
        }
    }
    if (!any)
        ImGui::TextDisabled("No components yet.");

    if (ImGui::Button("Add Component..."))
        ImGui::OpenPopup("Add Component");
    if (ImGui::BeginPopup("Add Component"))
    {
        ImGui::TextDisabled("ComponentRegistry will populate this menu in M0/M2.");
        ImGui::EndPopup();
    }
}

}
