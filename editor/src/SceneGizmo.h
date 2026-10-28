#pragma once

#include "Widget.hpp"

#include <k2d/Camera2D.h>
#include <mathc.h>

namespace k2d
{
class GameObject;
class Scene;
}

namespace k2d::editor
{

enum class SceneTool
{
    Select,
    Move,
    Rotate,
    Scale
};

// What the scene view draws over the scene and how it answers the mouse: the
// grid, the object markers, picking, and the handles of the move, rotate and
// scale tools. Coordinates are in the view's points, (0, 0) at its top left.
class SceneGizmo
{
public:
    void setTool(SceneTool tool) { mTool = tool; }
    SceneTool tool() const { return mTool; }
    void setGrid(bool grid) { mGrid = grid; }
    void setSnap(bool snap) { mSnap = snap; }

    // The camera and the size of the view the next calls are measured in.
    void setView(const Camera2D& camera, float width, float height);

    // The object whose origin is nearest to the point, within a few points;
    // hidden and locked objects are skipped.
    GameObject* pick(Scene& scene, float x, float y) const;

    // Starts a drag when the point is on a handle of the selected object.
    bool press(GameObject* selected, float x, float y);
    // Moves the drag on; true when the object changed.
    bool drag(float x, float y);
    // Ends the drag; true when it changed the object.
    bool release();
    bool dragging() const { return mHandle != Handle::None; }
    const char* editName() const;

    void paint(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, Scene& scene,
               GameObject* selected) const;

private:
    enum class Handle
    {
        None,
        AxisX,
        AxisY,
        Free,
        Ring
    };

    Math::Vec2 toScreen(const Math::Vec2& world) const;
    Math::Vec2 toWorld(float x, float y) const;
    Handle hit(GameObject& selected, float x, float y) const;
    void paintGrid(ig::retained::PaintContext& ctx, const ig::retained::Rect& area) const;
    void paintMarkers(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& object,
                      GameObject* selected) const;
    void paintHandles(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& selected) const;

    SceneTool mTool = SceneTool::Select;
    bool mGrid = true;
    bool mSnap = false;
    Camera2D mCamera;
    float mWidth = 1.0f;
    float mHeight = 1.0f;

    Handle mHandle = Handle::None;
    GameObject* mTarget = nullptr;
    bool mChanged = false;
    float mStartX = 0.0f;
    float mStartY = 0.0f;
    Math::Vec2 mStartWorld;
    Math::Vec2 mStartScale;
    float mStartRotation = 0.0f;
};

} // namespace k2d::editor
