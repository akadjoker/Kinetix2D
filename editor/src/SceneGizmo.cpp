#include "SceneGizmo.h"

#include "SceneShapes.h"
#include "Theme.hpp"

#include <k2d/CameraComponent.h>
#include <k2d/Collider2D.h>
#include <k2d/GameObject.h>
#include <k2d/Bone2D.h>
#include <k2d/Light2D.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Scene.h>
#include <k2d/Skeleton2D.h>

#include <cfloat>
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
    }
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
    mChanged = false;
    return changed;
}

void SceneGizmo::paint(rt::PaintContext& ctx, const rt::Rect& area, Scene& scene, GameObject* selected)
{
    refresh(scene);
    updateShown();
    if (mGrid)
        paintGrid(ctx, area);

    // The game camera's frame, so the view shows what the game will see.
    if (CameraComponent* camera = scene.activeCamera())
    {
        const Math::Vec2 centre = camera->owner()->globalPosition();
        const float halfW = camera->viewportWidth() * 0.5f / camera->camera().zoom.x;
        const float halfH = camera->viewportHeight() * 0.5f / camera->camera().zoom.y;
        const Math::Vec2 a = toScreen(Math::Vec2(centre.x - halfW, centre.y - halfH));
        const Math::Vec2 b = toScreen(Math::Vec2(centre.x + halfW, centre.y + halfH));
        ctx.line.SetColor(rt::Color(150, 120, 230, 200));
        ctx.lineRect(area.x + a.x, area.y + a.y, b.x - a.x, b.y - a.y);
    }

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

    paintBones(ctx, area, selected);
    paintMarkers(ctx, area, selected);
    if (!editable)
        return;
    paintOutline(ctx, area, *selected);
    paintLight(ctx, area, *selected);
    if (mTool == SceneTool::Select)
        paintPoints(ctx, area, *selected);
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
        const bool isSelected = &object == selected;
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
