#pragma once

#include "core/EditorPanel.h"

namespace k2d::editor
{
class AnimatorPanel final : public EditorPanel
{
public:
    explicit AnimatorPanel(EditorApplication &application) : EditorPanel("Animator", application, false) {}

private:
    void drawContents() override;
    int mSelectedFrame = -1;
};
}
