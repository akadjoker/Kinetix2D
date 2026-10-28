#include "SceneGizmo.h"

#include "SceneShapes.h"
#include "Theme.hpp"

#include <k2d/CameraComponent.h>
#include <k2d/Collider2D.h>
#include <k2d/GameObject.h>
#include <k2d/GameViewport.h>
#include <k2d/Bone2D.h>
#include <k2d/ClassDB.h>
#include <k2d/Joint2D.h>
#include <k2d/Light2D.h>
#include <k2d/NavigationRegion2D.h>
#include <k2d/SpriteBatch.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Scene.h>
#include <k2d/Skeleton2D.h>

#include <cfloat>
#include <cstdio>
#include <cmath>

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{

constexpr float kPickRadius = 16.0f;
constexpr float kAxisLength = 60.0f;
constexpr float kHitRadius = 6.0f;
constexpr float kCenterRadius = 7.0f;
constexpr float kRingRadius = 50.0f;
constexpr float kPointHalfSize = 4.5f;
constexpr float kPointHitRadius = 8.0f;
constexpr float kGridStep = 64.0f;
constexpr float kSnapStep = 16.0f;
constexpr float kSnapDegrees = 15.0f;
constexpr float kRadToDeg = 57.29577951f;

const rt::Color kAxisXColor(230, 70, 70, 255);
const rt::Color kAxisYColor(80, 210, 100, 255);
const rt::Color kActiveColor(255, 235, 120, 255);
const rt::Color kRingColor(120, 170, 230, 255);
const rt::Color kSelectedColor(255, 205, 65, 255);
const rt::Color kOutlineColor(255, 205, 65, 150);
const rt::Color kSolidColor(110, 225, 140, 220);
const rt::Color kSensorColor(90, 180, 255, 220);
const rt::Color kNoBodyColor(240, 150, 60, 200);
const rt::Color kLightColor(255, 220, 120, 140);
const rt::Color kBoneColor(205, 215, 235, 210);

float distanceToSegment(float px, float py, float ax, float ay, float bx, float by, float* along = nullptr)
{
    const float dx = bx - ax, dy = by - ay;
    const float length = dx * dx + dy * dy;
    float t = length > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / length : 0.0f;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    if (along)
        *along = t;
    const float cx = ax + t * dx - px, cy = ay + t * dy - py;
    return std::sqrt(cx * cx + cy * cy);
}

float cross(const Math::Vec2& o, const Math::Vec2& a, const Math::Vec2& b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

float snapped(float value, float step)
{
    return std::floor(value / step + 0.5f) * step;
}

bool pickable(const GameObject& object)
{
    return object.visible() && !object.locked();
}

} // namespace

void SceneGizmo::setView(const Camera2D& camera, float width, float height)
{
    mCamera = camera;
    mWidth = width > 1.0f ? width : 1.0f;
    mHeight = height > 1.0f ? height : 1.0f;
    mView = mCamera.CameraXform(mWidth, mHeight).AffineInverse();
}

Math::Vec2 SceneGizmo::toScreen(const Math::Vec2& world) const
{
    return mView.Transform(world);
}

Math::Vec2 SceneGizmo::toWorld(float x, float y) const
{
    return mCamera.ScreenToWorld(x, y, mWidth, mHeight);
}

void SceneGizmo::refresh(Scene& scene)
{
    if (mScene == &scene && mTopology == scene.topologyVersion())
        return;
    mScene = &scene;
    mTopology = scene.topologyVersion();
    mEntries.clear();
    mColliderEntries.clear();
    mBoneEntries.clear();
    mJointEntries.clear();
    mRegionEntries.clear();
    GameObject& root = scene.root();
    for (std::size_t i = 0; i < root.childCount(); ++i)
        mEntries.push_back(Entry{root.child(i), -1});
    for (std::size_t i = 0; i < mEntries.size(); ++i)
    {
        GameObject* object = mEntries[i].object;
        for (std::size_t c = 0; c < object->childCount(); ++c)
            mEntries.push_back(Entry{object->child(c), static_cast<int>(i)});
        const std::size_t colliders = object->componentCount<Collider2D>();
        for (std::size_t c = 0; c < colliders; ++c)
        {
            Collider2D* collider = object->getComponentAt<Collider2D>(c);
            if (collider && colliderPointCount(*collider) > 0)
                mColliderEntries.push_back(ColliderEntry{collider, static_cast<int>(i)});
        }
        if (Bone2D* bone = object->getComponent<Bone2D>())
            mBoneEntries.push_back(BoneEntry{bone, static_cast<int>(i)});
        const std::size_t joints = object->componentCount<Joint2D>();
        for (std::size_t c = 0; c < joints; ++c)
            if (Joint2D* joint = object->getComponentAt<Joint2D>(c))
            {
                const ClassInfo* info = ClassDB::find(*joint);
                mJointEntries.push_back(JointEntry{joint, static_cast<int>(i),
                                                   info ? info->find("localAnchorA") : nullptr,
                                                   info ? info->find("localAnchorB") : nullptr,
                                                   info ? info->find("anchorsConfigured") : nullptr, ct::String(),
                                                   nullptr});
            }
        if (NavigationRegion2D* region = object->getComponent<NavigationRegion2D>())
            mRegionEntries.push_back(RegionEntry{region, static_cast<int>(i)});
    }
}

GameObject* SceneGizmo::findTarget(JointEntry& entry)
{
    if (entry.targetName != entry.joint->targetName())
    {
        entry.targetName = entry.joint->targetName();
        entry.target = nullptr;
        for (const Entry& candidate : mEntries)
            if (candidate.object->name() == entry.targetName)
            {
                entry.target = candidate.object;
                break;
            }
    }
    return entry.target;
}

// Both ends in the view's points: anchors in their bodies when the joint has
// them, the objects' origins otherwise.
bool SceneGizmo::jointEnds(JointEntry& entry, Math::Vec2& a, Math::Vec2& b)
{
    GameObject* owner = entry.joint->owner();
    GameObject* target = findTarget(entry);
    if (!owner || !target)
        return false;
    const Math::Vec2 anchorA = entry.anchorA ? entry.anchorA->get(*entry.joint).asVec2() : Math::Vec2(0.0f, 0.0f);
    const Math::Vec2 anchorB = entry.anchorB ? entry.anchorB->get(*entry.joint).asVec2() : Math::Vec2(0.0f, 0.0f);
    a = toScreen(bodyFrame(*owner).Transform(anchorA));
    b = toScreen(bodyFrame(*target).Transform(anchorB));
    return true;
}

// The bone from its joint to its tip, in the view's points.
bool SceneGizmo::boneEnds(const Bone2D& bone, Math::Vec2& from, Math::Vec2& to) const
{
    const GameObject* object = bone.owner();
    if (!object || bone.length() <= 0.0f)
        return false;
    from = toScreen(object->globalPosition());
    to = toScreen(object->globalTransform().Transform(bone.length(), 0.0f));
    return true;
}

void SceneGizmo::updateShown()
{
    mShown.resize(mEntries.size());
    for (std::size_t i = 0; i < mEntries.size(); ++i)
    {
        const Entry& entry = mEntries[i];
        const bool parentShown = entry.parent < 0 || mShown[static_cast<std::size_t>(entry.parent)];
        mShown[i] = parentShown && entry.object->visible() ? 1 : 0;
    }
}

GameObject* SceneGizmo::pick(Scene& scene, float x, float y)
{
    refresh(scene);
    updateShown();
    GameObject* best = nullptr;
    float bestDistance = kPickRadius;
    for (std::size_t i = 0; i < mEntries.size(); ++i)
    {
        GameObject* object = mEntries[i].object;
        if (!mShown[i] || !pickable(*object))
            continue;
        const Math::Vec2 point = toScreen(object->globalPosition());
        const float distance = std::sqrt((point.x - x) * (point.x - x) + (point.y - y) * (point.y - y));
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = object;
        }
    }
    if (best)
        return best;

    float bestBone = kHitRadius;
    for (const BoneEntry& entry : mBoneEntries)
    {
        GameObject* object = mEntries[static_cast<std::size_t>(entry.entry)].object;
        Math::Vec2 a, b;
        if (!mShown[static_cast<std::size_t>(entry.entry)] || !pickable(*object) || !boneEnds(*entry.bone, a, b))
            continue;
        const float distance = distanceToSegment(x, y, a.x, a.y, b.x, b.y);
        if (distance <= bestBone)
        {
            bestBone = distance;
            best = object;
        }
    }
    if (best)
        return best;

    const Math::Vec2 p(x, y);
    float bestArea = FLT_MAX;
    for (std::size_t i = 0; i < mEntries.size(); ++i)
    {
        GameObject* object = mEntries[i].object;
        Math::Vec4 r;
        if (!mShown[i] || !pickable(*object) || !objectBounds(*object, r))
            continue;
        const Matrix2D& xform = object->globalTransform();
        const Math::Vec2 q[4] = {toScreen(xform.Transform(r.x, r.y)), toScreen(xform.Transform(r.x + r.z, r.y)),
                                 toScreen(xform.Transform(r.x + r.z, r.y + r.w)),
                                 toScreen(xform.Transform(r.x, r.y + r.w))};
        bool inside = true, near = false;
        const float winding = cross(q[0], q[1], q[2]) < 0.0f ? -1.0f : 1.0f;
        for (int k = 0; k < 4; ++k)
        {
            const Math::Vec2& a = q[k];
            const Math::Vec2& b = q[(k + 1) % 4];
            inside = inside && cross(a, b, p) * winding >= 0.0f;
            near = near || distanceToSegment(x, y, a.x, a.y, b.x, b.y) <= kHitRadius;
        }
        if (!inside && !near)
            continue;
        const float area = std::fabs(cross(q[0], q[1], q[3]));
        if (area < bestArea)
        {
            bestArea = area;
            best = object;
        }
    }
    return best;
}

