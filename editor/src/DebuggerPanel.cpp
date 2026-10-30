#include "DebuggerPanel.h"

#include "Icons.h"

#include "BasicWidgets.hpp"
#include "ScrollWidgets.hpp"
#include "ToolBar.hpp"

#include <cstdio>

namespace rt = ig::retained;

namespace k2d::editor
{

DebuggerPanel::DebuggerPanel() : rt::BoxLayout(rt::LayoutDir::Vertical)
{
    setPadding(0.0f);
    setSpacing(0.0f);
    auto* bar = createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    bar->setPadding(2.0f, 4.0f);
    bar->setSpacing(8.0f);
    mTools = bar->createChild<rt::ToolBar>();
    mTools->setDraggable(false);
    mContinueItem = mTools->addButton(Icons::get("Play"), "Continue (F9)");
    mPauseItem = mTools->addButton(Icons::get("Pause"), "Pause: stop at the next line a script runs");
    mOverItem = mTools->addButton(rt::IconId::StepForward, "Step over (F10): the next line here");
    mInItem = mTools->addButton(rt::IconId::ArrowDown, "Step in (F11): into the function this line calls");
    mOutItem = mTools->addButton(rt::IconId::ArrowUp, "Step out (Shift+F11): back to the caller");
    mStopItem = mTools->addButton(Icons::get("Stop"), "Stop: close the game");
    mTools->buttonClicked.connect([this](int item)
    {
        if (item == mContinueItem)
            commandRequested.emit("continue");
        else if (item == mPauseItem)
            commandRequested.emit("pause");
        else if (item == mOverItem)
            commandRequested.emit("step_over");
        else if (item == mInItem)
            commandRequested.emit("step_in");
        else if (item == mOutItem)
            commandRequested.emit("step_out");
        else if (item == mStopItem)
            stopRequested.emit();
    });
    mStatus = bar->createChild<rt::Label>("");
    mStatus->setStretch(1.0f);
    mStatus->setLayoutAlign(rt::Align::Center);

    auto* columns = createChild<rt::BoxLayout>(rt::LayoutDir::Horizontal);
    columns->setStretch(1.0f);
    columns->setPadding(4.0f);
    columns->setSpacing(6.0f);
    mStack = columns->createChild<rt::ListBox>();
    mStack->setStretch(1.0f);
    mStack->selectionChanged.connect([this](int index)
    {
        if (!mShowing)
            showFrame(index);
    });
    mLocals = columns->createChild<rt::ListBox>();
    mLocals->setStretch(2.0f);
    showIdle("Debug in a window (F9) runs the saved scene in the game with the debugger; click a script's gutter "
             "for a breakpoint");
}

void DebuggerPanel::syncTools(bool connected, bool stopped)
{
    mTools->setItemEnabled(mContinueItem, stopped);
    mTools->setItemEnabled(mPauseItem, connected && !stopped);
    mTools->setItemEnabled(mOverItem, stopped);
    mTools->setItemEnabled(mInItem, stopped);
    mTools->setItemEnabled(mOutItem, stopped);
    mTools->setItemEnabled(mStopItem, connected);
}

void DebuggerPanel::showIdle(const char* status)
{
    mFrames = ct::Json();
    mStack->clearItems();
    mLocals->clearItems();
    mStatus->setText(status);
    syncTools(false, false);
}

void DebuggerPanel::showRunning()
{
    mStatus->setText("Running");
    mFrames = ct::Json();
    mStack->clearItems();
    mLocals->clearItems();
    syncTools(true, false);
}

void DebuggerPanel::showStopped(const ct::Json& stop)
{
    mFrames = stop["frames"];
    mShowing = true;
    mStack->clearItems();
    for (std::size_t i = 0; mFrames.is_array() && i < mFrames.size(); ++i)
    {
        const ct::Json& frame = mFrames[i];
        const ct::String file = frame["file"].as_cstr("");
        const std::size_t slash = file.rfind('/');
        char text[256];
        std::snprintf(text, sizeof text, "%s   %s:%d", frame["function"].as_cstr("?"),
                      slash == ct::String::npos ? file.c_str() : file.c_str() + slash + 1,
                      static_cast<int>(frame["line"].as_int(0)));
        mStack->addItem(text);
    }
    mShowing = false;
    char status[128];
    std::snprintf(status, sizeof status, "Stopped: %s", stop["reason"].as_cstr(""));
    mStatus->setText(status);
    syncTools(true, true);
    mStack->setSelectedIndex(mFrames.size() > 0 ? 0 : -1);
    showFrame(0);
}

void DebuggerPanel::showFrame(int index)
{
    mLocals->clearItems();
    if (!mFrames.is_array() || index < 0 || index >= static_cast<int>(mFrames.size()))
        return;
    const ct::Json& frame = mFrames[static_cast<std::size_t>(index)];
    const ct::Json& locals = frame["locals"];
    for (std::size_t i = 0; locals.is_array() && i < locals.size(); ++i)
    {
        ct::String line = locals[i]["name"].as_cstr("");
        line += " = ";
        line += locals[i]["value"].as_cstr("");
        mLocals->addItem(line);
    }
    if (!locals.is_array() || locals.size() == 0)
        mLocals->addItem("(no locals)");
    frameSelected.emit(ct::String(frame["file"].as_cstr("")), static_cast<int>(frame["line"].as_int(0)));
}

} // namespace k2d::editor
