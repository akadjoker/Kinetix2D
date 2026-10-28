#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"

#include <k2d/Color.h>
#include <mathc.h>

namespace k2d::editor
{

// What the game runs with, as the runner reads it: the project's physics
// (project.k2dproj), and the scene's own gravity when it has one and its ambient.
struct SceneSettings
{
    Math::Vec2 gravity = Math::Vec2(0.0f, 980.0f);
    float fixedTimeStep = 1.0f / 60.0f;
    int velocityIterations = 8;
    bool treeBroadphase = true;
    bool sceneGravity = false;   // the scene overrides the project's gravity
    Math::Vec2 sceneGravityValue = Math::Vec2(0.0f, 980.0f);
    Color ambient;
    int displayWidth = 1280;    // the window the game opens
    int displayHeight = 720;

    Math::Vec2 effectiveGravity() const { return sceneGravity ? sceneGravityValue : gravity; }
};

// Shows and edits SceneSettings. Project settings save to the project file
// as they change; the scene's go with the scene when it is saved.
class SettingsPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(SettingsPanel, ig::retained::BoxLayout);

    SettingsPanel();

    void show(const SceneSettings& settings);

    // project: true for a project setting, false for one of the scene's.
    ig::retained::Signal<const SceneSettings&, bool> changed;

private:
    void rebuild();

    SceneSettings mSettings;
    ig::retained::BoxLayout* mFields = nullptr;
};

} // namespace k2d::editor
