#include "HierarchyPanel.h"

#include <k2d/GameObject.h>
#include <k2d/Scene.h>

namespace rt = ig::retained;

namespace k2d::editor
{

HierarchyPanel::HierarchyPanel() : rt::BoxLayout(rt::LayoutDir::Vertical)
{
    setPadding(0.0f);
    setSpacing(0.0f);
    mTree = createChild<rt::TreeView>();
    mTree->setStretch(1.0f);
    mTree->selectionChanged.connect([this](rt::TreeNode* node)
    {
        GameObject** object = node ? mObjects.find(node) : nullptr;
        mSelected = object ? *object : nullptr;
        selectionChanged.emit(mSelected);
    });
}

void HierarchyPanel::rebuild(Scene& scene)
{
    mTree->clear();
    mObjects.clear();
    mSelected = nullptr;
    GameObject& root = scene.root();
    for (std::size_t i = 0; i < root.childCount(); ++i)
        addObject(nullptr, root.child(i));
}

void HierarchyPanel::addObject(rt::TreeNode* parent, GameObject* object)
{
    const rt::String text(object->name().c_str());
    rt::TreeNode* node = parent ? parent->addChild(text) : mTree->addRoot(text);
    mObjects[node] = object;
    for (std::size_t i = 0; i < object->childCount(); ++i)
        addObject(node, object->child(i));
}

} // namespace k2d::editor