SceneGizmo::Handle SceneGizmo::hit(GameObject& selected, float x, float y) const
{
    const Math::Vec2 c = toScreen(selected.globalPosition());
    const float dx = x - c.x, dy = y - c.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (mTool == SceneTool::Move || mTool == SceneTool::Scale)
    {
        if (distance <= kCenterRadius)
            return Handle::Free;
        if (distanceToSegment(x, y, c.x, c.y, c.x + kAxisLength, c.y) <= kHitRadius)
            return Handle::AxisX;
        if (distanceToSegment(x, y, c.x, c.y, c.x, c.y + kAxisLength) <= kHitRadius)
            return Handle::AxisY;
    }
    else if (mTool == SceneTool::Rotate)
    {
        if (std::fabs(distance - kRingRadius) <= kHitRadius)
            return Handle::Ring;
    }
    return Handle::None;
}

bool SceneGizmo::hitPoint(GameObject& selected, float x, float y, Collider2D*& collider, int& index) const
{
    const Matrix2D frame = bodyFrame(selected);
    const Math::Vec2 scale = bodyScale(selected);
    float best = kPointHitRadius;
    collider = nullptr;
    const std::size_t count = selected.componentCount<Collider2D>();
    for (std::size_t c = 0; c < count; ++c)
    {
        Collider2D* candidate = selected.getComponentAt<Collider2D>(c);
        if (!candidate || !candidate->active())
            continue;
        for (int i = 0, points = colliderPointCount(*candidate); i < points; ++i)
        {
            const Math::Vec2 s = toScreen(frame.Transform(colliderPoint(*candidate, scale, i)));
            const float distance = std::sqrt((s.x - x) * (s.x - x) + (s.y - y) * (s.y - y));
            if (distance <= best)
            {
                best = distance;
                collider = candidate;
                index = i;
            }
        }
    }
    return collider != nullptr;
}

bool SceneGizmo::hitSegment(GameObject& selected, float x, float y, Collider2D*& collider, int& after,
                            Math::Vec2& point) const
{
    const Matrix2D frame = bodyFrame(selected);
    const Math::Vec2 scale = bodyScale(selected);
    float best = kPointHitRadius;
    collider = nullptr;
    const std::size_t count = selected.componentCount<Collider2D>();
    for (std::size_t c = 0; c < count; ++c)
    {
        Collider2D* candidate = selected.getComponentAt<Collider2D>(c);
        if (!candidate || !candidate->active() || !colliderEditable(*candidate))
            continue;
        const int points = colliderPointCount(*candidate);
        const int segments = colliderClosed(*candidate) ? points : points - 1;
        for (int i = 0; i < segments; ++i)
        {
            const Math::Vec2 a = toScreen(frame.Transform(colliderPoint(*candidate, scale, i)));
            const Math::Vec2 b = toScreen(frame.Transform(colliderPoint(*candidate, scale, (i + 1) % points)));
            float t = 0.0f;
            const float distance = distanceToSegment(x, y, a.x, a.y, b.x, b.y, &t);
            if (distance <= best)
            {
                best = distance;
                collider = candidate;
                after = i;
                // Where the outline was clicked, so the shape does not shift when the point goes in.
                const Math::Vec2 world = toWorld(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t);
                point = frame.AffineInverse().Transform(world);
            }
        }
    }
    return collider != nullptr;
}

