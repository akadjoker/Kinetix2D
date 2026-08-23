#pragma once

#include "core/EditorPanel.h"

namespace k2d::editor
{

class GamePanel final : public EditorPanel
{
public:
    explicit GamePanel(EditorApplication &application)
        : EditorPanel("Game", application)
    {
    }

private:
    void drawContents() override;
};

}
