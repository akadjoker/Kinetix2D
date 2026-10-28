#include "InspectorPanel.h"

#include "Icons.h"
#include "PropertyFields.h"

#include "ComboBox.hpp"
#include "WidgetApp.hpp"

#include <k2d/ClassDB.h>
#include <k2d/GameObject.h>
#include <k2d/Serializer.h>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{
ct::String editName(const char* property)
{
    return ct::String("Edit ") + property;
}
} // namespace

InspectorPanel::InspectorPanel()
{
    mColumn = setContent<rt::BoxLayout>(rt::LayoutDir::Vertical);
    mColumn->setPadding(6.0f);
    mColumn->setSpacing(4.0f);
}

Component* InspectorPanel::component(ComponentType type, std::size_t index) const
{
    if (!mObject || index >= mObject->rawComponentCount(type))
        return nullptr;
    return mObject->rawComponent(type, index);
}

void InspectorPanel::clear()
{
    while (!mColumn->children().empty())
        mColumn->removeChild(mColumn->children().back());
}

void InspectorPanel::show(GameObject* object)
{
    clear();
    mObject = object;
    if (!object)
        mColumn->createChild<rt::Label>("Nothing selected");
    else
    {
        addTransform();
        for (std::size_t type = 0; type < static_cast<std::size_t>(ComponentType::Count); ++type)
        {
            const ComponentType componentType = static_cast<ComponentType>(type);
            const std::size_t count = object->rawComponentCount(componentType);
            for (std::size_t index = 0; index < count; ++index)
                addComponent(componentType, index);
        }
        addComponentPicker();
    }
    setScrollY(0.0f);
    rt::WidgetApp::instance().requestLayout();
}

void InspectorPanel::addTransform()
{
    // The transform is not a component, but reads like one: rows from the kit.
    auto edit = [this](const char* label) {
        return [this, label](bool final) { edited.emit(label, final); };
    };
    FieldBinding name;
    name.get = [this]() { return PropertyValue::from(mObject->name()); };
    name.set = [this](const PropertyValue& v) { mObject->setName(v.asString()); };
    addPropertyRow(mColumn, PropertyInfo(PropertyType::String, "Name"), name, edit("Rename"));

    auto* section = mColumn->createChild<rt::Collapsible>("Transform");
    FieldBinding position;
    position.get = [this]() { return PropertyValue::from(mObject->position()); };
    position.set = [this](const PropertyValue& v) { mObject->setPosition(v.asVec2()); };
    addPropertyRow(section, PropertyInfo(PropertyType::Vec2, "Position"), position, edit("Move"));
    FieldBinding rotation;
    rotation.get = [this]() { return PropertyValue::from(mObject->rotationDegrees()); };
    rotation.set = [this](const PropertyValue& v) { mObject->setRotationDegrees(v.asFloat()); };
    addPropertyRow(section, PropertyInfo(PropertyType::Float, "Rotation"), rotation, edit("Rotate"));
    FieldBinding scale;
    scale.get = [this]() { return PropertyValue::from(mObject->scale()); };
    scale.set = [this](const PropertyValue& v) { mObject->setScale(v.asVec2()); };
    addPropertyRow(section, PropertyInfo(PropertyType::Vec2, "Scale"), scale, edit("Scale"));
}

void InspectorPanel::addComponent(ComponentType type, std::size_t index)
{
    Component* target = component(type, index);
    const ClassInfo* info = target ? ClassDB::find(*target) : nullptr;
    if (!info)
        return;
    auto* section = mColumn->createChild<rt::Collapsible>(info->name);
    for (std::size_t p = 0; p < info->properties.size(); ++p)
        addProperty(section, type, index, info->properties[p]);
    iconButton(section, "Remove", "Remove this component",
               [this, target]() { removeComponentRequested.emit(target); });
}

void InspectorPanel::addComponentPicker()
{
    auto* picker = mColumn->createChild<rt::ComboBox>();
    picker->addItem("Add Component...");
    for (std::size_t i = 0; i < Serializer::TypeCount(); ++i)
        picker->addItem(Serializer::TypeName(i));
    picker->setSelectedIndex(0);
    picker->setMaxVisible(16);
    picker->selectionChanged.connect([this, picker](int index) {
        if (index <= 0)
            return;
        addComponentRequested.emit(Serializer::TypeName(static_cast<std::size_t>(index - 1)));
        picker->setSelectedIndex(0);
    });
}

void InspectorPanel::addProperty(rt::Widget* parent, ComponentType type, std::size_t index,
                                 const ClassProperty& property)
{
    // The component is looked up again on every read and write, so a row never
    // holds a pointer into an object that may be rebuilt.
    const ClassProperty* bound = &property;
    FieldBinding binding;
    binding.get = [this, type, index, bound]() {
        const Component* c = component(type, index);
        return c ? bound->get(*c) : PropertyValue();
    };
    binding.set = [this, type, index, bound](const PropertyValue& v) {
        if (Component* c = component(type, index))
            bound->set(*c, v);
    };
    const ct::String label = editName(property.info.name.c_str());
    addPropertyRow(parent, property.info, binding, [this, label](bool final) { edited.emit(label.c_str(), final); });
}

} // namespace k2d::editor