SceneGizmo::Press SceneGizmo::press(GameObject* selected, float x, float y, int clicks, bool ctrl)
{
    mHandle = Handle::None;
    if (!selected || selected->locked() || !selected->parent())
        return Press::Miss;
    mTarget = selected;
    mChanged = false;
    if (mTool == SceneTool::Select)
    {
        Collider2D* collider = nullptr;
        int index = -1;
        if (hitPoint(*selected, x, y, collider, index))
        {
            if (ctrl)
            {
                mEditName = "Remove collider point";
                return removeColliderPoint(*collider, index, mScratch) ? Press::Edited : Press::Shape;
            }
            mHandle = Handle::Point;
            mPointCollider = collider;
            mPointIndex = index;
            mEditName = "Edit collider";
            return Press::Drag;
        }
        Math::Vec2 point;
        if (hitSegment(*selected, x, y, collider, index, point))
        {
            if (clicks < 2 || !insertColliderPoint(*collider, bodyScale(*selected), index, point, mScratch))
                return Press::Shape;
            mHandle = Handle::Point;
            mPointCollider = collider;
            mPointIndex = index + 1;
            mChanged = true;
            mEditName = "Add collider point";
            return Press::Drag;
        }
        int jointIndex = -1;
        bool atB = false;
        if (hitJoint(*selected, x, y, jointIndex, atB))
        {
            mHandle = Handle::JointAnchor;
            mJointIndex = jointIndex;
            mJointAtB = atB;
            mEditName = "Move joint anchor";
            return Press::Drag;
        }
        int loop = -1;
        bool onEdge = false;
        Math::Vec2 local;
        if (NavigationRegion2D* region = selected->getComponent<NavigationRegion2D>();
            region && hitRegion(*selected, x, y, loop, index, onEdge, local))
        {
            const int count = static_cast<int>((loop == 0 ? region->polygon() : region->holes()[loop - 1]).size());
            copyLoops(*region);
            ct::Vector<Math::Vec2>& points = mLoopScratch[static_cast<std::size_t>(loop)];
            if (!onEdge && ctrl)
            {
                if (count <= 3)
                    return Press::Shape;
                points.erase(points.begin() + index);
                storeLoops(*region);
                mEditName = "Remove navigation point";
                return Press::Edited;
            }
            if (onEdge && clicks < 2)
                return Press::Shape;
            if (onEdge)
            {
                points.insert(points.begin() + index + 1, local);
                storeLoops(*region);
                ++index;
                mChanged = true;
            }
            mHandle = Handle::RegionPoint;
            mRegion = region;
            mRegionLoop = loop;
            mPointIndex = index;
            mEditName = onEdge ? "Add navigation point" : "Move navigation point";
            return Press::Drag;
        }
        if (hitBatch(*selected, x, y, index))
        {
            mHandle = Handle::BatchEntry;
            mBatch = selected->getComponent<SpriteBatch>();
            mPointIndex = index;
            mGrabLocal = selected->globalTransform().AffineInverse().Transform(toWorld(x, y));
            mGrabPosition = mBatch->entryAt(index)->position;
            mEditName = "Move batch sprite";
            return Press::Drag;
        }
        // A bone turns when its body is dragged and pulls its chain by the tip.
        Math::Vec2 joint, tip;
        const Bone2D* bone = selected->getComponent<Bone2D>();
        if (!bone || !boneEnds(*bone, joint, tip))
            return Press::Miss;
        const bool atTip = std::sqrt((x - tip.x) * (x - tip.x) + (y - tip.y) * (y - tip.y)) <= kPointHitRadius;
        if (!atTip && distanceToSegment(x, y, joint.x, joint.y, tip.x, tip.y) > kHitRadius)
            return Press::Miss;
        mHandle = atTip ? Handle::BoneTip : Handle::BoneTurn;
        mStartX = x;
        mStartY = y;
        mStartWorld = selected->globalPosition();
        mStartRotation = selected->rotationDegrees();
        mEditName = atTip ? "Pull bone chain" : "Turn bone";
        return Press::Drag;
    }
    mHandle = hit(*selected, x, y);
    if (mHandle == Handle::None)
        return Press::Miss;
    mStartX = x;
    mStartY = y;
    mStartWorld = selected->globalPosition();
    mStartScale = selected->scale();
    mStartRotation = selected->rotationDegrees();
    mEditName = mTool == SceneTool::Move ? "Move" : (mTool == SceneTool::Rotate ? "Rotate" : "Scale");
    // The others selected follow, all but those under one of them.
    mGroup.clear();
    if (mSelection && mSelection->size() > 1)
        for (GameObject* other : *mSelection)
        {
            bool nested = other == selected || other->locked() || !other->parent();
            for (GameObject* above = other->parent(); above && !nested; above = above->parent())
                nested = isSelected(above, selected);
            if (!nested)
                mGroup.push_back(GroupStart{other, other->globalPosition(), other->scale(), other->rotationDegrees()});
        }
    return Press::Drag;
}

