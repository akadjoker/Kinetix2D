#pragma once

#include "Widget.hpp"

#include <k2d/Camera2D.h>
#include <k2d/Matrix2D.h>

#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{
class Collider2D;
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
// grid, the object markers and outlines, colliders, picking, the handles of the
// move, rotate and scale tools, and the points of the selected object's
// colliders under the select tool. Coordinates are in the view's points, (0, 0)
// at its top left.
class SceneGizmo
{
public:
    enum class Press
    {
        Miss,
        Drag,
        Shape,
        Edited
    };

    void setTool(SceneTool tool) { mTool = tool; }
    SceneTool tool() const { return mTool; }
    void setGrid(bool grid) { mGrid = grid; }
    void setSnap(bool snap) { mSnap = snap; }
    void setColliders(bool colliders) { mColliders = colliders; }

    // The camera and the size of the view the next calls are measured in.
    void setView(const Camera2D& camera, float width, float height);

    // The object under the point: an origin marker within a few points first,
    // then the smallest outline holding it; hidden and locked objects are skipped.
    GameObject* pick(Scene& scene, float x, float y);

    // Drag starts a drag on a handle, Shape is a press on the selected object's
    // collider outline, Edited a point added (double click on an outline) or
    // removed (Ctrl+click on it); Miss leaves the press to picking.
    Press press(GameObject* selected, float x, float y, int clicks, bool ctrl);
    // Moves the drag on; true when the object changed.
    bool drag(float x, float y);
    // Ends the drag; true when it changed the object.
    bool release();
    bool dragging() const { return mHandle != Handle::None; }
    // What the last press set out to do, as an undo step names it.
    const char* editName() const { return mEditName; }

    void paint(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, Scene& scene, GameObject* selected);

private:
    enum class Handle
    {
        None,
        AxisX,
        AxisY,
        Free,
        Ring,
        Point
    };

    struct Entry
    {
        GameObject* object;
        int parent;
    };

    struct ColliderEntry
    {
        Collider2D* collider;
        int entry;
    };

    Math::Vec2 toScreen(const Math::Vec2& world) const;
    Math::Vec2 toWorld(float x, float y) const;
    void refresh(Scene& scene);
    void updateShown();
    Handle hit(GameObject& selected, float x, float y) const;
    bool hitPoint(GameObject& selected, float x, float y, Collider2D*& collider, int& index) const;
    bool hitSegment(GameObject& selected, float x, float y, Collider2D*& collider, int& after,
                    Math::Vec2& point) const;
    void paintGrid(ig::retained::PaintContext& ctx, const ig::retained::Rect& area) const;
    void paintMarkers(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject* selected) const;
    void paintOutline(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& selected) const;
    void paintCollider(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& object,
                       const Collider2D& collider) const;
    void paintPoints(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& selected) const;
    void paintLight(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& selected) const;
    void paintHandles(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& selected) const;

    SceneTool mTool = SceneTool::Select;
    bool mGrid = true;
    bool mSnap = false;
    bool mColliders = true;
    Camera2D mCamera;
    Matrix2D mView;
    float mWidth = 1.0f;
    float mHeight = 1.0f;

    // The scene's objects in tree order and its colliders, kept until its
    // topology changes; which objects show is worked out again every frame.
    const Scene* mScene = nullptr;
    uint32_t mTopology = 0;
    ct::Vector<Entry> mEntries;
    ct::Vector<ColliderEntry> mColliderEntries;
    ct::Vector<unsigned char> mShown;
    ct::Vector<Math::Vec2> mScratch;

    Handle mHandle = Handle::None;
    GameObject* mTarget = nullptr;
    Collider2D* mPointCollider = nullptr;
    int mPointIndex = -1;
    const char* mEditName = "Edit";
    bool mChanged = false;
    float mStartX = 0.0f;
    float mStartY = 0.0f;
    Math::Vec2 mStartWorld;
    Math::Vec2 mStartScale;
    float mStartRotation = 0.0f;
};

} // namespace k2d::editor
