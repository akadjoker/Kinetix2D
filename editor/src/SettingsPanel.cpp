#include "SettingsPanel.h"

#include "BasicWidgets.hpp"
#include "SpinSlider.hpp"
#include "WidgetApp.hpp"

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

    Vec2f sizeHint() const override { return {110.0f, rt::Label::sizeHint().y}; }
};

rt::BoxLayout* row(rt::Widget* parent, const char* name)
{
    auto* line = parent->createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    line->setPadding(0.0f);
    line->setSpacing(4.0f);
    line->createChild<FieldLabel>(name);
    return line;
}

rt::SpinSlider* number(rt::Widget* parent, const char* label, double value, double step, bool integer)
{
    auto* field = parent->createChild<rt::SpinSlider>();
    field->setLabel(label);
    field->setStep(step);
    field->setMin(-100000.0);
    field->setMax(100000.0);
    field->setEditingInteger(integer);
    field->setControlState(rt::SpinSlider::ControlState::Hide);
    field->setValueNoSignal(value);
    field->setStretch(1.0f);
    // One change per release while dragging.
    field->setDeferredDragMode(true);
    return field;
}

} // namespace

SettingsPanel::SettingsPanel() : rt::BoxLayout(rt::LayoutDir::Vertical)
{
    setPadding(6.0f);
    setSpacing(4.0f);
    mFields = createChild<rt::BoxLayout>(rt::LayoutDir::Vertical);
    mFields->setPadding(0.0f);
    mFields->setSpacing(4.0f);
    rebuild();
}

void SettingsPanel::show(const PhysicsSettings& settings)
{
    mSettings = settings;
    rebuild();
}

void SettingsPanel::rebuild()
{
    while (!mFields->children().empty())
        mFields->removeChild(mFields->children().back());

    auto* scene = mFields->createChild<rt::Collapsible>("Scene");
    {
        rt::BoxLayout* own = row(scene, "own gravity");
        auto* check = own->createChild<rt::CheckBox>();
        check->setChecked(mSettings.sceneGravity);
        check->setTooltip("The scene's gravity instead of the project's");
        check->toggled.connect([this](bool on) {
            mSettings.sceneGravity = on;
            changed.emit(mSettings, false);
        });
        rt::BoxLayout* gravity = row(scene, "gravity");
        number(gravity, "x", mSettings.sceneGravityValue.x, 1.0, false)->valueChanged.connect([this](double v) {
            mSettings.sceneGravityValue.x = static_cast<float>(v);
            changed.emit(mSettings, false);
        });
        number(gravity, "y", mSettings.sceneGravityValue.y, 1.0, false)->valueChanged.connect([this](double v) {
            mSettings.sceneGravityValue.y = static_cast<float>(v);
            changed.emit(mSettings, false);
        });
    }

    auto* project = mFields->createChild<rt::Collapsible>("Project physics");
    {
        rt::BoxLayout* gravity = row(project, "gravity");
        number(gravity, "x", mSettings.gravity.x, 1.0, false)->valueChanged.connect([this](double v) {
            mSettings.gravity.x = static_cast<float>(v);
            changed.emit(mSettings, true);
        });
        number(gravity, "y", mSettings.gravity.y, 1.0, false)->valueChanged.connect([this](double v) {
            mSettings.gravity.y = static_cast<float>(v);
            changed.emit(mSettings, true);
        });
        number(row(project, "steps per s"), "", 1.0 / mSettings.fixedTimeStep, 1.0, true)
            ->valueChanged.connect([this](double v) {
                mSettings.fixedTimeStep = v >= 1.0 ? static_cast<float>(1.0 / v) : 1.0f / 60.0f;
                changed.emit(mSettings, true);
            });
        number(row(project, "iterations"), "", mSettings.velocityIterations, 1.0, true)
            ->valueChanged.connect([this](double v) {
                mSettings.velocityIterations = v >= 1.0 ? static_cast<int>(v) : 1;
                changed.emit(mSettings, true);
            });
        rt::BoxLayout* tree = row(project, "tree broadphase");
        auto* check = tree->createChild<rt::CheckBox>();
        check->setChecked(mSettings.treeBroadphase);
        check->toggled.connect([this](bool on) {
            mSettings.treeBroadphase = on;
            changed.emit(mSettings, true);
        });
    }
    rt::WidgetApp::instance().requestLayout();
}

} // namespace k2d::editor