bool SceneGizmo::drag(float x, float y)
{
    if (mHandle == Handle::None || !mTarget)
        return false;
    GameObject& object = *mTarget;
    const Math::Vec2 c = toScreen(mStartWorld);

    if (mHandle == Handle::Point)
    {
        Math::Vec2 world = toWorld(x, y);
        if (mSnap)
            world = Math::Vec2(snapped(world.x, kSnapStep), snapped(world.y, kSnapStep));
        setColliderPoint(*mPointCollider, bodyScale(object), mPointIndex, bodyFrame(object).AffineInverse().Transform(world),
                         mScratch);
    }
    else if (mHandle == Handle::JointAnchor)
    {
        JointEntry& entry = mJointEntries[static_cast<std::size_t>(mJointIndex)];
        GameObject* body = mJointAtB ? findTarget(entry) : entry.joint->owner();
        const ClassProperty* anchor = mJointAtB ? entry.anchorB : entry.anchorA;
        if (!body || !anchor)
            return false;
        Math::Vec2 world = toWorld(x, y);
        if (mSnap)
            world = Math::Vec2(snapped(world.x, kSnapStep), snapped(world.y, kSnapStep));
        anchor->set(*entry.joint, PropertyValue::from(bodyFrame(*body).AffineInverse().Transform(world)));
        if (entry.configured)
            entry.configured->set(*entry.joint, PropertyValue::from(true));
    }
    else if (mHandle == Handle::RegionPoint)
    {
        Math::Vec2 world = toWorld(x, y);
        if (mSnap)
            world = Math::Vec2(snapped(world.x, kSnapStep), snapped(world.y, kSnapStep));
        copyLoops(*mRegion);
        mLoopScratch[static_cast<std::size_t>(mRegionLoop)][static_cast<std::size_t>(mPointIndex)] =
            object.globalTransform().AffineInverse().Transform(world);
        storeLoops(*mRegion);
    }
    else if (mHandle == Handle::BatchEntry)
    {
        const Math::Vec2 local = object.globalTransform().AffineInverse().Transform(toWorld(x, y));
        Math::Vec2 position(mGrabPosition.x + local.x - mGrabLocal.x, mGrabPosition.y + local.y - mGrabLocal.y);
        if (mSnap)
            position = Math::Vec2(snapped(position.x, kSnapStep), snapped(position.y, kSnapStep));
        if (SpriteBatch::Entry* entry = mBatch->entryAt(mPointIndex))
            entry->position = position;
    }
    else if (mHandle == Handle::BoneTip)
    {
        Skeleton2D* skeleton = nullptr;
        for (GameObject* above = &object; above && !skeleton; above = above->parent())
            skeleton = above->getComponent<Skeleton2D>();
        if (!skeleton || !skeleton->solveIK(object.name().c_str(), toWorld(x, y), mIkChain, 16, 0.25f))
            return false;
    }
    else if (mTool == SceneTool::Move)
    {
        const Math::Vec2 from = toWorld(mStartX, mStartY);
        const Math::Vec2 to = toWorld(mHandle == Handle::AxisY ? mStartX : x, mHandle == Handle::AxisX ? mStartY : y);
        Math::Vec2 world(mStartWorld.x + to.x - from.x, mStartWorld.y + to.y - from.y);
        if (mSnap)
            world = Math::Vec2(snapped(world.x, kSnapStep), snapped(world.y, kSnapStep));
        // The handles move the object in the world; the parent's transform turns
        // that into the local position the object keeps.
        object.setPosition(object.parent()->globalTransform().AffineInverse().Transform(world));
    }
    else if (mTool == SceneTool::Rotate || mHandle == Handle::BoneTurn)
    {
        const float start = std::atan2(mStartY - c.y, mStartX - c.x);
        const float now = std::atan2(y - c.y, x - c.x);
        float degrees = mStartRotation + (now - start) * kRadToDeg;
        if (mSnap)
            degrees = snapped(degrees, kSnapDegrees);
        object.setRotationDegrees(degrees);
    }
    else if (mTool == SceneTool::Scale)
    {
        Math::Vec2 scale = mStartScale;
        if (mHandle == Handle::Free)
        {
            const float before = std::sqrt((mStartX - c.x) * (mStartX - c.x) + (mStartY - c.y) * (mStartY - c.y));
            const float after = std::sqrt((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y));
            const float ratio = before > 1.0f ? after / before : 1.0f;
            scale = Math::Vec2(mStartScale.x * ratio, mStartScale.y * ratio);
        }
        else
        {
            // The handle's distance from the centre, from the axis length at the press.
            const bool axisX = mHandle == Handle::AxisX;
            const float before = axisX ? mStartX - c.x : mStartY - c.y;
            const float after = axisX ? x - c.x : y - c.y;
            const float ratio = std::fabs(before) > 1.0f ? after / before : 1.0f;
            if (axisX)
                scale.x = mStartScale.x * ratio;
            else
                scale.y = mStartScale.y * ratio;
        }
        object.setScale(scale);
    }
    const bool transform = mHandle == Handle::AxisX || mHandle == Handle::AxisY || mHandle == Handle::Free ||
                           mHandle == Handle::Ring;
    if (transform)
    {
        const Math::Vec2 world = object.globalPosition();
        const Math::Vec2 moved(world.x - mStartWorld.x, world.y - mStartWorld.y);
        const float turned = object.rotationDegrees() - mStartRotation;
        const Math::Vec2 ratio(mStartScale.x != 0.0f ? object.scale().x / mStartScale.x : 1.0f,
                               mStartScale.y != 0.0f ? object.scale().y / mStartScale.y : 1.0f);
        for (const GroupStart& start : mGroup)
        {
            GameObject& other = *start.object;
            if (mTool == SceneTool::Move)
                other.setPosition(other.parent()->globalTransform().AffineInverse().Transform(
                    Math::Vec2(start.world.x + moved.x, start.world.y + moved.y)));
            else if (mTool == SceneTool::Rotate)
                other.setRotationDegrees(start.rotation + turned);
            else if (mTool == SceneTool::Scale)
                other.setScale(Math::Vec2(start.scale.x * ratio.x, start.scale.y * ratio.y));
        }
    }
    mChanged = true;
    return true;
}

bool SceneGizmo::release()
{
    const bool changed = mHandle != Handle::None && mChanged;
    mHandle = Handle::None;
    mTarget = nullptr;
    mPointCollider = nullptr;
    mPointIndex = -1;
    mJointIndex = -1;
    mRegion = nullptr;
    mGroup.clear();
    mBatch = nullptr;
    mChanged = false;
    return changed;
}

void SceneGizmo::paint(rt::PaintContext& ctx, const rt::Rect& area, Scene& scene, GameObject* selected)
{
    refresh(scene);
    updateShown();
    if (mGrid)
        paintGrid(ctx, area);

    if (CameraComponent* camera = scene.activeCamera())
        paintCameraFrames(ctx, area, *camera);

    const bool editable = selected && selected->parent();
    if (mColliders)
    {
        for (const ColliderEntry& entry : mColliderEntries)
            if (mShown[static_cast<std::size_t>(entry.entry)] && entry.collider->active())
                paintCollider(ctx, area, *mEntries[static_cast<std::size_t>(entry.entry)].object, *entry.collider);
    }
    else if (editable)
    {
        const std::size_t count = selected->componentCount<Collider2D>();
        for (std::size_t c = 0; c < count; ++c)
            if (Collider2D* collider = selected->getComponentAt<Collider2D>(c))
                if (collider->active() && colliderPointCount(*collider) > 0)
                    paintCollider(ctx, area, *selected, *collider);
    }

    if (mColliders || editable)
    {
        paintJoints(ctx, area, selected);
        paintRegions(ctx, area, selected);
    }
    paintBones(ctx, area, selected);
    paintMarkers(ctx, area, selected);
    if (mMarquee)
    {
        ctx.line.SetColor(kSelectedColor);
        ctx.lineRect(area.x + (mMarqueeX0 < mMarqueeX1 ? mMarqueeX0 : mMarqueeX1),
                     area.y + (mMarqueeY0 < mMarqueeY1 ? mMarqueeY0 : mMarqueeY1), std::fabs(mMarqueeX1 - mMarqueeX0),
                     std::fabs(mMarqueeY1 - mMarqueeY0));
    }
    if (!editable)
        return;
    paintOutline(ctx, area, *selected);
    if (mSelection)
        for (GameObject* other : *mSelection)
            if (other != selected && other->parent())
                paintOutline(ctx, area, *other);
    paintLight(ctx, area, *selected);
    if (mTool == SceneTool::Select)
    {
        paintPoints(ctx, area, *selected);
        paintBatch(ctx, area, *selected);
    }
    else
        paintHandles(ctx, area, *selected);
}

