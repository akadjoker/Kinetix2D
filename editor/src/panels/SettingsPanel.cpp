#include "SettingsPanel.h"

#include "core/EditorApplication.h"

#include <k2d/PhysicsWorld2D.h>

#include <IconsMaterialDesignIcons.h>

namespace k2d::editor
{

void SettingsPanel::persist()
{
    app().applyPhysicsSettings();
    if (app().project().valid())
        app().project().save();
}

void SettingsPanel::drawPhysics()
{
    if (!ImGui::CollapsingHeader("Physics 2D", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    PhysicsSettings &physics = app().project().physics();

    float gravity[2] = {physics.gravity.x, physics.gravity.y};
    if (ImGui::DragFloat2("Gravity", gravity, 5.0f))
        physics.gravity = Math::Vec2(gravity[0], gravity[1]);
    if (ImGui::IsItemDeactivatedAfterEdit())
        persist();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Positive Y pulls down, the same way screen coordinates run");

    if (ImGui::Button("Earth"))
    {
        physics.gravity = Math::Vec2(0.0f, 980.0f);
        persist();
    }
    ImGui::SameLine();
    if (ImGui::Button("Zero G"))
    {
        physics.gravity = Math::Vec2(0.0f, 0.0f);
        persist();
    }
    ImGui::SameLine();
    if (ImGui::Button("Moon"))
    {
        physics.gravity = Math::Vec2(0.0f, 162.0f);
        persist();
    }
    ImGui::SameLine();
    if (ImGui::Button("Flip"))
    {
        physics.gravity = Math::Vec2(-physics.gravity.x, -physics.gravity.y);
        persist();
    }

    ImGui::Separator();

    bool fixedStep = physics.fixedTimeStep > 0.0f;
    if (ImGui::Checkbox("Fixed Time Step", &fixedStep))
    {
        physics.fixedTimeStep = fixedStep ? 1.0f / 60.0f : 0.0f;
        persist();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Off means the simulation advances by the frame time, which is not "
                          "deterministic");

    if (fixedStep)
    {
        int rate = physics.fixedTimeStep > 0.0f
                       ? static_cast<int>(1.0f / physics.fixedTimeStep + 0.5f)
                       : 60;
        if (ImGui::DragInt("Steps per Second", &rate, 1.0f, 15, 240))
            physics.fixedTimeStep = 1.0f / static_cast<float>(rate < 1 ? 1 : rate);
        if (ImGui::IsItemDeactivatedAfterEdit())
            persist();
        ImGui::TextDisabled("%.4f s per step, up to 8 steps caught up per frame",
                            physics.fixedTimeStep);
    }

    if (ImGui::DragInt("Velocity Iterations", &physics.velocityIterations, 1.0f, 1, 32))
    {
    }
    if (ImGui::IsItemDeactivatedAfterEdit())
        persist();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("More iterations means stiffer stacks and a slower step");

    if (ImGui::Checkbox("Tree Broadphase", &physics.treeBroadphase))
        persist();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Off falls back to brute force, useful to compare results");

    ImGui::Separator();

    if (PhysicsWorld2D *world = app().physicsWorld())
    {
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 0.4f, 1.0f), ICON_MDI_PLAY " Simulating");
        ImGui::Text("%d bodies, %d contacts", static_cast<int>(world->bodyCount()),
                    static_cast<int>(world->contactCount()));
        ImGui::TextDisabled("Changes above apply to the running simulation immediately.");
    }
    else
    {
        ImGui::TextDisabled("Physics runs in Play only. Press Play to simulate.");
    }

    if (!app().project().valid())
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f),
                           ICON_MDI_ALERT " No project open, these values are not saved");
}

void SettingsPanel::drawViewport()
{
    if (!ImGui::CollapsingHeader("Viewport", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (ImGui::Checkbox("Show Colliders", &app().settings().showColliders))
        app().log(app().settings().showColliders ? "Collider overlay on" : "Collider overlay off");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Draw physics shapes over the Scene view");

    ImGui::Checkbox("Live Preview", &app().settings().viewportLivePreview);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Tick particles and animations while editing");

    ImGui::Checkbox("Script Hot Reload", &app().settings().scriptHotReload);
}

void SettingsPanel::drawContents()
{
    drawPhysics();
    drawViewport();
}

}
