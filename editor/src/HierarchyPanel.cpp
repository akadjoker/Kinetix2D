#include "HierarchyPanel.h"

#include "Icons.h"

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
    mTree = createChild<rt::TreeView>();
    mTree->setStretch(1.0f);
    mTree->selectionChanged.connect([this](rt::TreeNode* node)
    {
        GameObject** object = node ? mObjects.find(node) : nullptr;
        mSelected = object ? *object : nullptr;
        selectionChanged.emit(mSelected);
    });
    mTree->toggleChanged.connect([this](rt::TreeNode* node, int toggle, bool on)
    {
        GameObject** object = node ? mObjects.find(node) : nullptr;
        if (!object)
            return;
        if (toggle == 0)
            (*object)->setVisible(on);
        else
            (*object)->setLocked(on);
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
    node->setIconId(Icons::get(iconFor(*object)));
    node->addToggle(Icons::get("VisibilityVisible"), Icons::get("VisibilityHidden"), object->visible(),
                    "Visible: click to hide", "Hidden: click to show");
    node->addToggle(Icons::get("Lock"), Icons::get("Unlock"), object->locked(), "Locked: click to unlock",
                    "Unlocked: click to lock");
    mObjects[node] = object;
    for (std::size_t i = 0; i < object->childCount(); ++i)
        addObject(node, object->child(i));
}

} // namespace k2d::editor
