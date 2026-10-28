#pragma once

#include "Widget.hpp"

#include <k2d/Camera2D.h>
#include <k2d/Matrix2D.h>

#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{
class Bone2D;
class CameraComponent;
class Collider2D;
class Joint2D;
class NavigationRegion2D;
class SpriteBatch;
struct ClassProperty;
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
    // The size of the game's window, for the screen's frame around the camera.
    void setDisplay(float width, float height)
    {
        mDisplayWidth = width;
        mDisplayHeight = height;
    }
    // How many bones a pulled bone tip moves, itself included.
    void setIkChain(int bones) { mIkChain = bones > 0 ? bones : 1; }
    // The objects selected with the one passed to press and paint: drawn as
    // selected and moved, turned and scaled with it.
    void setSelection(const ct::Vector<GameObject*>* selection) { mSelection = selection; }
    // A rectangle being dragged out to select what it holds (view points).
    void setMarquee(bool on, float x0 = 0.0f, float y0 = 0.0f, float x1 = 0.0f, float y1 = 0.0f);
    // The pickable objects whose origin is in the rectangle.
    void objectsIn(Scene& scene, float x0, float y0, float x1, float y1, ct::Vector<GameObject*>& out);

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
        Point,
        BoneTurn,
        BoneTip,
        JointAnchor,
        RegionPoint,
        BatchEntry
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

    struct BoneEntry
    {
        Bone2D* bone;
        int entry;
    };

    // A joint drawn from its body to the object it names; that object is looked
    // up again only when the name changes.
    struct JointEntry
    {
        Joint2D* joint;
        int entry;
        const ClassProperty* anchorA;
        const ClassProperty* anchorB;
        const ClassProperty* configured;
        ct::String targetName;
        GameObject* target;
    };

    struct GroupStart
    {
        GameObject* object;
        Math::Vec2 world;
        Math::Vec2 scale;
        float rotation;
    };

    struct RegionEntry
    {
        NavigationRegion2D* region;
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
    void paintCameraFrames(ig::retained::PaintContext& ctx, const ig::retained::Rect& area,
                           const CameraComponent& camera, bool active) const;
    void paintScreenFrame(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, const Camera2D& camera,
                          float halfWidth, float halfHeight, const char* label) const;
    void paintFrame(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, const Math::Vec2 corners[4],
                    const ig::retained::Color& color, const char* label, bool labelBelow) const;
    void paintMarkers(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject* selected) const;
    void paintOutline(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& selected) const;
    void paintCollider(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& object,
                       const Collider2D& collider) const;
    void paintPoints(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& selected) const;
    void paintLight(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& selected) const;
    void paintBones(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject* selected) const;
    bool boneEnds(const Bone2D& bone, Math::Vec2& from, Math::Vec2& to) const;
    bool isSelected(const GameObject* object, const GameObject* primary) const;
    GameObject* findTarget(JointEntry& entry);
    bool jointEnds(JointEntry& entry, Math::Vec2& a, Math::Vec2& b);
    void paintJoints(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject* selected);
    void paintRegions(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject* selected) const;
    void paintBatch(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& selected) const;
    bool hitJoint(GameObject& selected, float x, float y, int& joint, bool& atB);
    bool hitRegion(GameObject& selected, float x, float y, int& loop, int& index, bool& onEdge, Math::Vec2& local) const;
    bool hitBatch(GameObject& selected, float x, float y, int& index) const;
    void copyLoops(const NavigationRegion2D& region);
    void storeLoops(NavigationRegion2D& region);
    void paintHandles(ig::retained::PaintContext& ctx, const ig::retained::Rect& area, GameObject& selected) const;

    SceneTool mTool = SceneTool::Select;
    bool mGrid = true;
    float mDisplayWidth = 1280.0f;
    float mDisplayHeight = 720.0f;
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
    ct::Vector<BoneEntry> mBoneEntries;
    ct::Vector<JointEntry> mJointEntries;
    ct::Vector<RegionEntry> mRegionEntries;
    const ct::Vector<GameObject*>* mSelection = nullptr;
    ct::Vector<GroupStart> mGroup;
    bool mMarquee = false;
    float mMarqueeX0 = 0.0f, mMarqueeY0 = 0.0f, mMarqueeX1 = 0.0f, mMarqueeY1 = 0.0f;
    ct::Vector<ct::Vector<Math::Vec2>> mLoopScratch;
    ct::Vector<const Math::Vec2*> mHolePointers;
    ct::Vector<int> mHoleCounts;
    int mIkChain = 2;
    ct::Vector<unsigned char> mShown;
    ct::Vector<Math::Vec2> mScratch;

    Handle mHandle = Handle::None;
    GameObject* mTarget = nullptr;
    Collider2D* mPointCollider = nullptr;
    int mPointIndex = -1;
    int mJointIndex = -1;
    bool mJointAtB = false;
    NavigationRegion2D* mRegion = nullptr;
    int mRegionLoop = -1;
    SpriteBatch* mBatch = nullptr;
    Math::Vec2 mGrabLocal;
    Math::Vec2 mGrabPosition;
    const char* mEditName = "Edit";
    bool mChanged = false;
    float mStartX = 0.0f;
    float mStartY = 0.0f;
    Math::Vec2 mStartWorld;
    Math::Vec2 mStartScale;
    float mStartRotation = 0.0f;
};

} // namespace k2d::editor
