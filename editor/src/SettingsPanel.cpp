#include "SettingsPanel.h"

#include "PropertyFields.h"

#include "BasicWidgets.hpp"
#include "WidgetApp.hpp"

namespace rt = ig::retained;

namespace k2d::editor
{

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

    // Each value is applied, and a project setting written, once its edit ends.
    auto row = [this](rt::Widget* parent, const PropertyInfo& info, ct::Function<PropertyValue()> get,
                      ct::Function<void(const PropertyValue&)> set, bool project) {
        FieldBinding binding;
        binding.get = get;
        binding.set = set;
        addPropertyRow(parent, info, binding, [this, project](bool final) {
            if (final)
                changed.emit(mSettings, project);
        });
    };

    auto* scene = mFields->createChild<rt::Collapsible>("Scene");
    row(scene, PropertyInfo(PropertyType::Bool, "own gravity"), [this]() { return PropertyValue::from(mSettings.sceneGravity); },
        [this](const PropertyValue& v) { mSettings.sceneGravity = v.asBool(); }, false);
    row(scene, PropertyInfo(PropertyType::Vec2, "gravity"), [this]() { return PropertyValue::from(mSettings.sceneGravityValue); },
        [this](const PropertyValue& v) { mSettings.sceneGravityValue = v.asVec2(); }, false);

    auto* project = mFields->createChild<rt::Collapsible>("Project physics");
    row(project, PropertyInfo(PropertyType::Vec2, "gravity"), [this]() { return PropertyValue::from(mSettings.gravity); },
        [this](const PropertyValue& v) { mSettings.gravity = v.asVec2(); }, true);
    row(project, PropertyInfo::range(PropertyType::Int, "steps per s", 10.0f, 240.0f, 1.0f),
        [this]() { return PropertyValue::from(static_cast<int>(1.0f / mSettings.fixedTimeStep + 0.5f)); },
        [this](const PropertyValue& v) { mSettings.fixedTimeStep = 1.0f / static_cast<float>(v.asInt() > 0 ? v.asInt() : 60); },
        true);
    row(project, PropertyInfo::range(PropertyType::Int, "iterations", 1.0f, 32.0f, 1.0f),
        [this]() { return PropertyValue::from(mSettings.velocityIterations); },
        [this](const PropertyValue& v) { mSettings.velocityIterations = v.asInt() > 0 ? v.asInt() : 1; }, true);
    row(project, PropertyInfo(PropertyType::Bool, "tree broadphase"),
        [this]() { return PropertyValue::from(mSettings.treeBroadphase); },
        [this](const PropertyValue& v) { mSettings.treeBroadphase = v.asBool(); }, true);
    rt::WidgetApp::instance().requestLayout();
}

} // namespace k2d::editor
