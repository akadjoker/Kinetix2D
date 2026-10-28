#include "InspectorPanel.h"

#include "AssetsPanel.h"
#include "core/EditorFileSystem.h"

#include "ComboBox.hpp"
#include "SpinSlider.hpp"
#include "TextInputWidgets.hpp"
#include "WidgetApp.hpp"

#include <k2d/ClassDB.h>
#include <k2d/GameObject.h>
#include <k2d/Serializer.h>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{

constexpr float kLabelWidth = 96.0f;

// A label with a fixed width, so the widgets of every row start at the same x.
class FieldLabel : public rt::Label
{
public:
    IG_WIDGET_CLASS(FieldLabel, rt::Label);

    explicit FieldLabel(const rt::String& text) : rt::Label(text) {}

    Vec2f sizeHint() const override
    {
        return {kLabelWidth, rt::Label::sizeHint().y};
    }
};

// A resource path: typed and submitted, or an asset dropped from the Assets
// panel when its extension is one the property takes.
class ResourceField : public rt::TextInput
{
public:
    IG_WIDGET_CLASS(ResourceField, rt::TextInput);

    ResourceField(const rt::String& text, const ct::String& extensions)
        : rt::TextInput(text), mExtensions(extensions)
    {
    }

    ct::Function<void(const ct::String&)> picked;

    bool acceptsDrop(const rt::DragPayload& payload) override
    {
        const ct::String* path = std::any_cast<ct::String>(&payload.data);
        return payload.type == kAssetPayload && path && takes(*path);
    }

    void onDropReceive(const rt::DragPayload& payload) override
    {
        if (const ct::String* path = std::any_cast<ct::String>(&payload.data))
        {
            setText(*path);
            if (picked)
                picked(*path);
        }
    }

private:
    bool takes(const ct::String& path) const
    {
        if (mExtensions.empty())
            return true;
        const ct::String ext = EditorFileSystem::extension(path);
        std::size_t start = 0;
        while (start <= mExtensions.size())
        {
            std::size_t end = mExtensions.find(',', start);
            if (end == ct::String::npos)
                end = mExtensions.size();
            if (mExtensions.substr(start, end - start) == ext)
                return true;
            start = end + 1;
        }
        return false;
    }

    ct::String mExtensions;
};

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

rt::BoxLayout* InspectorPanel::row(rt::Widget* parent, const char* name)
{
    auto* line = parent->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    line->setPadding(0.0f);
    line->setSpacing(4.0f);
    line->createChild<FieldLabel>(name);
    return line;
}

