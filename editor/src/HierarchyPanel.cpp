#include "HierarchyPanel.h"

#include "AssetsPanel.h"
#include "Icons.h"

#include "MenuWidgets.hpp"

#include <k2d/GameObject.h>
#include <k2d/Scene.h>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{
const char* iconFor(const GameObject& object)
{
    static const struct
    {
        ComponentType type;
        const char* icon;
    } kIcons[] = {
        {ComponentType::Camera, "Camera"},        {ComponentType::Sprite, "Sprite"},
        {ComponentType::TileMap, "TileMap"},      {ComponentType::SpriteBatch, "SpriteBatch"},
        {ComponentType::Light, "Light2D"},        {ComponentType::Occluder, "LightOccluder"},
        {ComponentType::Particle, "Particle"},    {ComponentType::AudioPlayer, "AudioPlayer"},
        {ComponentType::RigidBody, "RigidBody2D"}, {ComponentType::CharacterBody, "CharacterBody2D"},
        {ComponentType::Collider, "BoxCollider2D"}, {ComponentType::Joint, "DistanceJoint2D"},
        {ComponentType::Polygon2D, "Polygon2D"},  {ComponentType::LinePath, "Line2D"},
        {ComponentType::NinePatch, "NinePatch"},  {ComponentType::CircleShape, "CircleShape"},
        {ComponentType::RectShape, "RectShape"},  {ComponentType::CapsuleShape, "CapsuleShape"},
        {ComponentType::UiCanvas, "UiCanvas"},    {ComponentType::UiPanel, "UiPanel"},
        {ComponentType::UiLabel, "UiLabel"},      {ComponentType::UiButton, "UiButton"},
        {ComponentType::UiCheckBox, "UiCheckBox"}, {ComponentType::UiSlider, "UiSlider"},
        {ComponentType::NavigationRegion, "NavigationRegion"}, {ComponentType::NavigationAgent, "NavigationAgent"},
        {ComponentType::MotionTween, "MotionTween"}, {ComponentType::MotionStreak, "MotionStreak"},
        {ComponentType::Skeleton, "Skeleton"},    {ComponentType::Bone, "Bone"},
        {ComponentType::ParallaxLayer, "ParallaxLayer"}, {ComponentType::Steering, "Steering"},
        {ComponentType::Formation, "Formation"},  {ComponentType::PathMotion, "PathMotion"},
        {ComponentType::ActionSequence, "ActionSequence"}, {ComponentType::Animation, "Animation"},
        {ComponentType::Script, "Script"},
    };
    for (const auto& entry : kIcons)
        if (object.rawComponentCount(entry.type) > 0)
            return entry.icon;
    return "Object";
}
} // namespace

