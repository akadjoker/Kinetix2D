#pragma once

#include "BasicWidgets.hpp"
#include "LayoutWidgets.hpp"
#include "ScrollWidgets.hpp"
#include "Signal.hpp"

#include <k2d/Component.h>

#include <ct/function.hpp>
#include <ct/string.hpp>

namespace k2d
{
class GameObject;
class ZenScriptComponent;
struct ClassProperty;
struct ZenScriptProperty;
}

namespace ig::retained
{
class Menu;
class SpinSlider;
}

namespace k2d::editor
{

// The properties of the selected object: the transform, then one section per
// component with a row per ClassDB property. Edits go straight to the component
// and are announced through edited, so the window can record them for undo.
class InspectorPanel : public ig::retained::ScrollView
{
public:
    IG_WIDGET_CLASS(InspectorPanel, ig::retained::ScrollView);

    InspectorPanel();
    ~InspectorPanel() override;

    void show(GameObject* object);
    // The same object shown again with its values now, at the same scroll.
    void refresh();

    // An edit was made; final is false while a value is still being dragged.
    ig::retained::Signal<const char*, bool> edited;
    // Asked from a widget of this panel: the window applies them and shows the
    // object again on the next frame, after the widget's signal has returned.
    ig::retained::Signal<const char*> addComponentRequested;
    ig::retained::Signal<Component*> removeComponentRequested;
    // A script's file, a project path, to edit.
    ig::retained::Signal<const ct::String&> scriptOpenRequested;

private:
    void clear();
    void addTransform();
    void addComponent(ComponentType type, std::size_t index);
    void addComponentPicker();
    void addProperty(ig::retained::Widget* parent, ComponentType type, std::size_t index,
                     const ClassProperty& property);
    Component* component(ComponentType type, std::size_t index) const;
    ZenScriptComponent* script(ComponentType type, std::size_t index) const;
    void addScript(ComponentType type, std::size_t index);
    void addScriptProperty(ig::retained::Widget* parent, ComponentType type, std::size_t index,
                           const ZenScriptProperty& declared, const char* source);

    void buildAddMenu();

    ig::retained::BoxLayout* mColumn = nullptr;
    GameObject* mObject = nullptr;
    // Add Component's menu and its category submenus, kept across rebuilds.
    ig::retained::Menu* mAddMenu = nullptr;
    ct::Vector<ig::retained::Menu*> mAddCategories;
};

} // namespace k2d::editor
