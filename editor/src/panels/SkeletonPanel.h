#pragma once

#include "core/EditorPanel.h"

namespace k2d::editor
{

class SkeletonPanel final : public EditorPanel
{
public:
    explicit SkeletonPanel(EditorApplication& application)
        : EditorPanel("Skeleton", application, false)
    {
    }

private:
    void drawContents() override;
    int mSelectedClip = -1;
    float mTarget[2] = {0.0f, 0.0f};
    int mChainLength = 0;
    int mIterations = 8;
    float mTolerance = 0.5f;
};

} // namespace k2d::editor