void SceneGizmo::paintGrid(rt::PaintContext& ctx, const rt::Rect& area) const
{
    const Math::Vec2 topLeft = toWorld(0.0f, 0.0f);
    const Math::Vec2 bottomRight = toWorld(mWidth, mHeight);
    const float pixels = (toScreen(Math::Vec2(kGridStep, 0.0f)).x - toScreen(Math::Vec2(0.0f, 0.0f)).x);
    if (pixels < 8.0f)
        return;
    ctx.fill.SetColor(rt::Color(255, 255, 255, 18));
    for (float x = std::floor(topLeft.x / kGridStep) * kGridStep; x <= bottomRight.x; x += kGridStep)
        ctx.fillRect(area.x + toScreen(Math::Vec2(x, 0.0f)).x, area.y, 1.0f, area.h);
    for (float y = std::floor(topLeft.y / kGridStep) * kGridStep; y <= bottomRight.y; y += kGridStep)
        ctx.fillRect(area.x, area.y + toScreen(Math::Vec2(0.0f, y)).y, area.w, 1.0f);
    // The world axes through the origin, a little brighter.
    const Math::Vec2 origin = toScreen(Math::Vec2(0.0f, 0.0f));
    ctx.fill.SetColor(rt::Color(230, 70, 70, 60));
    ctx.fillRect(area.x, area.y + origin.y, area.w, 1.0f);
    ctx.fill.SetColor(rt::Color(80, 210, 100, 60));
    ctx.fillRect(area.x + origin.x, area.y, 1.0f, area.h);
}

void SceneGizmo::paintMarkers(rt::PaintContext& ctx, const rt::Rect& area, GameObject* selected) const
{
    for (std::size_t i = 0; i < mEntries.size(); ++i)
    {
        if (!mShown[i])
            continue;
        GameObject& object = *mEntries[i].object;
        const Math::Vec2 p = toScreen(object.globalPosition());
        const bool isSelected = this->isSelected(&object, selected);
        ctx.fill.SetColor(isSelected ? kSelectedColor : rt::Color(70, 180, 235, 220));
        ctx.fill.Circle(area.x + p.x, area.y + p.y, isSelected ? 5.0f : 3.0f, true);
        if (isSelected)
        {
            const auto& theme = rt::Theme::instance();
            ctx.font.SetFontSize(theme.fontSize);
            ctx.font.SetBatch(&ctx.text);
            ctx.font.SetColor(rt::Color(220, 225, 235, 230));
            ctx.font.Print(object.name().empty() ? "GameObject" : object.name().c_str(), area.x + p.x + 8.0f,
                           area.y + p.y - 6.0f);
        }
    }
}

// What the game will see: the camera's resolution, the window around it (wider
// than the camera where Fit leaves bars, the same where Expand fills it) and
// the limits the camera keeps inside.
void SceneGizmo::paintCameraFrames(rt::PaintContext& ctx, const rt::Rect& area, const CameraComponent& camera) const
{
    const float width = camera.viewportWidth();
    const float height = camera.viewportHeight();
    if (width <= 0.0f || height <= 0.0f)
        return;
    const Matrix2D toWorld = camera.camera().CameraXform(width, height);
    auto corners = [&](float halfW, float halfH, Math::Vec2 out[4]) {
        const float cx = width * 0.5f;
        const float cy = height * 0.5f;
        out[0] = toScreen(toWorld.Transform(cx - halfW, cy - halfH));
        out[1] = toScreen(toWorld.Transform(cx + halfW, cy - halfH));
        out[2] = toScreen(toWorld.Transform(cx + halfW, cy + halfH));
        out[3] = toScreen(toWorld.Transform(cx - halfW, cy + halfH));
    };
    char label[96];
    Math::Vec2 frame[4];
    const GameViewport screen = CalculateGameViewport(mDisplayWidth, mDisplayHeight, width, height,
                                                      camera.viewportScaleMode(), camera.integerScale());
    if (screen.valid())
    {
        const float pixelsPerUnit = screen.width / screen.virtualWidth;
        corners(mDisplayWidth * 0.5f / pixelsPerUnit, mDisplayHeight * 0.5f / pixelsPerUnit, frame);
        std::snprintf(label, sizeof(label), "Screen %.0f x %.0f", mDisplayWidth, mDisplayHeight);
        paintFrame(ctx, area, frame, rt::Color(90, 200, 255, 170), label, true);
    }
    corners(width * 0.5f, height * 0.5f, frame);
    std::snprintf(label, sizeof(label), "Camera %.0f x %.0f | %s", width, height,
                  camera.viewportScaleMode() == ViewportScaleMode::Expand ? "Expand" : "Fit");
    paintFrame(ctx, area, frame, rt::Color(150, 120, 230, 220), label, false);

    const Camera2D& view = camera.camera();
    if (view.limitEnabled)
    {
        const Math::Vec4& l = view.limits;
        frame[0] = toScreen(Math::Vec2(l.x, l.y));
        frame[1] = toScreen(Math::Vec2(l.z, l.y));
        frame[2] = toScreen(Math::Vec2(l.z, l.w));
        frame[3] = toScreen(Math::Vec2(l.x, l.w));
        paintFrame(ctx, area, frame, rt::Color(255, 170, 60, 200), "Limits", false);
    }
}

