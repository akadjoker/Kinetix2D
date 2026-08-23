#pragma once

#include "core/EditorPanel.h"

namespace k2d
{
class GameObject;
}

namespace k2d::editor
{

constexpr const char *kNodeDragDropPayload = "K2D_HIERARCHY_NODE";

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
    bool subtreeMatchesFilter(GameObject &object) const;
    void createNode();
    void duplicateSelected();
    void moveSelected(int direction);
    void deleteSelected();

    char mSearchFilter[128] = "";
};

}
