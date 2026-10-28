#include "InspectorPanel.h"

#include "Icons.h"
#include "PropertyFields.h"
#include "core/EditorFileSystem.h"

#include "ComboBox.hpp"
#include "WidgetApp.hpp"

#include <k2d/ClassDB.h>

#include <cstdlib>
#include <cstring>
#include <k2d/GameObject.h>
#include <k2d/FileBuffer.h>
#include <k2d/FileSystem.h>
#include <k2d/Serializer.h>
#include <k2d/ZenScriptComponent.h>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{
ct::String editName(const char* property)
{
    return ct::String("Edit ") + property;
}

const char* skipSpaces(const char* p, const char* stop)
{
    while (p < stop && (*p == ' ' || *p == '\t'))
        ++p;
    return p;
}

// "speed = 240  # range(0, 500[, 5])" on the line that sets a variable, as
// Godot's export_range: the comment leaves the script itself alone.
bool scriptRange(const char* source, const ct::String& name, PropertyInfo& info)
{
    for (const char* line = source; line && *line;)
    {
        const char* end = std::strchr(line, '\n');
        const char* stop = end ? end : line + std::strlen(line);
        const char* p = skipSpaces(line, stop);
        if (stop - p > 5 && std::strncmp(p, "self.", 5) == 0)
            p += 5;
        if (static_cast<std::size_t>(stop - p) > name.size() && std::strncmp(p, name.c_str(), name.size()) == 0)
        {
            const char* q = skipSpaces(p + name.size(), stop);
            if (q < stop && *q == ':')
                while (q < stop && *q != '=')
                    ++q;
            const char* hash = q < stop && *q == '=' ? static_cast<const char*>(std::memchr(q, '#', stop - q)) : nullptr;
            const char* r = hash ? skipSpaces(hash + 1, stop) : nullptr;
            if (r && stop - r > 6 && std::strncmp(r, "range(", 6) == 0)
            {
                char* next = nullptr;
                const float minimum = std::strtof(r + 6, &next);
                const float maximum = next && *next == ',' ? std::strtof(next + 1, &next) : minimum;
                const float step = next && *next == ',' ? std::strtof(next + 1, &next) : 0.0f;
                if (maximum > minimum)
                {
                    const bool integer = info.type == PropertyType::Int;
                    info = PropertyInfo::range(info.type, name.c_str(), minimum, maximum,
                                               step > 0.0f ? step : (integer ? 1.0f : 0.01f));
                    return true;
                }
            }
        }
        line = end ? end + 1 : stop;
    }
    return false;
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

void InspectorPanel::refresh()
{
    const float y = scrollY();
    show(mObject);
    setScrollY(y);
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
    addPropertyRow(section, PropertyInfo::angle("Rotation"), rotation, edit("Rotate"));
    FieldBinding scale;
    scale.get = [this]() { return PropertyValue::from(mObject->scale()); };
    scale.set = [this](const PropertyValue& v) { mObject->setScale(v.asVec2()); };
    // A negative scale flips, so the slider is open at both ends.
    PropertyInfo scaleInfo = PropertyInfo::range(PropertyType::Vec2, "Scale", 0.0f, 4.0f, 0.01f);
    scaleInfo.hintString = "or_greater,or_less";
    addPropertyRow(section, scaleInfo, scale, edit("Scale"));
}

ZenScriptComponent* InspectorPanel::script(ComponentType type, std::size_t index) const
{
    Component* target = component(type, index);
    return target && target->componentClass() == ComponentClass::ZenScript ? static_cast<ZenScriptComponent*>(target)
                                                                             : nullptr;
}

// The script's variables, read from its source so they show while nothing
// runs; a value set here is this object's own, written with the scene.
void InspectorPanel::addScript(ComponentType type, std::size_t index)
{
    ZenScriptComponent* target = script(type, index);
    const ct::String path = target->scriptPath();
    auto* section = mColumn->createChild<rt::Collapsible>(
        path.empty() ? ct::String("Script") : "Script  " + EditorFileSystem::fileName(path));
    // The class as compiled holds its variables and those __init__ sets; a
    // script that did not compile is read as text for the latter.
    target->reloadIfChanged();
    ct::Vector<ZenScriptProperty> declared;
    for (std::size_t i = 0; i < target->declaredPropertyCount(); ++i)
        declared.push_back(*target->declaredPropertyAt(i));
    FileBuffer source;
    const bool read = !path.empty() && FileSystem::Instance().LoadFile(path.c_str(), source, true);
    if (declared.empty() && read)
        ScanZenScriptProperties(source.Text(), declared);
    for (const ZenScriptProperty& property : declared)
        addScriptProperty(section, type, index, property, read ? source.Text() : nullptr);
    if (declared.empty())
        section->createChild<rt::Label>(path.empty() ? "No script file" : "No variables (speed = 200 in the class)");
    auto* tools = section->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    tools->setPadding(0.0f);
    tools->setSpacing(4.0f);
    if (!path.empty())
        iconButton(tools, "ScriptFile", "Open in the Script Editor", [this, path]() { scriptOpenRequested.emit(path); });
    iconButton(tools, "Remove", "Remove this component", [this, target]() { removeComponentRequested.emit(target); });
}

void InspectorPanel::addScriptProperty(rt::Widget* parent, ComponentType type, std::size_t index,
                                       const ZenScriptProperty& declared, const char* source)
{
    const ZenScriptProperty fallback = declared;
    const PropertyType kind = fallback.kind == ZenScriptProperty::Kind::Bool     ? PropertyType::Bool
                              : fallback.kind == ZenScriptProperty::Kind::String ? PropertyType::String
                              : fallback.integer                                 ? PropertyType::Int
                                                                                 : PropertyType::Float;
    FieldBinding binding;
    binding.get = [this, type, index, fallback, kind]()
    {
        const ZenScriptComponent* s = script(type, index);
        const ZenScriptProperty* own = s ? s->findOverride(fallback.name.c_str()) : nullptr;
        const ZenScriptProperty& value = own ? *own : fallback;
        switch (kind)
        {
        case PropertyType::Bool:
            return PropertyValue::from(value.flag);
        case PropertyType::String:
            return PropertyValue::from(value.text);
        case PropertyType::Int:
            return PropertyValue::from(static_cast<int>(value.number));
        default:
            return PropertyValue::from(static_cast<float>(value.number));
        }
    };
    // The script's own value again means no value of the object's.
    binding.set = [this, type, index, fallback, kind](const PropertyValue& value)
    {
        ZenScriptComponent* s = script(type, index);
        if (!s)
            return;
        const char* name = fallback.name.c_str();
        if (kind == PropertyType::Bool)
            value.asBool() == fallback.flag ? s->clearOverride(name) : s->setBoolOverride(name, value.asBool());
        else if (kind == PropertyType::String)
            value.asString() == fallback.text ? s->clearOverride(name)
                                              : s->setStringOverride(name, value.asString().c_str());
        else
        {
            const double number = kind == PropertyType::Int ? value.asInt() : value.asFloat();
            if (static_cast<float>(number) == static_cast<float>(fallback.number))
                s->clearOverride(name);
            else
                s->setNumberOverride(name, number, kind == PropertyType::Int);
        }
    };
    const ct::String label = editName(fallback.name.c_str());
    PropertyInfo info(kind, fallback.name.c_str());
    if (source && (kind == PropertyType::Int || kind == PropertyType::Float))
        scriptRange(source, fallback.name, info);
    addPropertyRow(parent, info, binding,
                   [this, label](bool final) { edited.emit(label.c_str(), final); });
}

void InspectorPanel::addComponent(ComponentType type, std::size_t index)
{
    Component* target = component(type, index);
    if (script(type, index))
    {
        addScript(type, index);
        return;
    }
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