void SceneGizmo::paintFrame(rt::PaintContext& ctx, const rt::Rect& area, const Math::Vec2 corners[4],
                            const rt::Color& color, const char* label, bool labelBelow) const
{
    ctx.line.SetColor(color);
    for (int i = 0; i < 4; ++i)
    {
        const Math::Vec2& a = corners[i];
        const Math::Vec2& b = corners[(i + 1) % 4];
        ctx.line.Line2D(area.x + a.x, area.y + a.y, area.x + b.x, area.y + b.y);
    }
    const auto& theme = rt::Theme::instance();
    ctx.font.SetFontSize(theme.fontSize);
    ctx.font.SetBatch(&ctx.text);
    ctx.font.SetColor(color);
    const Math::Vec2& at = labelBelow ? corners[3] : corners[0];
    ctx.font.Print(label, area.x + at.x + 4.0f, area.y + at.y + 4.0f);
}

void SceneGizmo::paintOutline(rt::PaintContext& ctx, const rt::Rect& area, GameObject& selected) const
{
    Math::Vec4 r;
    if (!objectBounds(selected, r))
        return;
    const Matrix2D& xform = selected.globalTransform();
    const Math::Vec2 q[4] = {toScreen(xform.Transform(r.x, r.y)), toScreen(xform.Transform(r.x + r.z, r.y)),
                             toScreen(xform.Transform(r.x + r.z, r.y + r.w)), toScreen(xform.Transform(r.x, r.y + r.w))};
    ctx.line.SetColor(kOutlineColor);
    for (int k = 0; k < 4; ++k)
        ctx.line.Line2D(area.x + q[k].x, area.y + q[k].y, area.x + q[(k + 1) % 4].x, area.y + q[(k + 1) % 4].y);
}

void SceneGizmo::paintCollider(rt::PaintContext& ctx, const rt::Rect& area, GameObject& object,
                               const Collider2D& collider) const
{
    const Matrix2D frame = bodyFrame(object);
    const Math::Vec2 scale = bodyScale(object);
    // A collider without a body on its object is never simulated.
    ctx.line.SetColor(!object.getComponent<RigidBody2D>() ? kNoBodyColor
                                                         : (collider.isSensor() ? kSensorColor : kSolidColor));
    const auto screen = [&](const Math::Vec2& body)
    {
        const Math::Vec2 s = toScreen(frame.Transform(body));
        return Math::Vec2(area.x + s.x, area.y + s.y);
    };
    const auto segment = [&](const Math::Vec2& a, const Math::Vec2& b)
    {
        const Math::Vec2 sa = screen(a), sb = screen(b);
        ctx.line.Line2D(sa.x, sa.y, sb.x, sb.y);
    };
    const Math::Vec2 o = collider.offset();
    switch (collider.componentClass())
    {
    case ComponentClass::BoxCollider2D:
    {
        const Math::Vec2 corner = colliderPoint(collider, scale, 0);
        const float hw = corner.x - o.x, hh = corner.y - o.y;
        const Math::Vec2 q[4] = {Math::Vec2(o.x - hw, o.y - hh), Math::Vec2(o.x + hw, o.y - hh),
                                 Math::Vec2(o.x + hw, o.y + hh), Math::Vec2(o.x - hw, o.y + hh)};
        for (int k = 0; k < 4; ++k)
            segment(q[k], q[(k + 1) % 4]);
        break;
    }
    case ComponentClass::CircleCollider2D:
    {
        const Math::Vec2 rim = colliderPoint(collider, scale, 0);
        const Math::Vec2 c = screen(o), e = screen(rim);
        ctx.line.Circle(c.x, c.y, std::sqrt((e.x - c.x) * (e.x - c.x) + (e.y - c.y) * (e.y - c.y)), false);
        ctx.line.Line2D(c.x, c.y, e.x, e.y);
        break;
    }
    default:
    {
        const int points = colliderPointCount(collider);
        const int segments = colliderClosed(collider) ? points : points - 1;
        for (int i = 0; i < segments; ++i)
            segment(colliderPoint(collider, scale, i), colliderPoint(collider, scale, (i + 1) % points));
        break;
    }
    }
}

void SceneGizmo::paintPoints(rt::PaintContext& ctx, const rt::Rect& area, GameObject& selected) const
{
    const Matrix2D frame = bodyFrame(selected);
    const Math::Vec2 scale = bodyScale(selected);
    const std::size_t count = selected.componentCount<Collider2D>();
    for (std::size_t c = 0; c < count; ++c)
    {
        const Collider2D* collider = selected.getComponentAt<Collider2D>(c);
        if (!collider || !collider->active())
            continue;
        for (int i = 0, points = colliderPointCount(*collider); i < points; ++i)
        {
            const Math::Vec2 s = toScreen(frame.Transform(colliderPoint(*collider, scale, i)));
            const bool active = collider == mPointCollider && i == mPointIndex;
            ctx.fill.SetColor(active ? kActiveColor : kSelectedColor);
            ctx.fillRect(area.x + s.x - kPointHalfSize, area.y + s.y - kPointHalfSize, kPointHalfSize * 2.0f,
                         kPointHalfSize * 2.0f);
        }
    }
}

void SceneGizmo::paintBones(rt::PaintContext& ctx, const rt::Rect& area, GameObject* selected) const
{
    for (const BoneEntry& entry : mBoneEntries)
    {
        Math::Vec2 a, b;
        if (!mShown[static_cast<std::size_t>(entry.entry)] || !boneEnds(*entry.bone, a, b))
            continue;
        const bool isSelected = entry.bone->owner() == selected;
        const float dx = b.x - a.x, dy = b.y - a.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length < 1.0f)
            continue;
        const float width = length * 0.12f < 8.0f ? length * 0.12f : 8.0f;
        const float nx = -dy / length * width, ny = dx / length * width;
        const float mx = a.x + dx * 0.2f, my = a.y + dy * 0.2f;
        const Math::Vec2 kite[4] = {Math::Vec2(a.x, a.y), Math::Vec2(mx + nx, my + ny), Math::Vec2(b.x, b.y),
                                    Math::Vec2(mx - nx, my - ny)};
        ctx.line.SetColor(isSelected ? kSelectedColor : kBoneColor);
        for (int k = 0; k < 4; ++k)
            ctx.line.Line2D(area.x + kite[k].x, area.y + kite[k].y, area.x + kite[(k + 1) % 4].x,
                            area.y + kite[(k + 1) % 4].y);
        if (isSelected && mTool == SceneTool::Select)
        {
            ctx.fill.SetColor(mHandle == Handle::BoneTip ? kActiveColor : kSelectedColor);
            ctx.fill.Circle(area.x + b.x, area.y + b.y, 4.0f, true);
        }
    }
}

