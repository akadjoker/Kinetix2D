#include "k2d/Formation2D.h"

#include "k2d/GameObject.h"
#include "k2d/Navigation2D.h"
#include "k2d/NavigationAgent2D.h"
#include "k2d/Scene.h"
#include "k2d/Utils.h"

#include <cmath>

namespace k2d
{
namespace
{
constexpr float kDegToRad = 0.01745329251994329577f;

void collectMembers(const GameObject& from, const ct::String& tag, ct::Vector<const GameObject*>& out)
{
    if (from.tag() == tag && from.getComponent<Formation2D>())
        out.push_back(&from);
    for (std::size_t i = 0; i < from.childCount(); ++i)
        if (const GameObject* child = from.child(i))
            collectMembers(*child, tag, out);
}
} // namespace

Formation2D::Formation2D() : Component(Type, ComponentEventUpdate)
{
}

void Formation2D::setGroupTag(const char* tag)
{
    mGroupTag = tag ? tag : "";
    mGroupVersion = 0xFFFFFFFFu;
}

void Formation2D::setAnchorName(const char* name)
{
    mAnchorName = name ? name : "";
    mAnchor = nullptr;
    mGroupVersion = 0xFFFFFFFFu;
}

void Formation2D::setSpacing(float spacing)
{
    mSpacing = spacing > 0.0f ? spacing : 0.0f;
}

void Formation2D::setUpdateInterval(float seconds)
{
    mUpdateInterval = seconds > 0.0f ? seconds : 0.0f;
}

void Formation2D::resolveGroup()
{
    GameObject* object = owner();
    Scene* scene = object ? object->scene() : nullptr;
    if (!scene)
        return;
    if (mGroupVersion == scene->topologyVersion())
        return;
    mGroupVersion = scene->topologyVersion();

    mAnchor = mAnchorName.empty() ? nullptr : scene->find(mAnchorName.c_str());

    mSlot = 0;
    mMemberCount = 1;
    if (mGroupTag.empty())
        return;

    ct::Vector<const GameObject*> members;
    collectMembers(scene->root(), mGroupTag, members);
    if (members.empty())
        return;

    // Slot order is id order, which never changes while the group does not, so
    // a member keeps its place instead of swapping every time the list is
    // gathered in a different order.
    mMemberCount = (int)members.size();
    mSlot = 0;
    for (std::size_t i = 0; i < members.size(); ++i)
        if (members[i] != object && members[i]->id() < object->id())
            ++mSlot;
}

bool Formation2D::computeGoal(Math::Vec2& out) const
{
    if (!mAnchor || !owner())
        return false;

    const Math::Vec2 centre = mAnchor->globalPosition();
    const float facing = mAnchor->globalRotationDegrees() * kDegToRad;
    const Math::Vec2 look(std::cos(facing), std::sin(facing));
    const Math::Vec2 right(-look.y, look.x);
    const int count = mMemberCount > 0 ? mMemberCount : 1;

    switch (mShape)
    {
    case Shape::Surround:
    {
        // Evenly spaced around the anchor, the ring turning with it so a slot
        // stays on the same side of him rather than sliding round as he turns.
        const float step = 6.28318530718f / (float)count;
        const float angle = facing + step * (float)mSlot;
        out = centre + Math::Vec2(std::cos(angle), std::sin(angle)) * mSpacing;
        return true;
    }
    case Shape::Abreast:
    {
        const float offset = ((float)mSlot - 0.5f * (float)(count - 1)) * mSpacing;
        out = centre + right * offset;
        return true;
    }
    case Shape::Wedge:
    {
        const int row = mSlot / 2 + 1;
        const float side = (mSlot % 2) == 0 ? 1.0f : -1.0f;
        out = centre - look * (mSpacing * (float)row) + right * (mSpacing * (float)row * side);
        return true;
    }
    case Shape::SingleFile:
        out = centre - look * (mSpacing * (float)(mSlot + 1));
        return true;
    }
    return false;
}

void Formation2D::onUpdate(float deltaTime)
{
    GameObject* object = owner();
    if (!object)
        return;
    NavigationAgent2D* agent = object->getComponent<NavigationAgent2D>();
    if (!agent)
        return;

    resolveGroup();
    if (!mAnchor)
        return;

    mTimer -= deltaTime;
    if (mTimer > 0.0f)
        return;
    mTimer = mUpdateInterval;

    Math::Vec2 goal(0.0f, 0.0f);
    if (!computeGoal(goal))
        return;

    // A slot inside a wall is no use to anyone. Fall back on the anchor itself,
    // which is walkable by definition if he is standing there.
    Scene* scene = object->scene();
    if (scene && !Navigation2D::Contains(*scene, goal))
        goal = mAnchor->globalPosition();

    mGoal = goal;
    // setTargetPosition only repaths once the goal has really moved, so calling
    // it on the interval costs a compare and not a search.
    agent->setTargetPosition(goal);
}

} // namespace k2d
