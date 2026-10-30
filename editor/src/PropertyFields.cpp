#include "PropertyFields.h"

#include "AssetsPanel.h"
#include "Icons.h"
#include "core/EditorFileSystem.h"

#include "BasicWidgets.hpp"
#include "ComboBox.hpp"
#include "MenuWidgets.hpp"
#include "SpinSlider.hpp"
#include "TextInputWidgets.hpp"
#include "TreePropertyColorWidgets.hpp"

#include <k2d/CollisionLayers.h>

#include <cmath>
#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{

class FieldLabel : public rt::Label
{
public:
    IG_WIDGET_CLASS(FieldLabel, rt::Label);

    explicit FieldLabel(const rt::String& text) : rt::Label(text) {}

    Vec2f sizeHint() const override { return {kFieldLabelWidth, rt::Label::sizeHint().y}; }
};

class NumberField : public rt::SpinSlider
{
public:
    IG_WIDGET_CLASS(NumberField, rt::SpinSlider);

    float width = 0.0f;

    Vec2f sizeHint() const override
    {
        const Vec2f hint = rt::SpinSlider::sizeHint();
        if (width > 0.0f)
            return {width, hint.y};
        return {hint.x > kNumberMinWidth ? hint.x : kNumberMinWidth, hint.y};
    }
};

ResourcePreview& resourcePreview()
{
    static ResourcePreview preview;
    return preview;
}

// A resource path: typed and submitted, or an asset dropped from the Assets
// panel when its extension is one the property takes. It shows the file's name;
// hovering shows the whole path and, for an image, the image.
class ResourceField : public rt::TextInput
{
public:
    IG_WIDGET_CLASS(ResourceField, rt::TextInput);

    ResourceField(const ct::String& path, const ct::String& extensions) : rt::TextInput(""), mExtensions(extensions)
    {
        setPath(path);
    }

    ct::Function<void(const ct::String&)> picked;

    void setPath(const ct::String& path)
    {
        mPath = path;
        setText(EditorFileSystem::fileName(path));
        setTooltip(path);
        rt::TextureHandle texture;
        float width = 0.0f, height = 0.0f;
        if (path.empty() || !resourcePreview() || !resourcePreview()(path, texture, width, height))
            texture = rt::TextureHandle();
        setTooltipImage(texture, width, height);
    }

    // What was typed, or the path itself while the name it shows is untouched.
    ct::String typedPath() const
    {
        const ct::String typed = text();
        return typed == EditorFileSystem::fileName(mPath) ? mPath : typed;
    }

    bool acceptsDrop(const rt::DragPayload& payload) override
    {
        const ct::String* path = assetPath(payload);
        return path && takes(*path);
    }

