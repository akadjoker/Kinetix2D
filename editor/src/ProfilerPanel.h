#pragma once

#include "LayoutWidgets.hpp"

namespace ig::retained
{
class Label;
}

namespace k2d::editor
{

class ProfileView;

// Where the frame goes: the engine's profile scopes (scene update, physics,
// rendering, scripts...) as rows with their average and peak, their share of
// the frame and the last two seconds as a graph.
class ProfilerPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(ProfilerPanel, ig::retained::BoxLayout);

    ProfilerPanel();

    // Repaints with the profiler's numbers, unless frozen.
    void tick();

private:
    bool mFrozen = false;
    ig::retained::Label* mFrame = nullptr;
    ProfileView* mView = nullptr;
};

} // namespace k2d::editor
