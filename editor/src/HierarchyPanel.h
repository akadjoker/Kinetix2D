#pragma once

#include "LayoutWidgets.hpp"
#include "Signal.hpp"
#include "ToolBar.hpp"
#include "TreePropertyColorWidgets.hpp"

#include <ct/hashmap.hpp>
#include <ct/string.hpp>

namespace k2d
{
class GameObject;
class Scene;
}

namespace k2d::editor
{

enum class HierarchyAction
{
    Create,
    Duplicate,
    MoveUp,
    MoveDown,
    Delete,
    SavePrefab
};

class HierarchyPanel : public ig::retained::BoxLayout
{
public:
    IG_WIDGET_CLASS(HierarchyPanel, ig::retained::BoxLayout);

    HierarchyPanel();

    void rebuild(Scene& scene);
    // Shows the object as selected without emitting selectionChanged.
    void select(GameObject* object);
    GameObject* selected() const { return mSelected; }

    ig::retained::Signal<GameObject*> selectionChanged;
    ig::retained::Signal<HierarchyAction> actionRequested;
    // A visibility or lock toggle changed an object; the argument names the edit.
    ig::retained::Signal<const char*> edited;
    // An object was dragged onto another: the object, the one it was dropped on
    // (nullptr below the last row), and where.
    ig::retained::Signal<GameObject*, GameObject*, ig::retained::TreeView::DropZone> dropped;
    // A script or a prefab from Assets dropped on a row (the object) or below
    // them (nullptr).
    ig::retained::Signal<const ct::String&, GameObject*> assetDropped;

private:
    void addObject(ig::retained::TreeNode* parent, GameObject* object);

    ig::retained::ToolBar* mTools = nullptr;
    ig::retained::TreeView* mTree = nullptr;
    ct::HashMap<ig::retained::TreeNode*, GameObject*> mObjects;
    ct::HashMap<GameObject*, ig::retained::TreeNode*> mNodes;
    GameObject* mSelected = nullptr;
    bool mSyncing = false;
};

} // namespace k2d::editor