rt::SpinSlider* InspectorPanel::number(rt::Widget* parent, const char* label, double value, double step,
                                       const ct::String& edit, ct::Function<void(double)> change)
{
    auto* field = parent->createChild<rt::SpinSlider>();
    field->setLabel(label);
    field->setStep(step);
    field->setAllowGreater(true);
    field->setAllowLesser(true);
    field->setControlState(rt::SpinSlider::ControlState::Hide);
    field->setValueNoSignal(value);
    field->setStretch(1.0f);
    // A drag is one edit: it stays open while the value moves and closes on release.
    field->valueChanged.connect([this, field, edit, change](double v) {
        change(v);
        edited.emit(edit.c_str(), !field->isGrabbing());
    });
    field->ungrabbed.connect([this, edit]() { edited.emit(edit.c_str(), true); });
    return field;
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
    auto* name = row(mColumn, "Name")->createChild<rt::TextInput>(mObject->name());
    name->setStretch(1.0f);
    name->submitted.connect([this](const rt::String& text) {
        mObject->setName(text);
        edited.emit("Rename", true);
    });

    auto* section = mColumn->createChild<rt::Collapsible>("Transform");

    const Math::Vec2 position = mObject->position();
    rt::BoxLayout* line = row(section, "Position");
    number(line, "x", position.x, 0.01, "Move", [this](double v) {
        Math::Vec2 p = mObject->position();
        p.x = static_cast<float>(v);
        mObject->setPosition(p);
    });
    number(line, "y", position.y, 0.01, "Move", [this](double v) {
        Math::Vec2 p = mObject->position();
        p.y = static_cast<float>(v);
        mObject->setPosition(p);
    });

    number(row(section, "Rotation"), "", mObject->rotationDegrees(), 0.1, "Rotate",
           [this](double v) { mObject->setRotationDegrees(static_cast<float>(v)); });

    const Math::Vec2 scale = mObject->scale();
    line = row(section, "Scale");
    number(line, "x", scale.x, 0.01, "Scale", [this](double v) {
        Math::Vec2 s = mObject->scale();
        s.x = static_cast<float>(v);
        mObject->setScale(s);
    });
    number(line, "y", scale.y, 0.01, "Scale", [this](double v) {
        Math::Vec2 s = mObject->scale();
        s.y = static_cast<float>(v);
        mObject->setScale(s);
    });
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
    auto* remove = section->createChild<rt::Button>("Remove Component");
    remove->clicked.connect([this, target]() { removeComponentRequested.emit(target); });
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
    const Component* target = component(type, index);
    const PropertyValue value = property.get(*target);
    const PropertyInfo& info = property.info;
    const ClassProperty* bound = &property;
    const ct::String edit = editName(info.name.c_str());
    rt::BoxLayout* line = row(parent, info.name.c_str());

    // Writes the value back through the ClassDB setter; the component is looked up
    // again each time, so a row never holds a pointer into the object.
    auto apply = [this, type, index, bound](const PropertyValue& v) {
        if (Component* c = component(type, index))
            bound->set(*c, v);
    };
    auto current = [this, type, index, bound]() {
        const Component* c = component(type, index);
        return c ? bound->get(*c) : PropertyValue();
    };

    switch (info.type)
    {
    case PropertyType::Bool:
    {
        auto* check = line->createChild<rt::CheckBox>();
        check->setChecked(value.asBool());
        check->toggled.connect([this, apply, edit](bool on) {
            apply(PropertyValue::from(on));
            edited.emit(edit.c_str(), true);
        });
        break;
    }
    case PropertyType::Int:
        if (info.hint == PropertyHint::Enum)
        {
            auto* combo = line->createChild<rt::ComboBox>();
            for (std::size_t i = 0; i < info.enumCount(); ++i)
                combo->addItem(info.enumName(i));
            combo->setSelectedIndex(value.asInt());
            combo->setStretch(1.0f);
            combo->selectionChanged.connect([this, apply, edit](int i) {
                apply(PropertyValue::from(i));
                edited.emit(edit.c_str(), true);
            });
            break;
        }
        [[fallthrough]];
    case PropertyType::Float:
    {
        const bool integer = info.type == PropertyType::Int;
        const double step = info.rangeStep > 0.0f ? info.rangeStep : (integer ? 1.0 : 0.01);
        const double start = integer ? value.asInt() : value.asFloat();
        auto* field = number(line, "", start, step, edit, [apply, integer](double v) {
            apply(integer ? PropertyValue::from(static_cast<int>(v)) : PropertyValue::from(static_cast<float>(v)));
        });
        field->setEditingInteger(integer);
        if (info.hint == PropertyHint::Range)
        {
            field->setMin(info.rangeMin);
            field->setMax(info.rangeMax);
            field->setAllowGreater(false);
            field->setAllowLesser(false);
            field->setControlState(rt::SpinSlider::ControlState::PreferSlider);
            field->setValueNoSignal(start);
        }
        break;
    }
    case PropertyType::Vec2:
    {
        const Math::Vec2 v = value.asVec2();
        number(line, "x", v.x, 0.01, edit, [apply, current](double x) {
            Math::Vec2 w = current().asVec2();
            w.x = static_cast<float>(x);
            apply(PropertyValue::from(w));
        });
        number(line, "y", v.y, 0.01, edit, [apply, current](double y) {
            Math::Vec2 w = current().asVec2();
            w.y = static_cast<float>(y);
            apply(PropertyValue::from(w));
        });
        break;
    }
    case PropertyType::Color:
    {
        // Four sliders until there is a swatch with a ColorPicker.
        const Color c = value.asColor();
        const float channels[4] = {c.r, c.g, c.b, c.a};
        const char* names[4] = {"r", "g", "b", "a"};
        const int count = info.hint == PropertyHint::ColorNoAlpha ? 3 : 4;
        for (int i = 0; i < count; ++i)
        {
            auto* field = number(line, names[i], channels[i], 0.01, edit, [apply, current, i](double x) {
                Color w = current().asColor();
                float* channel[4] = {&w.r, &w.g, &w.b, &w.a};
                *channel[i] = static_cast<float>(x);
                apply(PropertyValue::from(w));
            });
            field->setMin(0.0);
            field->setMax(1.0);
            field->setAllowGreater(false);
            field->setAllowLesser(false);
            field->setControlState(rt::SpinSlider::ControlState::PreferSlider);
            field->setValueNoSignal(channels[i]);
        }
        break;
    }
    case PropertyType::String:
    {
        auto* text = line->createChild<rt::TextInput>(value.asString());
        text->setStretch(1.0f);
        text->submitted.connect([this, apply, edit](const rt::String& s) {
            apply(PropertyValue::from(s));
            edited.emit(edit.c_str(), true);
        });
        break;
    }
    case PropertyType::ResourcePath:
    {
        auto* field = line->createChild<ResourceField>(value.asString(), info.hintString);
        field->setStretch(1.0f);
        field->setPlaceholder("Drop an asset here");
        field->picked = [this, apply, edit](const ct::String& path) {
            apply(PropertyValue::resourcePath(path));
            edited.emit(edit.c_str(), true);
        };
        field->submitted.connect([field](const rt::String& text) {
            if (field->picked)
                field->picked(text);
        });
        auto* clear = line->createChild<rt::Button>("x");
        clear->setTooltip("Clear");
        clear->clicked.connect([field]() {
            field->setText("");
            if (field->picked)
                field->picked("");
        });
        break;
    }
    case PropertyType::Nil:
        line->createChild<rt::Label>(value.toString());
        break;
    }
}

} // namespace k2d::editor