HierarchyPanel::HierarchyPanel() : rt::BoxLayout(rt::LayoutDir::Vertical)
{
    setPadding(0.0f);
    setSpacing(0.0f);

    mTools = createChild<rt::ToolBar>();
    mTools->setDraggable(false);
    const int create = mTools->addButton(Icons::get("Add"), "New object");
    const int duplicate = mTools->addButton(Icons::get("Duplicate"), "Duplicate");
    mTools->addSeparator();
    // The atlas has no arrows yet; iGUI's own are drawn in the same toolbar.
    const int up = mTools->addButton(rt::IconId::ArrowUp, "Move up");
    const int down = mTools->addButton(rt::IconId::ArrowDown, "Move down");
    mTools->addSeparator();
    const int prefab = mTools->addButton(Icons::get("Prefab"), "Save as prefab");
    const int remove = mTools->addButton(Icons::get("Remove"), "Delete");
    mTools->buttonClicked.connect([this, create, duplicate, up, down, prefab, remove](int item)
    {
        if (item == prefab)
            actionRequested.emit(HierarchyAction::SavePrefab);
        else if (item == create)
            actionRequested.emit(HierarchyAction::Create);
        else if (item == duplicate)
            actionRequested.emit(HierarchyAction::Duplicate);
        else if (item == up)
            actionRequested.emit(HierarchyAction::MoveUp);
        else if (item == down)
            actionRequested.emit(HierarchyAction::MoveDown);
        else if (item == remove)
            actionRequested.emit(HierarchyAction::Delete);
    });

    mTree = createChild<rt::TreeView>();
    mTree->setStretch(1.0f);
    mTree->setMultiSelect(true);
    mTree->selectionSetChanged.connect([this]()
    {
        if (!mSyncing)
            selectionSetChanged.emit();
    });
    mTree->selectionChanged.connect([this](rt::TreeNode* node)
    {
        GameObject** object = node ? mObjects.find(node) : nullptr;
        mSelected = object ? *object : nullptr;
        if (!mSyncing)
            selectionChanged.emit(mSelected);
    });

    mTree->setDragEnabled(true);
    mTree->nodeDropped.connect([this](rt::TreeNode* node, rt::TreeNode* target, rt::TreeView::DropZone zone)
    {
        GameObject** object = mObjects.find(node);
        GameObject** onto = target ? mObjects.find(target) : nullptr;
        if (object)
            dropped.emit(*object, onto ? *onto : nullptr, zone);
    });

    mTree->setExternalDrop([](const rt::DragPayload& payload)
    {
        const ct::String* path = assetPath(payload);
        return path && (isScriptPath(*path) || isPrefabPath(*path) || isImagePath(*path));
    });
    mTree->externalDropped.connect([this](const rt::DragPayload& payload, rt::TreeNode* node)
    {
        const ct::String* path = assetPath(payload);
        GameObject** object = node ? mObjects.find(node) : nullptr;
        if (path)
            assetDropped.emit(*path, object ? *object : nullptr);
    });

    rt::Menu* menu = new rt::Menu();
    menu->addAction("New Object", [this]() { actionRequested.emit(HierarchyAction::Create); });
    menu->addAction("Duplicate", [this]() { actionRequested.emit(HierarchyAction::Duplicate); });
    menu->addSeparator();
    menu->addAction("Move Up", [this]() { actionRequested.emit(HierarchyAction::MoveUp); });
    menu->addAction("Move Down", [this]() { actionRequested.emit(HierarchyAction::MoveDown); });
    menu->addSeparator();
    menu->addAction("Save as Prefab...", [this]() { actionRequested.emit(HierarchyAction::SavePrefab); });
    menu->addAction("Delete", [this]() { actionRequested.emit(HierarchyAction::Delete); });
    mTree->setContextMenu(menu);
    mTree->toggleChanged.connect([this](rt::TreeNode* node, int toggle, bool on)
    {
        GameObject** object = node ? mObjects.find(node) : nullptr;
        if (!object)
            return;
        if (toggle == 0)
            (*object)->setVisible(on);
        else
            (*object)->setLocked(on);
        edited.emit(toggle == 0 ? "Toggle visibility" : "Toggle lock");
    });
}

void HierarchyPanel::rebuild(Scene& scene)
{
    mSyncing = true;
    mTree->clear();
    mSyncing = false;
    mObjects.clear();
    mNodes.clear();
    mSelected = nullptr;
    GameObject& root = scene.root();
    for (std::size_t i = 0; i < root.childCount(); ++i)
        addObject(nullptr, root.child(i));
}

void HierarchyPanel::addObject(rt::TreeNode* parent, GameObject* object)
{
    const rt::String text(object->name().c_str());
    rt::TreeNode* node = parent ? parent->addChild(text) : mTree->addRoot(text);
    node->setIconId(Icons::get(iconFor(*object)));
    node->addToggle(Icons::get("VisibilityVisible"), Icons::get("VisibilityHidden"), object->visible(),
                    "Visible: click to hide", "Hidden: click to show");
    node->addToggle(Icons::get("Lock"), Icons::get("Unlock"), object->locked(), "Locked: click to unlock",
                    "Unlocked: click to lock");
    mObjects[node] = object;
    mNodes[object] = node;
    for (std::size_t i = 0; i < object->childCount(); ++i)
        addObject(node, object->child(i));
}

void HierarchyPanel::selectedObjects(ct::Vector<GameObject*>& out) const
{
    ct::Vector<rt::TreeNode*> nodes;
    mTree->selectedNodes(nodes);
    out.clear();
    for (rt::TreeNode* node : nodes)
        if (GameObject* const* object = mObjects.find(node))
            out.push_back(*object);
}

void HierarchyPanel::selectExtra(const ct::Vector<GameObject*>& objects)
{
    ct::Vector<rt::TreeNode*> nodes;
    for (GameObject* object : objects)
        if (rt::TreeNode* const* node = mNodes.find(object))
            nodes.push_back(*node);
    mTree->setExtraSelection(nodes);
}

void HierarchyPanel::select(GameObject* object)
{
    rt::TreeNode** node = object ? mNodes.find(object) : nullptr;
    mSyncing = true;
    mTree->setSelectedNode(node ? *node : nullptr);
    mSyncing = false;
    mSelected = node ? object : nullptr;
}

} // namespace k2d::editor
