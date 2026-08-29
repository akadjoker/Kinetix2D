#pragma once

#include "core/EditorPanel.h"

#include <k2d/CanvasRenderer.h>

#include <ct/vector.hpp>

#include <mathc.h>

namespace k2d
{
class Bone2D;
class Collider2D;
class GameObject;
class SpriteBatch;
class CameraComponent;
}

namespace k2d::editor
{

class SceneViewportPanel final : public EditorPanel
{
public:
    explicit SceneViewportPanel(EditorApplication &application);
    ~SceneViewportPanel() override;

private:
    void drawContents() override;
    void drawGrid(ImDrawList &drawList, const ImVec2 &min, const ImVec2 &max) const;
    void drawObject(ImDrawList &drawList, GameObject &object, const ImVec2 &origin);
    void drawColliders(ImDrawList &drawList, GameObject &object, const ImVec2 &origin);
    void pickObject(GameObject &object, const ImVec2 &mouse, const ImVec2 &origin,
                    GameObject *&best, float &bestDistance);
    ImVec2 worldToScreen(float x, float y, const ImVec2 &origin) const;
    Math::Vec2 screenToWorld(const ImVec2 &screen, const ImVec2 &origin) const;
    void handlePrefabDrop(const ImVec2 &origin);
    void handleImageDrop(const ImVec2 &origin);
    void recordBoneKey(GameObject &boneObject);

    void drawGizmo(ImDrawList &drawList, GameObject &selected, const ImVec2 &origin) const;
    int hitTestGizmo(GameObject &selected, const ImVec2 &mouse, const ImVec2 &origin) const;

    ImVec2 objectLocalToScreen(GameObject &object, const Math::Vec2 &local, const ImVec2 &origin) const;
    Math::Vec2 screenToObjectLocal(GameObject &object, const ImVec2 &screen, const ImVec2 &origin) const;
    int hitTestColliderPoint(GameObject &object, Collider2D &collider, const ImVec2 &mouse,
                             const ImVec2 &origin) const;
    void applyColliderPointDrag(Collider2D &collider, int index, const Math::Vec2 &localPoint);

    void drawSpriteBatchEntries(ImDrawList &drawList, GameObject &object, SpriteBatch &batch, const ImVec2 &origin) const;
    void drawCameraGizmo(ImDrawList &drawList, CameraComponent &camera, const ImVec2 &origin) const;
    int hitTestBatchEntry(GameObject &object, SpriteBatch &batch, const ImVec2 &mouse, const ImVec2 &origin) const;

    void ensureFramebuffer(int width, int height);
    void destroyFramebuffer();
    void renderScene(int width, int height);

    ImVec2 mPan = ImVec2(0.0f, 0.0f);
    float mZoom = 1.0f;
    int mTool = 0;
    bool mSnap = false;
    bool mShowGrid = true;
    Math::Vec2 mGridSize = Math::Vec2(32.0f, 32.0f);
    bool mLivePreview = true;
    int mGizmoAxis = -1;
    Math::Vec2 mGizmoStartPosition = Math::Vec2(0.0f);
    ImVec2 mGizmoStartMouse = ImVec2(0.0f, 0.0f);
    Bone2D *mDraggedBone = nullptr;
    float mDraggedBoneStartRotation = 0.0f;
    bool mDraggedBoneMoved = false;

    Collider2D *mDraggedPointCollider = nullptr;
    int mDraggedPointIndex = -1;
    ct::Vector<Math::Vec2> mPointDragScratch;

    SpriteBatch *mDraggedBatch = nullptr;
    int mDraggedBatchIndex = -1;

    CanvasRenderer mCanvas;
    bool mCanvasInitialized = false;
    bool mCanvasReady = false;
    unsigned int mFramebuffer = 0;
    unsigned int mColorTexture = 0;
    int mFramebufferWidth = 0;
    int mFramebufferHeight = 0;
};

}
