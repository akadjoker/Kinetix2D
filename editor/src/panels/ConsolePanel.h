#pragma once

#include "core/EditorPanel.h"

namespace k2d::editor
{

class ConsolePanel final : public EditorPanel
{
public:
    explicit ConsolePanel(EditorApplication &application);

private:
    void drawContents() override;
    bool mFollowOutput = true;
};

}
