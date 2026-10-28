#include "InspectorPanel.h"

#include "ComboBox.hpp"
#include "SpinSlider.hpp"
#include "TextInputWidgets.hpp"
#include "WidgetApp.hpp"

#include <k2d/ClassDB.h>
#include <k2d/GameObject.h>

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

rt::SpinSlider* InspectorPanel::number(rt::Widget* parent, const char* label, double value, double step)
{
    auto* field = parent->createChild<rt::SpinSlider>();
    field->setLabel(label);
    field->setStep(step);
    field->setAllowGreater(true);
    field->setAllowLesser(true);
    field->setControlState(rt::SpinSlider::ControlState::Hide);
    field->setValueNoSignal(value);
    field->setStretch(1.0f);
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
    }
    setScrollY(0.0f);
    rt::WidgetApp::instance().requestLayout();
}

void InspectorPanel::addTransform()
{
    auto* name = row(mColumn, "Name")->createChild<rt::TextInput>(mObject->name());
    name->setStretch(1.0f);
    name->submitted.connect([this](const rt::String& text) {
        if (mObject)
            mObject->setName(text);
    });

    auto* section = mColumn->createChild<rt::Collapsible>("Transform");

    const Math::Vec2 position = mObject->position();
    rt::BoxLayout* line = row(section, "Position");
    number(line, "x", position.x, 0.01)->valueChanged.connect([this](double v) {
        Math::Vec2 p = mObject->position();
        p.x = static_cast<float>(v);
        mObject->setPosition(p);
    });
    number(line, "y", position.y, 0.01)->valueChanged.connect([this](double v) {
        Math::Vec2 p = mObject->position();
        p.y = static_cast<float>(v);
        mObject->setPosition(p);
    });

    number(row(section, "Rotation"), "", mObject->rotationDegrees(), 0.1)
        ->valueChanged.connect([this](double v) { mObject->setRotationDegrees(static_cast<float>(v)); });

    const Math::Vec2 scale = mObject->scale();
    line = row(section, "Scale");
    number(line, "x", scale.x, 0.01)->valueChanged.connect([this](double v) {
        Math::Vec2 s = mObject->scale();
        s.x = static_cast<float>(v);
        mObject->setScale(s);
    });
    number(line, "y", scale.y, 0.01)->valueChanged.connect([this](double v) {
        Math::Vec2 s = mObject->scale();
        s.y = static_cast<float>(v);
        mObject->setScale(s);
    });
}

void InspectorPanel::addComponent(ComponentType type, std::size_t index)
{
    const Component* target = component(type, index);
    const ClassInfo* info = target ? ClassDB::find(*target) : nullptr;
    if (!info)
        return;
    auto* section = mColumn->createChild<rt::Collapsible>(info->name);
    for (std::size_t p = 0; p < info->properties.size(); ++p)
        addProperty(section, type, index, info->properties[p]);
}

void InspectorPanel::addProperty(rt::Widget* parent, ComponentType type, std::size_t index,
                                 const ClassProperty& property)
{
    const Component* target = component(type, index);
    const PropertyValue value = property.get(*target);
    const PropertyInfo& info = property.info;
    const ClassProperty* bound = &property;
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
        check->toggled.connect([apply](bool on) { apply(PropertyValue::from(on)); });
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
            combo->selectionChanged.connect([apply](int i) { apply(PropertyValue::from(i)); });
            break;
        }
        [[fallthrough]];
    case PropertyType::Float:
    {
        const bool integer = info.type == PropertyType::Int;
        const double step = info.rangeStep > 0.0f ? info.rangeStep : (integer ? 1.0 : 0.01);
        auto* field = number(line, "", integer ? value.asInt() : value.asFloat(), step);
        field->setEditingInteger(integer);
        if (info.hint == PropertyHint::Range)
        {
            field->setMin(info.rangeMin);
            field->setMax(info.rangeMax);
            field->setAllowGreater(false);
            field->setAllowLesser(false);
            field->setControlState(rt::SpinSlider::ControlState::PreferSlider);
            field->setValueNoSignal(integer ? value.asInt() : value.asFloat());
        }
        field->valueChanged.connect([apply, integer](double v) {
            apply(integer ? PropertyValue::from(static_cast<int>(v)) : PropertyValue::from(static_cast<float>(v)));
        });
        break;
    }
    case PropertyType::Vec2:
    {
        const Math::Vec2 v = value.asVec2();
        number(line, "x", v.x, 0.01)->valueChanged.connect([apply, current](double x) {
            Math::Vec2 w = current().asVec2();
            w.x = static_cast<float>(x);
            apply(PropertyValue::from(w));
        });
        number(line, "y", v.y, 0.01)->valueChanged.connect([apply, current](double y) {
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
            auto* field = number(line, names[i], channels[i], 0.01);
            field->setMin(0.0);
            field->setMax(1.0);
            field->setAllowGreater(false);
            field->setAllowLesser(false);
            field->setControlState(rt::SpinSlider::ControlState::PreferSlider);
            field->setValueNoSignal(channels[i]);
            field->valueChanged.connect([apply, current, i](double x) {
                Color w = current().asColor();
                float* channel[4] = {&w.r, &w.g, &w.b, &w.a};
                *channel[i] = static_cast<float>(x);
                apply(PropertyValue::from(w));
            });
        }
        break;
    }
    case PropertyType::String:
    {
        auto* text = line->createChild<rt::TextInput>(value.asString());
        text->setStretch(1.0f);
        text->submitted.connect([apply](const rt::String& s) { apply(PropertyValue::from(s)); });
        break;
    }
    case PropertyType::ResourcePath:
    {
        // Read only until resources can be picked and loaded from here.
        auto* text = line->createChild<rt::TextInput>(value.asString(), rt::TextInput::Mode::ReadOnly);
        text->setStretch(1.0f);
        break;
    }
    case PropertyType::Nil:
        line->createChild<rt::Label>(value.toString());
        break;
    }
}

} // namespace k2d::editor