void SceneGizmo::paintLight(rt::PaintContext& ctx, const rt::Rect& area, GameObject& selected) const
{
    const Light2D* light = selected.getComponent<Light2D>();
    if (!light || !light->active())
        return;
    const Math::Vec2 world = selected.globalPosition();
    const Math::Vec2 c = toScreen(world);
    const Math::Vec2 e = toScreen(Math::Vec2(world.x + light->radius(), world.y));
    ctx.line.SetColor(kLightColor);
    ctx.line.Circle(area.x + c.x, area.y + c.y, std::sqrt((e.x - c.x) * (e.x - c.x) + (e.y - c.y) * (e.y - c.y)),
                    false);
}

void SceneGizmo::paintHandles(rt::PaintContext& ctx, const rt::Rect& area, GameObject& selected) const
{
    const Math::Vec2 p = toScreen(selected.globalPosition());
    const float cx = area.x + p.x, cy = area.y + p.y;
    if (mTool == SceneTool::Move || mTool == SceneTool::Scale)
    {
        const rt::Color xColor = mHandle == Handle::AxisX ? kActiveColor : kAxisXColor;
        const rt::Color yColor = mHandle == Handle::AxisY ? kActiveColor : kAxisYColor;
        ctx.fill.SetColor(xColor);
        ctx.fillRect(cx, cy - 1.0f, kAxisLength, 2.0f);
        if (mTool == SceneTool::Move)
            ctx.fillTriangle(cx + kAxisLength - 9.0f, cy - 5.0f, cx + kAxisLength - 9.0f, cy + 5.0f,
                             cx + kAxisLength + 3.0f, cy);
        else
            ctx.fillRect(cx + kAxisLength - 4.0f, cy - 4.0f, 8.0f, 8.0f);
        ctx.fill.SetColor(yColor);
        ctx.fillRect(cx - 1.0f, cy, 2.0f, kAxisLength);
        if (mTool == SceneTool::Move)
            ctx.fillTriangle(cx - 5.0f, cy + kAxisLength - 9.0f, cx + 5.0f, cy + kAxisLength - 9.0f, cx,
                             cy + kAxisLength + 3.0f);
        else
            ctx.fillRect(cx - 4.0f, cy + kAxisLength - 4.0f, 8.0f, 8.0f);
        ctx.fill.SetColor(mHandle == Handle::Free ? kActiveColor : rt::Color(230, 230, 235, 255));
        ctx.fillRect(cx - kCenterRadius * 0.5f, cy - kCenterRadius * 0.5f, kCenterRadius, kCenterRadius);
    }
    else if (mTool == SceneTool::Rotate)
    {
        ctx.line.SetColor(mHandle == Handle::Ring ? kActiveColor : kRingColor);
        ctx.line.Circle(cx, cy, kRingRadius, false);
        ctx.line.Circle(cx, cy, kRingRadius - 1.0f, false);
    }
}

} // namespace k2d::editor

namespace k2d::editor
{

namespace
{

const rt::Color kJointColor(240, 170, 80, 220);
const rt::Color kRegionColor(80, 210, 200, 200);
const rt::Color kBatchColor(140, 180, 255, 180);

const ct::Vector<Math::Vec2>& regionLoop(const NavigationRegion2D& region, int loop)
{
    return loop == 0 ? region.polygon() : region.holes()[static_cast<std::size_t>(loop - 1)];
}

int regionLoops(const NavigationRegion2D& region)
{
    return region.polygon().empty() ? 0 : 1 + static_cast<int>(region.holes().size());
}

} // namespace

void SceneGizmo::paintJoints(rt::PaintContext& ctx, const rt::Rect& area, GameObject* selected)
{
    for (JointEntry& entry : mJointEntries)
    {
        GameObject* owner = entry.joint->owner();
        Math::Vec2 a, b;
        if (!mShown[static_cast<std::size_t>(entry.entry)] || !entry.joint->active() || !jointEnds(entry, a, b))
            continue;
        const bool mine = owner == selected || entry.target == selected;
        if (!mColliders && !mine)
            continue;
        ctx.line.SetColor(kJointColor);
        ctx.line.Line2D(area.x + a.x, area.y + a.y, area.x + b.x, area.y + b.y);
        ctx.line.Circle(area.x + a.x, area.y + a.y, 4.0f, false);
        ctx.line.Circle(area.x + b.x, area.y + b.y, 4.0f, false);
        if (mine && mTool == SceneTool::Select && entry.anchorA)
        {
            ctx.fill.SetColor(kJointColor);
            if (owner == selected)
                ctx.fill.Circle(area.x + a.x, area.y + a.y, 4.0f, true);
            if (entry.target == selected && entry.anchorB)
                ctx.fill.Circle(area.x + b.x, area.y + b.y, 4.0f, true);
        }
    }
}

bool SceneGizmo::hitJoint(GameObject& selected, float x, float y, int& joint, bool& atB)
{
    for (std::size_t i = 0; i < mJointEntries.size(); ++i)
    {
        JointEntry& entry = mJointEntries[i];
        Math::Vec2 a, b;
        if (!entry.anchorA || !jointEnds(entry, a, b))
            continue;
        if (entry.joint->owner() == &selected && std::hypot(a.x - x, a.y - y) <= kPointHitRadius)
        {
            joint = static_cast<int>(i);
            atB = false;
            return true;
        }
        if (entry.target == &selected && entry.anchorB && std::hypot(b.x - x, b.y - y) <= kPointHitRadius)
        {
            joint = static_cast<int>(i);
            atB = true;
            return true;
        }
    }
    return false;
}

void SceneGizmo::paintRegions(rt::PaintContext& ctx, const rt::Rect& area, GameObject* selected) const
{
    for (const RegionEntry& entry : mRegionEntries)
    {
        const GameObject& object = *mEntries[static_cast<std::size_t>(entry.entry)].object;
        const bool mine = &object == selected;
        if (!mShown[static_cast<std::size_t>(entry.entry)] || (!mColliders && !mine))
            continue;
        const Matrix2D& xform = object.globalTransform();
        for (int loop = 0, loops = regionLoops(*entry.region); loop < loops; ++loop)
        {
            const ct::Vector<Math::Vec2>& points = regionLoop(*entry.region, loop);
            ctx.line.SetColor(kRegionColor);
            for (std::size_t i = 0; i < points.size(); ++i)
            {
                const Math::Vec2 p = toScreen(xform.Transform(points[i]));
                const Math::Vec2 q = toScreen(xform.Transform(points[(i + 1) % points.size()]));
                ctx.line.Line2D(area.x + p.x, area.y + p.y, area.x + q.x, area.y + q.y);
                if (mine && mTool == SceneTool::Select)
                {
                    const bool active = entry.region == mRegion && loop == mRegionLoop &&
                                        static_cast<int>(i) == mPointIndex;
                    ctx.fill.SetColor(active ? kActiveColor : kRegionColor);
                    ctx.fillRect(area.x + p.x - kPointHalfSize, area.y + p.y - kPointHalfSize, kPointHalfSize * 2.0f,
                                 kPointHalfSize * 2.0f);
                }
            }
        }
    }
}

bool SceneGizmo::hitRegion(GameObject& selected, float x, float y, int& loop, int& index, bool& onEdge,
                           Math::Vec2& local) const
{
    const NavigationRegion2D* region = selected.getComponent<NavigationRegion2D>();
    if (!region)
        return false;
    const Matrix2D& xform = selected.globalTransform();
    for (int l = 0, loops = regionLoops(*region); l < loops; ++l)
    {
        const ct::Vector<Math::Vec2>& points = regionLoop(*region, l);
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            const Math::Vec2 p = toScreen(xform.Transform(points[i]));
            if (std::hypot(p.x - x, p.y - y) <= kPointHitRadius)
            {
                loop = l;
                index = static_cast<int>(i);
                onEdge = false;
                return true;
            }
        }
    }
    for (int l = 0, loops = regionLoops(*region); l < loops; ++l)
    {
        const ct::Vector<Math::Vec2>& points = regionLoop(*region, l);
        for (std::size_t i = 0; i < points.size(); ++i)
        {
            const Math::Vec2 p = toScreen(xform.Transform(points[i]));
            const Math::Vec2 q = toScreen(xform.Transform(points[(i + 1) % points.size()]));
            float t = 0.0f;
            if (distanceToSegment(x, y, p.x, p.y, q.x, q.y, &t) <= kPointHitRadius)
            {
                loop = l;
                index = static_cast<int>(i);
                onEdge = true;
                local = xform.AffineInverse().Transform(toWorld(p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t));
                return true;
            }
        }
    }
    return false;
}

