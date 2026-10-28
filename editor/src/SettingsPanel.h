#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"

#include <mathc.h>

namespace k2d::editor
{

// The physics the game runs with, as the runner reads it: the project's
// settings (project.k2dproj) and the scene's own gravity when it has one.
struct PhysicsSettings
{
    Math::Vec2 gravity = Math::Vec2(0.0f, 980.0f);
    float fixedTimeStep = 1.0f / 60.0f;
    int velocityIterations = 8;
    bool treeBroadphase = true;
    bool sceneGravity = false;   // the scene overrides the project's gravity
    Math::Vec2 sceneGravityValue = Math::Vec2(0.0f, 980.0f);

    Math::Vec2 effectiveGravity() const { return sceneGravity ? sceneGravityValue : gravity; }
};

// Shows and edits PhysicsSettings. Project settings save to the project file
// as they change; the scene's gravity goes with the scene when it is saved.
class SettingsPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(SettingsPanel, ig::retained::BoxLayout);

    SettingsPanel();

    void show(const PhysicsSettings& settings);

    // project: true for a project setting, false for the scene's gravity.
    ig::retained::Signal<const PhysicsSettings&, bool> changed;

private:
    void rebuild();

    PhysicsSettings mSettings;
    ig::retained::BoxLayout* mFields = nullptr;
};

} // namespace k2d::editor
