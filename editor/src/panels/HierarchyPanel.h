#pragma once

#include "core/EditorPanel.h"

namespace k2d
{
class GameObject;
}

namespace k2d::editor
{

class HierarchyPanel final : public EditorPanel
{
public:
    explicit HierarchyPanel(EditorApplication &application)
        : EditorPanel("Hierarchy", application)
    {
    }

private:
    void drawContents() override;
    void drawObject(GameObject &object);
};

}
