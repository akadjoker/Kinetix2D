#pragma once

#include "core/EditorPanel.h"

#include <cstddef>

namespace k2d
{
class Animation2D;
struct AnimationClip;
}

namespace k2d::editor
{
class AnimatorPanel final : public EditorPanel
{
public:
    explicit AnimatorPanel(EditorApplication &application) : EditorPanel("Animator", application, false) {}

private:
    void drawContents() override;
    void drawFrameEventMarker(Animation2D &animation, AnimationClip &clip, std::size_t frameIndex);

    int mSelectedFrame = -1;
    int mSelectedPoint = -1;
    bool mDraggingPoint = false;
};
}
