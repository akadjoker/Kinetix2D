#pragma once

#include <imgui.h>

namespace k2d::editor
{

class EditorApplication;

class EditorPanel
{
public:
    EditorPanel(const char *title, EditorApplication &application)
        : mTitle(title), mApplication(application), mOpen(true)
    {
    }

    virtual ~EditorPanel() = default;

    const char *title() const { return mTitle; }
    bool &open() { return mOpen; }

    void draw()
    {
        if (!mOpen)
            return;
        if (ImGui::Begin(mTitle, &mOpen))
            drawContents();
        ImGui::End();
    }

protected:
    EditorApplication &app() { return mApplication; }
    virtual void drawContents() = 0;

private:
    const char *mTitle;
    EditorApplication &mApplication;
    bool mOpen;
};

}
