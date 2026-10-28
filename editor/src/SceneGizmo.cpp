#include "SceneGizmo.h"

#include "Theme.hpp"

#include <k2d/CameraComponent.h>
#include <k2d/GameObject.h>
#include <k2d/Scene.h>

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
constexpr float kGridStep = 64.0f;
constexpr float kSnapStep = 16.0f;
constexpr float kSnapDegrees = 15.0f;
constexpr float kRadToDeg = 57.29577951f;

const rt::Color kAxisXColor(230, 70, 70, 255);
const rt::Color kAxisYColor(80, 210, 100, 255);
const rt::Color kActiveColor(255, 235, 120, 255);
const rt::Color kRingColor(120, 170, 230, 255);

float distanceToSegment(float px, float py, float ax, float ay, float bx, float by)
{
    const float dx = bx - ax, dy = by - ay;
    const float length = dx * dx + dy * dy;
    float t = length > 0.0f ? ((px - ax) * dx + (py - ay) * dy) / length : 0.0f;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    const float cx = ax + t * dx - px, cy = ay + t * dy - py;
    return std::sqrt(cx * cx + cy * cy);
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
}

Math::Vec2 SceneGizmo::toScreen(const Math::Vec2& world) const
{
    return mCamera.CameraXform(mWidth, mHeight).AffineInverse().Transform(world);
}

Math::Vec2 SceneGizmo::toWorld(float x, float y) const
{
    return mCamera.ScreenToWorld(x, y, mWidth, mHeight);
}

GameObject* SceneGizmo::pick(Scene& scene, float x, float y) const
{
    GameObject* best = nullptr;
    float bestDistance = kPickRadius;
    ct::Vector<GameObject*> stack;
    GameObject& root = scene.root();
    for (std::size_t i = 0; i < root.childCount(); ++i)
        stack.push_back(root.child(i));
    while (!stack.empty())
    {
        GameObject* object = stack.back();
        stack.pop_back();
        if (!object->visible())
            continue;
        if (pickable(*object))
        {
            const Math::Vec2 point = toScreen(object->globalPosition());
            const float distance = std::sqrt((point.x - x) * (point.x - x) + (point.y - y) * (point.y - y));
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = object;
            }
        }
        for (std::size_t i = 0; i < object->childCount(); ++i)
            stack.push_back(object->child(i));
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

bool SceneGizmo::press(GameObject* selected, float x, float y)
{
    mHandle = Handle::None;
    if (!selected || selected->locked() || !selected->parent())
        return false;
    mHandle = hit(*selected, x, y);
    if (mHandle == Handle::None)
        return false;
    mTarget = selected;
    mChanged = false;
    mStartX = x;
    mStartY = y;
    mStartWorld = selected->globalPosition();
    mStartScale = selected->scale();
    mStartRotation = selected->rotationDegrees();
    return true;
}

bool SceneGizmo::drag(float x, float y)
{
    if (mHandle == Handle::None || !mTarget)
        return false;
    GameObject& object = *mTarget;
    const Math::Vec2 c = toScreen(mStartWorld);

    if (mTool == SceneTool::Move)
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
    else if (mTool == SceneTool::Rotate)
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
    mChanged = false;
    return changed;
}

const char* SceneGizmo::editName() const
{
    switch (mTool)
    {
    case SceneTool::Move:
        return "Move";
    case SceneTool::Rotate:
        return "Rotate";
    case SceneTool::Scale:
        return "Scale";
    case SceneTool::Select:
        break;
    }
    return "Edit";
}

void SceneGizmo::paint(rt::PaintContext& ctx, const rt::Rect& area, Scene& scene, GameObject* selected) const
{
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

    GameObject& root = scene.root();
    for (std::size_t i = 0; i < root.childCount(); ++i)
        paintMarkers(ctx, area, *root.child(i), selected);
    if (selected && selected->parent())
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

void SceneGizmo::paintMarkers(rt::PaintContext& ctx, const rt::Rect& area, GameObject& object,
                              GameObject* selected) const
{
    if (!object.visible())
        return;
    const Math::Vec2 p = toScreen(object.globalPosition());
    const bool isSelected = &object == selected;
    ctx.fill.SetColor(isSelected ? rt::Color(255, 205, 65, 255) : rt::Color(70, 180, 235, 220));
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
    for (std::size_t i = 0; i < object.childCount(); ++i)
        paintMarkers(ctx, area, *object.child(i), selected);
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
