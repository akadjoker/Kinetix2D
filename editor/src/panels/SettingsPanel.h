#pragma once

#include "core/EditorPanel.h"

namespace k2d::editor
{

class SettingsPanel : public EditorPanel
{
public:
    explicit SettingsPanel(EditorApplication &application) : EditorPanel("Settings", application) {}

protected:
    void drawContents() override;

private:
    void drawPhysics();
    void drawViewport();

    void persist();
};

}