    void onDropReceive(const rt::DragPayload& payload) override
    {
        if (const ct::String* path = assetPath(payload))
        {
            setPath(*path);
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
    ct::String mPath;
};

// The bits of a collider's category or mask, by the project's layer names:
// the button says which are on, its menu turns one on or off. One menu serves
// every field, so an edit that rebuilds the inspector never frees the menu
// that is calling back.
class LayersField : public rt::Button
{
public:
    IG_WIDGET_CLASS(LayersField, rt::Button);

    explicit LayersField(unsigned int bits) : rt::Button(""), mBits(bits) { sync(); }

    ct::Function<void(unsigned int)> changed;

    void onMousePress(rt::MouseEvent& e) override
    {
        if (e.button != 0)
        {
            rt::Button::onMousePress(e);
            return;
        }
        e.consumed = true;
        rt::Menu* menu = layersMenu();
        menu->clearActions();
        for (int layer = 0; layer < kCollisionLayers; ++layer)
        {
            const ct::String& name = CollisionLayerName(layer);
            char label[64];
            std::snprintf(label, sizeof label, "%2d   %s", layer + 1, name.c_str());
            rt::MenuAction* action = menu->addCheckable(label, ((mBits >> layer) & 1u) != 0);
            action->triggered.connect([this, layer]() {
                mBits ^= 1u << layer;
                sync();
                if (changed)
                    changed(mBits);
            });
        }
        const rt::Rect area = absoluteRect();
        menu->exec(area.x, area.y + area.h, this);
    }

private:
    static rt::Menu* layersMenu()
    {
        static rt::Menu* menu = new rt::Menu();
        return menu;
    }

    void sync()
    {
        const unsigned int all = (1u << kCollisionLayers) - 1u;
        ct::String text;
        if ((mBits & all) == 0)
            text = "None";
        else if ((mBits & all) == all)
            text = "All";
        else
            for (int layer = 0; layer < kCollisionLayers; ++layer)
            {
                if (((mBits >> layer) & 1u) == 0)
                    continue;
                if (!text.empty())
                    text += ", ";
                const ct::String& name = CollisionLayerName(layer);
                if (name.empty())
                {
                    char number[8];
                    std::snprintf(number, sizeof number, "%d", layer + 1);
                    text += number;
                }
                else
                    text += name;
            }
        setText(text);
        setTooltip(text);
    }

    unsigned int mBits;
};

unsigned char byte(float v)
{
    return static_cast<unsigned char>(v <= 0.0f ? 0 : (v >= 1.0f ? 255 : static_cast<int>(v * 255.0f + 0.5f)));
}

rt::Color toWidget(const Color& c)
{
    return rt::Color(byte(c.r), byte(c.g), byte(c.b), byte(c.a));
}

Color fromWidget(const rt::Color& c)
{
    return Color(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
}

} // namespace

void setResourcePreview(ResourcePreview preview)
{
    resourcePreview() = static_cast<ResourcePreview&&>(preview);
}

rt::BoxLayout* addFieldRow(rt::Widget* parent, const char* label)
{
    auto* line = parent->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    line->setPadding(0.0f);
    line->setSpacing(4.0f);
    line->createChild<FieldLabel>(label);
    return line;
}

rt::SpinSlider* addNumberField(rt::Widget* parent, const char* label, double value, const NumberSpec& spec,
                               ct::Function<void(double, bool)> changed)
{
    auto* field = parent->createChild<NumberField>();
    field->setLabel(label);
    field->setStep(spec.step);
    field->setEditingInteger(spec.integer);
    const bool bounded = spec.minimum > -1.0e8 && spec.maximum < 1.0e8;
    field->setMin(bounded ? spec.minimum : -1.0e9);
    field->setMax(bounded ? spec.maximum : 1.0e9);
    field->setAllowGreater(!bounded || spec.openAbove);
    field->setAllowLesser(!bounded || spec.openBelow);
    field->setControlState(bounded ? rt::SpinSlider::ControlState::PreferSlider : rt::SpinSlider::ControlState::Hide);
    field->setValueNoSignal(value);
    field->width = spec.width;
    field->setStretch(spec.width > 0.0f ? 0.0f : 1.0f);
    // While dragging the value moves live; the release is the edit's end.
    field->valueChanged.connect([field, changed](double v) {
        if (changed)
            changed(v, !field->isGrabbing());
    });
    field->ungrabbed.connect([field, changed]() {
        if (changed)
            changed(field->value(), true);
    });
    return field;
}

rt::BoxLayout* addPropertyRow(rt::Widget* parent, const PropertyInfo& info, const FieldBinding& binding,
                              const FieldEdited& edited)
{
    rt::BoxLayout* line = addFieldRow(parent, info.name.c_str());
    const PropertyValue value = binding.get();
    const ct::Function<void(const PropertyValue&)> set = binding.set;
    const ct::Function<PropertyValue()> get = binding.get;

    switch (info.type)
    {
    case PropertyType::Bool:
    {
        auto* check = line->createChild<rt::CheckBox>();
        check->setChecked(value.asBool());
        check->toggled.connect([set, edited](bool on) {
            set(PropertyValue::from(on));
            edited(true);
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
            combo->selectionChanged.connect([set, edited](int i) {
                set(PropertyValue::from(i));
                edited(true);
            });
            break;
        }
        if (info.hint == PropertyHint::Layers)
        {
            auto* layers = line->createChild<LayersField>(static_cast<unsigned int>(value.asInt()));
            layers->setStretch(1.0f);
            layers->changed = [set, edited](unsigned int bits) {
                set(PropertyValue::from(static_cast<int>(bits)));
                edited(true);
            };
            break;
        }
        [[fallthrough]];
    case PropertyType::Float:
    {
        const bool integer = info.type == PropertyType::Int;
        NumberSpec spec;
        spec.integer = integer;
        spec.step = info.rangeStep > 0.0f ? info.rangeStep : (integer ? 1.0 : 0.01);
        if (info.hint == PropertyHint::Range)
        {
            spec.minimum = info.rangeMin;
            spec.maximum = info.rangeMax;
            spec.openAbove = info.hintString.find("or_greater") != ct::String::npos;
            spec.openBelow = info.hintString.find("or_less") != ct::String::npos;
        }
        addNumberField(line, "", integer ? value.asInt() : value.asFloat(), spec, [set, edited, integer](double v, bool final) {
            set(integer ? PropertyValue::from(static_cast<int>(std::lround(v))) : PropertyValue::from(static_cast<float>(v)));
            edited(final);
        });
        break;
    }
    case PropertyType::Vec2:
    {
        const Math::Vec2 v = value.asVec2();
        NumberSpec spec;
        if (info.hint == PropertyHint::Range)
        {
            spec.minimum = info.rangeMin;
            spec.maximum = info.rangeMax;
            spec.step = info.rangeStep > 0.0f ? info.rangeStep : 0.01;
            spec.openAbove = info.hintString.find("or_greater") != ct::String::npos;
            spec.openBelow = info.hintString.find("or_less") != ct::String::npos;
        }
        addNumberField(line, "x", v.x, spec, [get, set, edited](double x, bool final) {
            Math::Vec2 w = get().asVec2();
            w.x = static_cast<float>(x);
            set(PropertyValue::from(w));
            edited(final);
        });
        addNumberField(line, "y", v.y, spec, [get, set, edited](double y, bool final) {
            Math::Vec2 w = get().asVec2();
            w.y = static_cast<float>(y);
            set(PropertyValue::from(w));
            edited(final);
        });
        break;
    }
    case PropertyType::Color:
    {
        auto* swatch = line->createChild<rt::ColorButton>(toWidget(value.asColor()));
        swatch->setShowAlpha(info.hint != PropertyHint::ColorNoAlpha);
        swatch->setStretch(1.0f);
        swatch->colorChanged.connect([set, edited](rt::Color c) {
            set(PropertyValue::from(fromWidget(c)));
            edited(false);
        });
        swatch->colorCommitted.connect([edited](rt::Color) { edited(true); });
        break;
    }
    case PropertyType::String:
    {
        auto* text = line->createChild<rt::TextInput>(value.asString());
        text->setStretch(1.0f);
        text->submitted.connect([set, edited](const rt::String& s) {
            set(PropertyValue::from(s));
            edited(true);
        });
        break;
    }
    case PropertyType::ResourcePath:
    {
        auto* field = line->createChild<ResourceField>(value.asString(), info.hintString);
        field->setStretch(1.0f);
        field->setPlaceholder("Drop an asset here");
        field->picked = [set, edited](const ct::String& path) {
            set(PropertyValue::resourcePath(path));
            edited(true);
        };
        field->submitted.connect([field](const rt::String&) {
            const ct::String path = field->typedPath();
            field->setPath(path);
            if (field->picked)
                field->picked(path);
        });
        iconButton(line, rt::IconId::Cross, "Clear", [field]() {
            field->setPath("");
            if (field->picked)
                field->picked("");
        });
        break;
    }
    case PropertyType::Nil:
        line->createChild<rt::Label>(value.toString());
        break;
    }
    return line;
}

} // namespace k2d::editor
