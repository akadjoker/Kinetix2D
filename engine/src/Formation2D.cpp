#include "k2d/Formation2D.h"

#include "k2d/GameObject.h"
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

void Formation2D::onAwake()
{
    // Spread the recompute phase across the interval. A squad spawned on one
    // frame otherwise asks for its places on the same frame for as long as it
    // lives, and every repath the group needs lands in one spike.
    if (const GameObject* object = owner())
    {
        uint32_t state = ((uint32_t)object->id() * 2246822519u) | 1u;
        state = state * 1664525u + 1013904223u;
        mTimer = mUpdateInterval * (float)(state >> 8) * (1.0f / 16777216.0f);
    }
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
        // Evenly spaced around the anchor, in world angles and not his own. A
        // ring tied to his facing spins with him, and a player who turns to aim
        // sends the whole group running in circles chasing places that move
        // faster than they do.
        const float step = 6.28318530718f / (float)count;
        const float angle = step * (float)mSlot;
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

    // A slot inside a wall used to fall back on the anchor itself, which sent
    // the member to stand where the anchor already stands and left it grinding
    // against him for good. Pathing snaps an unreachable end onto the mesh, so
    // handing the slot over as it is puts the member as close to its place as
    // the map allows.
    mGoal = goal;

    // Re-sending a goal the member has already reached makes it walk the last
    // few units over and over, because arriving leaves it with no path and a
    // fresh target is what asks for another one. Two distances and not one:
    // hold and set out again on the same radius and the member paces across it
    // for ever, since settling drifts it a little either way.
    const float arrived = agent->pathDesiredDistance();
    const float leave = arrived * 3.0f;
    const float distance = DistanceSquared(object->globalPosition(), goal);
    if (mHolding)
    {
        if (distance <= leave * leave)
            return;
        mHolding = false;
    }
    else if (distance <= arrived * arrived)
    {
        mHolding = true;
        return;
    }
    agent->setTargetPosition(goal);
}

} // namespace k2d
