#include "SkeletonPanel.h"

#include "core/EditorApplication.h"

#include <k2d/Bone2D.h>
#include <k2d/GameObject.h>
#include <k2d/Skeleton2D.h>

#include <imgui.h>
#include <IconsMaterialDesignIcons.h>

#include <algorithm>
#include <cstring>

namespace k2d::editor
{
namespace
{
Skeleton2D* selectedSkeleton(EditorApplication& application, GameObject*& selected)
{
    selected = application.selection().resolve(application.scene());
    if (!selected)
        return nullptr;
    if (Skeleton2D* skeleton = selected->getComponent<Skeleton2D>())
        return skeleton;
    for (GameObject* parent = selected->parent(); parent; parent = parent->parent())
        if (Skeleton2D* skeleton = parent->getComponent<Skeleton2D>())
            return skeleton;
    return nullptr;
}
}

void SkeletonPanel::drawContents()
{
    GameObject* selected = nullptr;
    Skeleton2D* skeleton = selectedSkeleton(app(), selected);
    if (!skeleton)
    {
        ImGui::TextDisabled("Select an object with Skeleton2D or one of its bones.");
        return;
    }

    ImGui::Text("Skeleton: %s", selected->name().c_str());
    ImGui::Separator();

    ImGui::BeginChild("##skeleton_clips", ImVec2(160.0f, 0.0f), true);
    ImGui::TextUnformatted("Clips");
    for (size_t index = 0; index < skeleton->clipCount(); ++index)
    {
        BoneAnimationClip* clip = skeleton->clipAt(index);
        if (!clip)
            continue;
        const bool active = std::strcmp(skeleton->currentAnimation(), clip->name.c_str()) == 0;
        if (ImGui::Selectable(clip->name.c_str(), active))
        {
            skeleton->play(clip->name.c_str(), clip->loop);
            mSelectedClip = static_cast<int>(index);
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##skeleton_controls", ImVec2(0.0f, 0.0f), true);
    const char* current = skeleton->currentAnimation();
    BoneAnimationClip* clip = skeleton->getClip(current);
    if (!clip)
    {
        ImGui::TextDisabled("Add an animation clip to this Skeleton2D.");
        ImGui::EndChild();
        return;
    }

    if (ImGui::Button(ICON_MDI_PLAY "  Play"))
        skeleton->resume();
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_PAUSE "  Pause"))
        skeleton->pause();
    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_STOP "  Stop"))
        skeleton->stop();

    float time = skeleton->currentTime();
    if (ImGui::SliderFloat("Time", &time, 0.0f, std::max(0.001f, clip->duration), "%.3f s"))
        skeleton->seek(time);
    float speed = skeleton->speed();
    if (ImGui::DragFloat("Speed", &speed, 0.01f, -4.0f, 4.0f))
        skeleton->setSpeed(speed);

    ImGui::SeparatorText("IK Solver");
    const char* effector = selected && selected->getComponent<Bone2D>() ? selected->name().c_str() : nullptr;
    if (!effector)
        ImGui::TextDisabled("Select a Bone2D to use it as the effector.");
    else
    {
        ImGui::Text("Effector: %s", effector);
        ImGui::DragFloat2("Target", mTarget, 0.5f);
        ImGui::DragInt("Chain length", &mChainLength, 1.0f, 0, 32);
        ImGui::DragInt("Iterations", &mIterations, 1.0f, 1, 64);
        ImGui::DragFloat("Tolerance", &mTolerance, 0.05f, 0.0f, 100.0f);
        if (ImGui::Button(ICON_MDI_TARGET "  Solve IK"))
            skeleton->solveIK(effector, Math::Vec2(mTarget[0], mTarget[1]),
                               mChainLength, mIterations, mTolerance);
    }
    ImGui::EndChild();
}

} // namespace k2d::editor