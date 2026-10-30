#pragma once

#include "LayoutWidgets.hpp"

#include <k2d/ClassDB.h>
#include <k2d/PropertyValue.h>

#include <ct/function.hpp>
#include <ct/string.hpp>

namespace ig::retained
{
class SpinSlider;
class Widget;
}

namespace k2d::editor
{

// The fields every panel of the editor builds its rows from, so a number, a
// vector, a colour or a resource looks and behaves the same everywhere: the
// same label column, the same widths, and one notice per edit whose final
// flag is false while a drag goes on (a drag is one undo step).

// The label column of every row.
constexpr float kFieldLabelWidth = 104.0f;
// No number field gets narrower than this, so its label and value both show.
constexpr float kNumberMinWidth = 72.0f;

// Called after each edit made through a field.
using FieldEdited = ct::Function<void(bool final)>;

// How a field reaches its value: read when the row is built, written on edit.
struct FieldBinding
{
    ct::Function<PropertyValue()> get;
    ct::Function<void(const PropertyValue&)> set;
};

// A row with a label and the field for info's type, under parent.
ig::retained::BoxLayout* addPropertyRow(ig::retained::Widget* parent, const PropertyInfo& info,
                                        const FieldBinding& binding, const FieldEdited& edited);

// What a resource field shows when hovered: the texture a project path names,
// with its size; false when the path is not a loaded image.
using ResourcePreview = ct::Function<bool(const ct::String& path, ig::retained::TextureHandle& texture, float& width,
                                          float& height)>;
void setResourcePreview(ResourcePreview preview);

// A row with only its label, for fields put in by hand.
ig::retained::BoxLayout* addFieldRow(ig::retained::Widget* parent, const char* label);

struct NumberSpec
{
    double step = 0.01;
    bool integer = false;
    // A slider is shown when the range is bounded on both sides; open ends
    // let a typed or dragged value go past them (Godot's or_greater, or_less).
    double minimum = -1.0e9;
    double maximum = 1.0e9;
    bool openAbove = false;
    bool openBelow = false;
    // 0: the field stretches in its row; otherwise its width, for tool bars.
    float width = 0.0f;
};

// A number field that stretches in its row; changed gets the value and the
// final flag.
ig::retained::SpinSlider* addNumberField(ig::retained::Widget* parent, const char* label, double value,
                                         const NumberSpec& spec, ct::Function<void(double, bool)> changed);

} // namespace k2d::editor
