#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"
#include "TreePropertyColorWidgets.hpp"

#include <ct/hashmap.hpp>

namespace k2d
{
class GameObject;
class Scene;
}

namespace k2d::editor
{

class HierarchyPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(HierarchyPanel, ig::retained::BoxLayout);

    HierarchyPanel();

    void rebuild(Scene& scene);
    GameObject* selected() const { return mSelected; }

    ig::retained::Signal<GameObject*> selectionChanged;

private:
    void addObject(ig::retained::TreeNode* parent, GameObject* object);

    ig::retained::TreeView* mTree = nullptr;
    ct::HashMap<ig::retained::TreeNode*, GameObject*> mObjects;
    GameObject* mSelected = nullptr;
};

} // namespace k2d::editor
