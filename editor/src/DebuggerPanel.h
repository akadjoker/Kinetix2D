#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"

#include <ct/json.hpp>
#include <ct/string.hpp>

namespace ig::retained
{
class Label;
class ListBox;
class ToolBar;
}

namespace k2d::editor
{

// The script debugger's controls: where the game stopped, its call stack and
// the locals of the frame picked in it; Continue, Pause and the three steps.
class DebuggerPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(DebuggerPanel, ig::retained::BoxLayout);

    DebuggerPanel();

    void showIdle(const char* status);
    void showRunning();
    // A "stopped" message from the game.
    void showStopped(const ct::Json& stop);

    // "continue", "pause", "step_over", "step_in", "step_out".
    ig::retained::Signal<const char*> commandRequested;
    ig::retained::Signal<> stopRequested;
    // A frame picked in the call stack: its file and line.
    ig::retained::Signal<const ct::String&, int> frameSelected;

private:
    void showFrame(int index);
    void syncTools(bool connected, bool stopped);

    ct::Json mFrames;
    ig::retained::ToolBar* mTools = nullptr;
    int mContinueItem = -1, mPauseItem = -1, mOverItem = -1, mInItem = -1, mOutItem = -1, mStopItem = -1;
    ig::retained::Label* mStatus = nullptr;
    ig::retained::ListBox* mStack = nullptr;
    ig::retained::ListBox* mLocals = nullptr;
    bool mShowing = false;
};

} // namespace k2d::editor
