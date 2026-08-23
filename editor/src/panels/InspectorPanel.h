#pragma once

#include "core/EditorPanel.h"

namespace k2d::editor
{

class InspectorPanel final : public EditorPanel
{
public:
    explicit InspectorPanel(EditorApplication &application)
        : EditorPanel("Inspector", application)
    {
    }

private:
    void drawContents() override;
};

}
