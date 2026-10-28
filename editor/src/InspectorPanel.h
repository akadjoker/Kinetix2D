#pragma once

#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "ScrollWidgets.hpp"

#include <k2d/Component.h>

namespace k2d
{
class GameObject;
struct ClassProperty;
}

namespace ig::retained
{
class SpinSlider;
}

namespace k2d::editor
{

// The properties of the selected object: the transform, then one section per
// component with a row per ClassDB property. Edits go straight to the component.
class InspectorPanel : public ig::retained::ScrollView
{
public:
    IG_WIDGET_CLASS(InspectorPanel, ig::retained::ScrollView);

    InspectorPanel();

    void show(GameObject* object);

private:
    void clear();
    void addTransform();
    void addComponent(ComponentType type, std::size_t index);
    void addProperty(ig::retained::Widget* parent, ComponentType type, std::size_t index,
                     const ClassProperty& property);
    ig::retained::BoxLayout* row(ig::retained::Widget* parent, const char* name);
    ig::retained::SpinSlider* number(ig::retained::Widget* parent, const char* label, double value,
                                     double step);
    Component* component(ComponentType type, std::size_t index) const;

    ig::retained::BoxLayout* mColumn = nullptr;
    GameObject* mObject = nullptr;
};

} // namespace k2d::editor