void SceneGizmo::copyLoops(const NavigationRegion2D& region)
{
    const int loops = regionLoops(region);
    mLoopScratch.resize(static_cast<std::size_t>(loops));
    for (int l = 0; l < loops; ++l)
    {
        ct::Vector<Math::Vec2>& out = mLoopScratch[static_cast<std::size_t>(l)];
        out.clear();
        for (const Math::Vec2& p : regionLoop(region, l))
            out.push_back(p);
    }
}

void SceneGizmo::storeLoops(NavigationRegion2D& region)
{
    if (mLoopScratch.empty())
        return;
    mHolePointers.clear();
    mHoleCounts.clear();
    for (std::size_t l = 1; l < mLoopScratch.size(); ++l)
    {
        mHolePointers.push_back(mLoopScratch[l].data());
        mHoleCounts.push_back(static_cast<int>(mLoopScratch[l].size()));
    }
    region.setPolygonWithHoles(mLoopScratch[0].data(), static_cast<int>(mLoopScratch[0].size()), mHolePointers.data(),
                               mHoleCounts.data(), static_cast<int>(mHolePointers.size()));
}

void SceneGizmo::paintBatch(rt::PaintContext& ctx, const rt::Rect& area, GameObject& selected) const
{
    const SpriteBatch* batch = selected.getComponent<SpriteBatch>();
    if (!batch)
        return;
    const Matrix2D& xform = selected.globalTransform();
    for (int i = 0; i < batch->count(); ++i)
    {
        const SpriteBatch::Entry* entry = batch->entry(i);
        if (!entry)
            continue;
        const Math::Vec2 p = entry->position, s = entry->size;
        const Math::Vec2 q[4] = {toScreen(xform.Transform(p)), toScreen(xform.Transform(p.x + s.x, p.y)),
                                 toScreen(xform.Transform(p.x + s.x, p.y + s.y)), toScreen(xform.Transform(p.x, p.y + s.y))};
        ctx.line.SetColor(batch == mBatch && i == mPointIndex ? kActiveColor : kBatchColor);
        for (int k = 0; k < 4; ++k)
            ctx.line.Line2D(area.x + q[k].x, area.y + q[k].y, area.x + q[(k + 1) % 4].x, area.y + q[(k + 1) % 4].y);
    }
}

// The top sprite under the point: entries draw in order, so the last one wins.
bool SceneGizmo::hitBatch(GameObject& selected, float x, float y, int& index) const
{
    const SpriteBatch* batch = selected.getComponent<SpriteBatch>();
    if (!batch)
        return false;
    const Math::Vec2 local = selected.globalTransform().AffineInverse().Transform(toWorld(x, y));
    for (int i = batch->count() - 1; i >= 0; --i)
    {
        const SpriteBatch::Entry* entry = batch->entry(i);
        if (entry && local.x >= entry->position.x && local.y >= entry->position.y &&
            local.x <= entry->position.x + entry->size.x && local.y <= entry->position.y + entry->size.y)
        {
            index = i;
            return true;
        }
    }
    return false;
}

} // namespace k2d::editor

namespace k2d::editor
{

bool SceneGizmo::isSelected(const GameObject* object, const GameObject* primary) const
{
    return object && (object == primary || (mSelection && mSelection->contains(object)));
}

void SceneGizmo::setMarquee(bool on, float x0, float y0, float x1, float y1)
{
    mMarquee = on;
    mMarqueeX0 = x0;
    mMarqueeY0 = y0;
    mMarqueeX1 = x1;
    mMarqueeY1 = y1;
}

void SceneGizmo::objectsIn(Scene& scene, float x0, float y0, float x1, float y1, ct::Vector<GameObject*>& out)
{
    refresh(scene);
    updateShown();
    out.clear();
    const float left = x0 < x1 ? x0 : x1, right = x0 < x1 ? x1 : x0;
    const float top = y0 < y1 ? y0 : y1, bottom = y0 < y1 ? y1 : y0;
    for (std::size_t i = 0; i < mEntries.size(); ++i)
    {
        GameObject* object = mEntries[i].object;
        if (!mShown[i] || !pickable(*object))
            continue;
        const Math::Vec2 p = toScreen(object->globalPosition());
        if (p.x >= left && p.x <= right && p.y >= top && p.y <= bottom)
            out.push_back(object);
    }
}

} // namespace k2d::editor
