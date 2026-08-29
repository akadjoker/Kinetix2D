#include "SceneViewportPanel.h"

#include "core/EditorApplication.h"
#include "core/EditorFileSystem.h"
#include "panels/AssetsPanel.h"
#include "widgets/EditorToolbar.h"

#include <k2d/Animation2D.h>
#include <k2d/Bone2D.h>
#include <k2d/BoxCollider2D.h>
#include <k2d/ChainCollider2D.h>
#include <k2d/CircleCollider2D.h>
#include <k2d/EdgeCollider2D.h>
#include <k2d/PolygonCollider2D.h>
#include <k2d/RigidBody2D.h>
#include <k2d/Assets.h>
#include <k2d/Camera2D.h>
#include <k2d/CameraComponent.h>
#include <k2d/NavigationRegion2D.h>
#include <k2d/GameObject.h>
#include <k2d/ParticleComponent.h>
#include <k2d/Prefab.h>
#include <k2d/Scene.h>
#include <k2d/Skeleton2D.h>
#include <k2d/SpriteBatch.h>
#include <IconsMaterialDesignIcons.h>

#include <cstdint>
#include <math.h>

namespace k2d::editor
{

namespace
{
float maxValue(float a, float b)
{
    return a > b ? a : b;
}
float clampValue(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

float distanceToSegment(const ImVec2& p, const ImVec2& a, const ImVec2& b)
{
    const ImVec2 ab(b.x - a.x, b.y - a.y);
    const float lengthSq = ab.x * ab.x + ab.y * ab.y;
    float t = lengthSq > 0.0001f ? ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / lengthSq : 0.0f;
    t = clampValue(t, 0.0f, 1.0f);
    const ImVec2 closest(a.x + ab.x * t, a.y + ab.y * t);
    const float dx = p.x - closest.x;
    const float dy = p.y - closest.y;
    return sqrtf(dx * dx + dy * dy);
}

constexpr float kGizmoAxisLength = 60.0f;
constexpr float kGizmoHitRadius = 8.0f;
constexpr float kGizmoCenterRadius = 8.0f;
constexpr float kGizmoRingRadius = 46.0f;
constexpr float kPointHandleHalfSize = 5.5f;
constexpr float kPointHandleHitRadius = 9.0f;
constexpr float kBatchHandleHalfSize = 4.5f;
constexpr float kBatchHandleHitRadius = 8.0f;
constexpr float kDegToRad = 0.01745329251f;

int colliderPointCount(const Collider2D &collider)
{
    if (const ChainCollider2D *chain = dynamic_cast<const ChainCollider2D *>(&collider))
        return static_cast<int>(chain->points().size());
    if (const PolygonCollider2D *polygon = dynamic_cast<const PolygonCollider2D *>(&collider))
        return static_cast<int>(polygon->points().size());
    if (dynamic_cast<const EdgeCollider2D *>(&collider))
        return 2;
    return 0;
}

Math::Vec2 colliderPointAt(const Collider2D &collider, int index)
{
    if (const ChainCollider2D *chain = dynamic_cast<const ChainCollider2D *>(&collider))
        return chain->points()[static_cast<size_t>(index)];
    if (const PolygonCollider2D *polygon = dynamic_cast<const PolygonCollider2D *>(&collider))
        return polygon->points()[static_cast<size_t>(index)];
    if (const EdgeCollider2D *edge = dynamic_cast<const EdgeCollider2D *>(&collider))
        return index == 0 ? edge->start() : edge->end();
    return Math::Vec2(0.0f, 0.0f);
}

Skeleton2D *skeletonFor(GameObject *object)
{
    for (GameObject *current = object; current; current = current->parent())
        if (Skeleton2D *skeleton = current->getComponent<Skeleton2D>())
            return skeleton;
    return nullptr;
}
} // namespace

SceneViewportPanel::SceneViewportPanel(EditorApplication& application) : EditorPanel("Scene", application)
{
    const EditorSettings& settings = application.settings();
    mPan = ImVec2(settings.viewportPan.x, settings.viewportPan.y);
    mZoom = settings.viewportZoom;
    mTool = settings.viewportTool;
    mSnap = settings.viewportSnap;
    mShowGrid = settings.viewportShowGrid;
    mGridSize = settings.viewportGridSize;
    mLivePreview = settings.viewportLivePreview;

    mCanvasInitialized = mCanvas.Init();
}

SceneViewportPanel::~SceneViewportPanel()
{
    destroyFramebuffer();
    if (mCanvasInitialized)
        mCanvas.Shutdown();
}

void SceneViewportPanel::ensureFramebuffer(int width, int height)
{
    if (width < 1)
        width = 1;
    if (height < 1)
        height = 1;
    if (mFramebuffer != 0 && width == mFramebufferWidth && height == mFramebufferHeight)
        return;

    destroyFramebuffer();

    glGenTextures(1, &mColorTexture);
    glBindTexture(GL_TEXTURE_2D, mColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &mFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTexture, 0);
    mCanvasReady = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    mFramebufferWidth = width;
    mFramebufferHeight = height;
}

void SceneViewportPanel::destroyFramebuffer()
{
    if (mFramebuffer)
    {
        glDeleteFramebuffers(1, &mFramebuffer);
        mFramebuffer = 0;
    }
    if (mColorTexture)
    {
        glDeleteTextures(1, &mColorTexture);
        mColorTexture = 0;
    }
    mCanvasReady = false;
    mFramebufferWidth = 0;
    mFramebufferHeight = 0;
}

void SceneViewportPanel::renderScene(int width, int height)
{
    if (!mCanvasInitialized || !mCanvasReady)
        return;

    GLint savedViewport[4];
    glGetIntegerv(GL_VIEWPORT, savedViewport);
    GLint savedFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFbo);

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glViewport(0, 0, width, height);
    glClearColor(0.098f, 0.110f, 0.133f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    Camera2D camera;
    camera.zoom = Math::Vec2(mZoom, mZoom);
    camera.offset = Math::Vec2(-mPan.x / mZoom, -mPan.y / mZoom);
    mCanvas.SetProjection(camera.Projection(static_cast<float>(width), static_cast<float>(height)));
    Scene& scene = app().scene();
    scene.setRenderCamera(&camera, static_cast<float>(width), static_cast<float>(height));
    scene.render(mCanvas);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(savedFbo));
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
}

ImVec2 SceneViewportPanel::worldToScreen(float x, float y, const ImVec2& origin) const
{
    return ImVec2(origin.x + mPan.x + x * mZoom, origin.y + mPan.y + y * mZoom);
}

Math::Vec2 SceneViewportPanel::screenToWorld(const ImVec2& screen, const ImVec2& origin) const
{
    return Math::Vec2((screen.x - origin.x - mPan.x) / mZoom, (screen.y - origin.y - mPan.y) / mZoom);
}

ImVec2 SceneViewportPanel::objectLocalToScreen(GameObject& object, const Math::Vec2& local, const ImVec2& origin) const
{
    const Math::Vec2 world = object.globalPosition();
    const Math::Vec2 scale = object.scale();
    const float scaleX = fabsf(scale.x) > 0.0001f ? fabsf(scale.x) : 1.0f;
    const float scaleY = fabsf(scale.y) > 0.0001f ? fabsf(scale.y) : 1.0f;
    const float angle = object.rotationDegrees() * kDegToRad;
    const float cosA = cosf(angle);
    const float sinA = sinf(angle);
    const float sx = local.x * scaleX;
    const float sy = local.y * scaleY;
    return worldToScreen(world.x + sx * cosA - sy * sinA, world.y + sx * sinA + sy * cosA, origin);
}

Math::Vec2 SceneViewportPanel::screenToObjectLocal(GameObject& object, const ImVec2& screen, const ImVec2& origin) const
{
    const Math::Vec2 world = screenToWorld(screen, origin);
    const Math::Vec2 objectWorld = object.globalPosition();
    const float dx = world.x - objectWorld.x;
    const float dy = world.y - objectWorld.y;
    const float angle = object.rotationDegrees() * kDegToRad;
    const float cosA = cosf(angle);
    const float sinA = sinf(angle);
    const float rx = dx * cosA + dy * sinA;
    const float ry = -dx * sinA + dy * cosA;
    const Math::Vec2 scale = object.scale();
    const float scaleX = fabsf(scale.x) > 0.0001f ? fabsf(scale.x) : 1.0f;
    const float scaleY = fabsf(scale.y) > 0.0001f ? fabsf(scale.y) : 1.0f;
    return Math::Vec2(rx / scaleX, ry / scaleY);
}

int SceneViewportPanel::hitTestColliderPoint(GameObject& object, Collider2D& collider, const ImVec2& mouse,
                                             const ImVec2& origin) const
{
    const Math::Vec2 offset = collider.offset();
    const int count = colliderPointCount(collider);
    for (int p = 0; p < count; ++p)
    {
        const Math::Vec2 raw = colliderPointAt(collider, p);
        const ImVec2 screen = objectLocalToScreen(object, Math::Vec2(raw.x + offset.x, raw.y + offset.y), origin);
        const float dx = mouse.x - screen.x;
        const float dy = mouse.y - screen.y;
        if (dx * dx + dy * dy <= kPointHandleHitRadius * kPointHandleHitRadius)
            return p;
    }
    return -1;
}

void SceneViewportPanel::applyColliderPointDrag(Collider2D& collider, int index, const Math::Vec2& localPoint)
{
    if (ChainCollider2D* chain = dynamic_cast<ChainCollider2D*>(&collider))
    {
        mPointDragScratch.resize(chain->points().size());
        for (size_t i = 0; i < mPointDragScratch.size(); ++i)
            mPointDragScratch[i] = chain->points()[i];
        mPointDragScratch[static_cast<size_t>(index)] = localPoint;
        chain->setPoints(mPointDragScratch.data(), static_cast<int>(mPointDragScratch.size()));
    }
    else if (PolygonCollider2D* polygon = dynamic_cast<PolygonCollider2D*>(&collider))
    {
        mPointDragScratch.resize(polygon->points().size());
        for (size_t i = 0; i < mPointDragScratch.size(); ++i)
            mPointDragScratch[i] = polygon->points()[i];
        mPointDragScratch[static_cast<size_t>(index)] = localPoint;
        polygon->setPoints(mPointDragScratch.data(), static_cast<int>(mPointDragScratch.size()));
    }
    else if (EdgeCollider2D* edge = dynamic_cast<EdgeCollider2D*>(&collider))
    {
        if (index == 0)
            edge->setPoints(localPoint, edge->end());
        else
            edge->setPoints(edge->start(), localPoint);
    }
}

int SceneViewportPanel::hitTestColliderSegment(GameObject& object, Collider2D& collider, const ImVec2& mouse,
                                               const ImVec2& origin, Math::Vec2& outLocalPoint) const
{
    const int count = colliderPointCount(collider);
    if (count < 2)
        return -1;

    const bool closed = dynamic_cast<const PolygonCollider2D*>(&collider) != nullptr;
    const ChainCollider2D* chain = dynamic_cast<const ChainCollider2D*>(&collider);
    const int segments = (closed || (chain && chain->loop())) ? count : count - 1;

    const Math::Vec2 offset = collider.offset();
    int best = -1;
    float bestDistanceSq = kPointHandleHitRadius * kPointHandleHitRadius;

    for (int i = 0; i < segments; ++i)
    {
        const Math::Vec2 rawA = colliderPointAt(collider, i);
        const Math::Vec2 rawB = colliderPointAt(collider, (i + 1) % count);
        const ImVec2 a = objectLocalToScreen(object, Math::Vec2(rawA.x + offset.x, rawA.y + offset.y), origin);
        const ImVec2 b = objectLocalToScreen(object, Math::Vec2(rawB.x + offset.x, rawB.y + offset.y), origin);

        const float abx = b.x - a.x;
        const float aby = b.y - a.y;
        const float lengthSq = abx * abx + aby * aby;
        if (lengthSq < 0.0001f)
            continue;
        float t = ((mouse.x - a.x) * abx + (mouse.y - a.y) * aby) / lengthSq;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        const float cx = a.x + abx * t;
        const float cy = a.y + aby * t;
        const float dx = mouse.x - cx;
        const float dy = mouse.y - cy;
        const float distanceSq = dx * dx + dy * dy;
        if (distanceSq <= bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            best = i;
            // Insert exactly where the line was clicked, not at the midpoint,
            // so the shape does not visibly shift when a point is added.
            const Math::Vec2 local = screenToObjectLocal(object, ImVec2(cx, cy), origin);
            outLocalPoint = Math::Vec2(local.x - offset.x, local.y - offset.y);
        }
    }
    return best;
}

bool SceneViewportPanel::insertColliderPoint(Collider2D& collider, int afterIndex, const Math::Vec2& localPoint)
{
    const int count = colliderPointCount(collider);
    if (afterIndex < 0 || count < 2)
        return false;

    mPointDragScratch.clear();
    mPointDragScratch.reserve(static_cast<size_t>(count) + 1);
    for (int i = 0; i < count; ++i)
    {
        mPointDragScratch.push_back(colliderPointAt(collider, i));
        if (i == afterIndex)
            mPointDragScratch.push_back(localPoint);
    }

    if (ChainCollider2D* chain = dynamic_cast<ChainCollider2D*>(&collider))
    {
        chain->setPoints(mPointDragScratch.data(), static_cast<int>(mPointDragScratch.size()));
        return true;
    }
    if (PolygonCollider2D* polygon = dynamic_cast<PolygonCollider2D*>(&collider))
    {
        polygon->setPoints(mPointDragScratch.data(), static_cast<int>(mPointDragScratch.size()));
        return true;
    }
    return false;
}

bool SceneViewportPanel::removeColliderPoint(Collider2D& collider, int index)
{
    const int count = colliderPointCount(collider);
    if (index < 0 || index >= count)
        return false;

    // A chain needs two points to be a line and a polygon three to have area;
    // dropping below that silently produces a collider that adds no shape.
    const int minimum = dynamic_cast<PolygonCollider2D*>(&collider) ? 3 : 2;
    if (count <= minimum)
        return false;

    mPointDragScratch.clear();
    mPointDragScratch.reserve(static_cast<size_t>(count) - 1);
    for (int i = 0; i < count; ++i)
        if (i != index)
            mPointDragScratch.push_back(colliderPointAt(collider, i));

    if (ChainCollider2D* chain = dynamic_cast<ChainCollider2D*>(&collider))
    {
        chain->setPoints(mPointDragScratch.data(), static_cast<int>(mPointDragScratch.size()));
        return true;
    }
    if (PolygonCollider2D* polygon = dynamic_cast<PolygonCollider2D*>(&collider))
    {
        polygon->setPoints(mPointDragScratch.data(), static_cast<int>(mPointDragScratch.size()));
        return true;
    }
    return false;
}

int SceneViewportPanel::hitTestBatchEntry(GameObject& object, SpriteBatch& batch, const ImVec2& mouse,
                                          const ImVec2& origin) const
{
    const int count = batch.count();
    for (int i = 0; i < count; ++i)
    {
        const SpriteBatch::Entry* entry = batch.entry(i);
        if (!entry)
            continue;
        const Math::Vec2 center(entry->position.x + entry->size.x * 0.5f, entry->position.y + entry->size.y * 0.5f);
        const ImVec2 screen = objectLocalToScreen(object, center, origin);
        const float dx = mouse.x - screen.x;
        const float dy = mouse.y - screen.y;
        if (dx * dx + dy * dy <= kBatchHandleHitRadius * kBatchHandleHitRadius)
            return i;
    }
    return -1;
}

void SceneViewportPanel::drawSpriteBatchEntries(ImDrawList& drawList, GameObject& object, SpriteBatch& batch,
                                                const ImVec2& origin) const
{
    const int count = batch.count();
    const ImVec2 mousePos = ImGui::GetIO().MousePos;
    const Math::Vec2 localMouse = screenToObjectLocal(object, mousePos, origin);

    for (int i = 0; i < count; ++i)
    {
        const SpriteBatch::Entry* entry = batch.entry(i);
        if (!entry)
            continue;

        const ImVec2 corners[4] = {
            objectLocalToScreen(object, Math::Vec2(entry->position.x, entry->position.y), origin),
            objectLocalToScreen(object, Math::Vec2(entry->position.x + entry->size.x, entry->position.y), origin),
            objectLocalToScreen(object, Math::Vec2(entry->position.x + entry->size.x, entry->position.y + entry->size.y),
                                origin),
            objectLocalToScreen(object, Math::Vec2(entry->position.x, entry->position.y + entry->size.y), origin)};

        const bool rectHovered = localMouse.x >= entry->position.x && localMouse.x <= entry->position.x + entry->size.x &&
                                 localMouse.y >= entry->position.y && localMouse.y <= entry->position.y + entry->size.y;

        const Math::Vec2 center(entry->position.x + entry->size.x * 0.5f, entry->position.y + entry->size.y * 0.5f);
        const ImVec2 handle = objectLocalToScreen(object, center, origin);
        const float hdx = mousePos.x - handle.x;
        const float hdy = mousePos.y - handle.y;
        const bool handleHovered = (hdx * hdx + hdy * hdy) <= kBatchHandleHitRadius * kBatchHandleHitRadius;
        const bool dragging = mDraggedBatch == &batch && mDraggedBatchIndex == i;

        const ImU32 outlineColor = (dragging || handleHovered || rectHovered) ? IM_COL32(255, 235, 120, 220)
                                                                              : IM_COL32(160, 200, 255, 190);
        drawList.AddPolyline(corners, 4, outlineColor, ImDrawFlags_Closed, 1.4f);

        const ImU32 handleColor = dragging   ? IM_COL32(255, 235, 120, 255)
                                  : handleHovered ? IM_COL32(255, 255, 255, 255)
                                                  : IM_COL32(255, 205, 65, 220);
        drawList.AddRectFilled(ImVec2(handle.x - kBatchHandleHalfSize, handle.y - kBatchHandleHalfSize),
                               ImVec2(handle.x + kBatchHandleHalfSize, handle.y + kBatchHandleHalfSize), handleColor);
    }
}

void SceneViewportPanel::handlePrefabDrop(const ImVec2& origin)
{
    if (!ImGui::BeginDragDropTarget())
        return;

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kPrefabDragDropPayload))
    {
        const char* path = static_cast<const char*>(payload->Data);
        Prefab prefab;
        if (!prefab.Load(path))
        {
            app().log("Prefab drop failed: could not load file");
            app().toasts().error("Could not load prefab");
        }
        else
        {
            app().preloadTextures(prefab.data());
            const EditorApplication::SceneChange before = app().beginChange();
            GameObject* created = prefab.Instantiate(app().scene(), &app().scene().root(), &app().assets());
            if (!created)
            {
                app().log("Prefab drop failed: could not instantiate");
                app().toasts().error("Could not instantiate prefab");
            }
            else
            {
                Math::Vec2 world = screenToWorld(ImGui::GetMousePos(), origin);
                if (mSnap)
                {
                    world.x = roundf(world.x / mGridSize.x) * mGridSize.x;
                    world.y = roundf(world.y / mGridSize.y) * mGridSize.y;
                }
                created->setPosition(world);
                app().selection().select(created);
                app().commitChange("Instantiate Prefab", before);
                ct::String message("Instantiated prefab: ");
                message += created->name();
                app().log(message);
                ct::String toast("Instantiated ");
                toast += created->name();
                app().toasts().success(toast);
            }
        }
    }
    ImGui::EndDragDropTarget();
}

void SceneViewportPanel::handleImageDrop(const ImVec2& origin)
{
    if (!ImGui::BeginDragDropTarget())
        return;

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kTextureDragDropPayload))
    {
        const char* path = static_cast<const char*>(payload->Data);
        GameObject* selected = app().selection().resolve(app().scene());
        SpriteBatch* batch = nullptr;
        if (selected && app().selection().componentId() != 0)
        {
            const size_t batchCount = selected->componentCount<SpriteBatch>();
            for (size_t i = 0; i < batchCount; ++i)
            {
                SpriteBatch* candidate = selected->getComponentAt<SpriteBatch>(i);
                if (candidate && candidate->id() == app().selection().componentId())
                {
                    batch = candidate;
                    break;
                }
            }
        }

        if (batch && selected)
        {
            Texture* texture = app().loadOrGetTexture(path);
            if (!texture)
            {
                app().log("Batch entry drop failed: could not load image");
                app().toasts().error("Could not load image");
            }
            else
            {
                Math::Vec2 localPoint = screenToObjectLocal(*selected, ImGui::GetMousePos(), origin);
                if (mSnap)
                {
                    localPoint.x = roundf(localPoint.x / mGridSize.x) * mGridSize.x;
                    localPoint.y = roundf(localPoint.y / mGridSize.y) * mGridSize.y;
                }
                const Math::Vec2 size(static_cast<float>(texture->Width()), static_cast<float>(texture->Height()));
                const Math::Vec2 position(localPoint.x - size.x * 0.5f, localPoint.y - size.y * 0.5f);

                const EditorApplication::SceneChange before = app().beginChange();
                const int index = batch->add(texture, position, size);
                batch->setSource(index, Math::Vec4(0.0f, 0.0f, size.x, size.y));
                app().commitChange("Add Batch Entry", before);

                ct::String message("Added batch entry: ");
                message += EditorFileSystem::fileName(path);
                app().log(message);
                ct::String toast("Added ");
                toast += EditorFileSystem::fileName(path);
                app().toasts().success(toast);
            }
        }
        else
        {
            Math::Vec2 world = screenToWorld(ImGui::GetMousePos(), origin);
            if (mSnap)
            {
                world.x = roundf(world.x / mGridSize.x) * mGridSize.x;
                world.y = roundf(world.y / mGridSize.y) * mGridSize.y;
            }
            app().createSpriteNodeFromImage(path, selected, &world);
        }
    }
    ImGui::EndDragDropTarget();
}

void SceneViewportPanel::recordBoneKey(GameObject &boneObject)
{
    Bone2D *bone = boneObject.getComponent<Bone2D>();
    Skeleton2D *skeleton = skeletonFor(&boneObject);
    if (!bone || !skeleton || !skeleton->currentAnimation()[0])
        return;
    BoneAnimationClip *clip = skeleton->getClip(skeleton->currentAnimation());
    if (!clip)
        return;

    const float time = skeleton->currentTime();
    const float value = boneObject.rotationDegrees() - bone->restRotationDegrees();
    for (BoneTrack &track : clip->tracks)
    {
        if (track.boneName != boneObject.name() || track.property != BoneTrack::Rotation)
            continue;
        for (BoneKeyframe &keyframe : track.keyframes)
        {
            if (fabsf(keyframe.time - time) < 0.0001f)
            {
                keyframe.value = value;
                return;
            }
        }
        track.addKeyframe(time, value);
        return;
    }

    BoneTrack track;
    track.boneName = boneObject.name();
    track.property = BoneTrack::Rotation;
    track.addKeyframe(time, value);
    clip->tracks.push_back(track);
}

void SceneViewportPanel::drawGrid(ImDrawList& drawList, const ImVec2& min, const ImVec2& max) const
{
    const float stepX = mGridSize.x * mZoom;
    const float stepY = mGridSize.y * mZoom;
    if (stepX < 6.0f && stepY < 6.0f)
        return;
    float x = fmodf(min.x + mPan.x, stepX);
    if (x < 0.0f)
        x += stepX;
    if (stepX >= 6.0f)
        for (; min.x + x < max.x; x += stepX)
            drawList.AddLine(ImVec2(min.x + x, min.y), ImVec2(min.x + x, max.y), IM_COL32(47, 52, 62, 150));
    float y = fmodf(min.y + mPan.y, stepY);
    if (y < 0.0f)
        y += stepY;
    if (stepY >= 6.0f)
        for (; min.y + y < max.y; y += stepY)
            drawList.AddLine(ImVec2(min.x, min.y + y), ImVec2(max.x, min.y + y), IM_COL32(47, 52, 62, 150));
}

void SceneViewportPanel::drawObject(ImDrawList& drawList, GameObject& object, const ImVec2& origin)
{
    const Math::Vec2 world = object.globalPosition();
    const ImVec2 point = worldToScreen(world.x, world.y, origin);
    if (object.parent())
    {
        const Math::Vec2 parentWorld = object.parent()->globalPosition();
        const ImVec2 parent = worldToScreen(parentWorld.x, parentWorld.y, origin);
        drawList.AddLine(parent, point, IM_COL32(100, 110, 125, 170), 1.0f);
    }

    const bool selected = app().selection().hasSelection() && app().selection().objectId() == object.id();
    drawList.AddCircleFilled(point, selected ? 6.0f : 4.0f,
                             selected ? IM_COL32(255, 205, 65, 255) : IM_COL32(70, 180, 235, 255));
    drawList.AddText(ImVec2(point.x + 8.0f, point.y - 8.0f), IM_COL32(220, 225, 235, 230),
                     object.name().empty() ? "GameObject" : object.name().c_str());

    for (size_t i = 0; i < object.childCount(); ++i)
        drawObject(drawList, *object.child(i), origin);
}

void SceneViewportPanel::drawColliders(ImDrawList& drawList, GameObject& object, const ImVec2& origin)
{
    const size_t count = object.componentCount<Collider2D>();
    if (count > 0)
    {
        const RigidBody2D* body = object.getComponent<RigidBody2D>();
        const Math::Vec2 world = object.globalPosition();
        const Math::Vec2 scale = object.scale();
        const float scaleX = fabsf(scale.x) > 0.0001f ? fabsf(scale.x) : 1.0f;
        const float scaleY = fabsf(scale.y) > 0.0001f ? fabsf(scale.y) : 1.0f;
        const float angle = object.rotationDegrees() * kDegToRad;
        const float cosA = cosf(angle);
        const float sinA = sinf(angle);

        const auto place = [&](float lx, float ly) -> ImVec2
        {
            const float sx = lx * scaleX;
            const float sy = ly * scaleY;
            return worldToScreen(world.x + sx * cosA - sy * sinA, world.y + sx * sinA + sy * cosA, origin);
        };

        const bool objectSelected = app().selection().hasSelection() && app().selection().objectId() == object.id();
        const uint32_t selectedComponentId = app().selection().componentId();
        const ImVec2 mousePos = ImGui::GetIO().MousePos;

        for (size_t i = 0; i < count; ++i)
        {
            Collider2D* collider = object.getComponentAt<Collider2D>(i);
            if (!collider || !collider->active())
                continue;

            const bool orphan = body == nullptr;
            ImU32 color = collider->isSensor() ? IM_COL32(90, 180, 255, 220) : IM_COL32(110, 225, 140, 220);
            if (orphan)
                color = IM_COL32(240, 150, 60, 200);

            const Math::Vec2 offset = collider->offset();

            if (const BoxCollider2D* box = dynamic_cast<const BoxCollider2D*>(collider))
            {
                const float hw = box->size().x * 0.5f;
                const float hh = box->size().y * 0.5f;
                const ImVec2 corners[4] = {place(offset.x - hw, offset.y - hh), place(offset.x + hw, offset.y - hh),
                                           place(offset.x + hw, offset.y + hh), place(offset.x - hw, offset.y + hh)};
                drawList.AddPolyline(corners, 4, color, ImDrawFlags_Closed, 1.6f);
            }
            else if (const CircleCollider2D* circle = dynamic_cast<const CircleCollider2D*>(collider))
            {
                const ImVec2 center = place(offset.x, offset.y);
                const float radius = circle->radius() * (scaleX > scaleY ? scaleX : scaleY) * mZoom;
                drawList.AddCircle(center, radius, color, 32, 1.6f);
                drawList.AddLine(center, place(offset.x + circle->radius(), offset.y), color, 1.0f);
            }
            else if (const EdgeCollider2D* edge = dynamic_cast<const EdgeCollider2D*>(collider))
            {
                drawList.AddLine(place(edge->start().x + offset.x, edge->start().y + offset.y),
                                 place(edge->end().x + offset.x, edge->end().y + offset.y), color, 1.6f);
            }
            else if (const PolygonCollider2D* polygon = dynamic_cast<const PolygonCollider2D*>(collider))
            {
                const ct::Vector<Math::Vec2>& points = polygon->points();
                for (size_t p = 0; p + 1 <= points.size() && points.size() >= 2; ++p)
                {
                    const Math::Vec2& a = points[p];
                    const Math::Vec2& b = points[(p + 1) % points.size()];
                    drawList.AddLine(place(a.x + offset.x, a.y + offset.y), place(b.x + offset.x, b.y + offset.y),
                                     color, 1.6f);
                }
            }
            else if (const ChainCollider2D* chain = dynamic_cast<const ChainCollider2D*>(collider))
            {
                const ct::Vector<Math::Vec2>& points = chain->points();
                const size_t segments = chain->loop() ? points.size() : (points.size() > 0 ? points.size() - 1 : 0);
                for (size_t p = 0; p < segments; ++p)
                {
                    const Math::Vec2& a = points[p];
                    const Math::Vec2& b = points[(p + 1) % points.size()];
                    drawList.AddLine(place(a.x + offset.x, a.y + offset.y), place(b.x + offset.x, b.y + offset.y),
                                     color, 1.6f);
                }
            }

            if (objectSelected && selectedComponentId != 0 && collider->id() == selectedComponentId)
            {
                const int pointCount = colliderPointCount(*collider);
                for (int p = 0; p < pointCount; ++p)
                {
                    const Math::Vec2 raw = colliderPointAt(*collider, p);
                    const ImVec2 screen = place(raw.x + offset.x, raw.y + offset.y);
                    const float dx = mousePos.x - screen.x;
                    const float dy = mousePos.y - screen.y;
                    const bool hovered = (dx * dx + dy * dy) <= kPointHandleHitRadius * kPointHandleHitRadius;
                    const bool dragging = mDraggedPointCollider == collider && mDraggedPointIndex == p;
                    const ImU32 handleColor = dragging   ? IM_COL32(255, 235, 120, 255)
                                              : hovered   ? IM_COL32(255, 255, 255, 255)
                                                          : IM_COL32(255, 205, 65, 220);
                    drawList.AddRectFilled(ImVec2(screen.x - kPointHandleHalfSize, screen.y - kPointHandleHalfSize),
                                           ImVec2(screen.x + kPointHandleHalfSize, screen.y + kPointHandleHalfSize),
                                           handleColor);
                }
            }
        }
    }

    for (size_t i = 0; i < object.childCount(); ++i)
        drawColliders(drawList, *object.child(i), origin);
}

int SceneViewportPanel::regionLoopCount(const NavigationRegion2D& region) const
{
    return 1 + static_cast<int>(region.holes().size());
}

int SceneViewportPanel::regionPointCount(const NavigationRegion2D& region, int loop) const
{
    if (loop == 0)
        return static_cast<int>(region.polygon().size());
    const int hole = loop - 1;
    if (hole < 0 || hole >= static_cast<int>(region.holes().size()))
        return 0;
    return static_cast<int>(region.holes()[static_cast<size_t>(hole)].size());
}

Math::Vec2 SceneViewportPanel::regionPointAt(const NavigationRegion2D& region, int loop, int index) const
{
    if (loop == 0)
        return region.polygon()[static_cast<size_t>(index)];
    return region.holes()[static_cast<size_t>(loop - 1)][static_cast<size_t>(index)];
}

bool SceneViewportPanel::hitTestRegionPoint(GameObject& object, NavigationRegion2D& region, const ImVec2& mouse,
                                            const ImVec2& origin, int& outLoop, int& outPoint) const
{
    const int loops = regionLoopCount(region);
    for (int loop = 0; loop < loops; ++loop)
    {
        const int count = regionPointCount(region, loop);
        for (int i = 0; i < count; ++i)
        {
            const ImVec2 screen = objectLocalToScreen(object, regionPointAt(region, loop, i), origin);
            const float dx = mouse.x - screen.x;
            const float dy = mouse.y - screen.y;
            if (dx * dx + dy * dy <= kPointHandleHitRadius * kPointHandleHitRadius)
            {
                outLoop = loop;
                outPoint = i;
                return true;
            }
        }
    }
    return false;
}

int SceneViewportPanel::hitTestRegionSegment(GameObject& object, NavigationRegion2D& region, const ImVec2& mouse,
                                             const ImVec2& origin, int& outLoop, Math::Vec2& outLocalPoint) const
{
    const int loops = regionLoopCount(region);
    int best = -1;
    float bestDistanceSq = kPointHandleHitRadius * kPointHandleHitRadius;

    for (int loop = 0; loop < loops; ++loop)
    {
        const int count = regionPointCount(region, loop);
        if (count < 3)
            continue;
        for (int i = 0; i < count; ++i)
        {
            const ImVec2 a = objectLocalToScreen(object, regionPointAt(region, loop, i), origin);
            const ImVec2 b = objectLocalToScreen(object, regionPointAt(region, loop, (i + 1) % count), origin);
            const float abx = b.x - a.x;
            const float aby = b.y - a.y;
            const float lengthSq = abx * abx + aby * aby;
            if (lengthSq < 0.0001f)
                continue;
            float t = ((mouse.x - a.x) * abx + (mouse.y - a.y) * aby) / lengthSq;
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
            const float cx = a.x + abx * t;
            const float cy = a.y + aby * t;
            const float dx = mouse.x - cx;
            const float dy = mouse.y - cy;
            const float distanceSq = dx * dx + dy * dy;
            if (distanceSq <= bestDistanceSq)
            {
                bestDistanceSq = distanceSq;
                best = i;
                outLoop = loop;
                outLocalPoint = screenToObjectLocal(object, ImVec2(cx, cy), origin);
            }
        }
    }
    return best;
}

void SceneViewportPanel::rebuildRegion(NavigationRegion2D& region, const ct::Vector<ct::Vector<Math::Vec2>>& loops)
{
    if (loops.empty() || loops[0].size() < 3)
        return;

    ct::Vector<const Math::Vec2*> holePointers;
    ct::Vector<int> holeCounts;
    for (size_t i = 1; i < loops.size(); ++i)
    {
        if (loops[i].size() < 3)
            continue;
        holePointers.push_back(loops[i].data());
        holeCounts.push_back(static_cast<int>(loops[i].size()));
    }
    region.setPolygonWithHoles(loops[0].data(), static_cast<int>(loops[0].size()), holePointers.data(),
                               holeCounts.data(), static_cast<int>(holePointers.size()));
}

void SceneViewportPanel::applyRegionPointDrag(NavigationRegion2D& region, int loop, int index,
                                              const Math::Vec2& localPoint)
{
    ct::Vector<ct::Vector<Math::Vec2>> loops;
    const int loopCount = regionLoopCount(region);
    for (int l = 0; l < loopCount; ++l)
    {
        ct::Vector<Math::Vec2> points;
        const int count = regionPointCount(region, l);
        for (int i = 0; i < count; ++i)
            points.push_back(l == loop && i == index ? localPoint : regionPointAt(region, l, i));
        loops.push_back(points);
    }
    rebuildRegion(region, loops);
}

bool SceneViewportPanel::insertRegionPoint(NavigationRegion2D& region, int loop, int afterIndex,
                                           const Math::Vec2& localPoint)
{
    if (afterIndex < 0)
        return false;
    ct::Vector<ct::Vector<Math::Vec2>> loops;
    const int loopCount = regionLoopCount(region);
    for (int l = 0; l < loopCount; ++l)
    {
        ct::Vector<Math::Vec2> points;
        const int count = regionPointCount(region, l);
        for (int i = 0; i < count; ++i)
        {
            points.push_back(regionPointAt(region, l, i));
            if (l == loop && i == afterIndex)
                points.push_back(localPoint);
        }
        loops.push_back(points);
    }
    rebuildRegion(region, loops);
    return true;
}

bool SceneViewportPanel::removeRegionPoint(NavigationRegion2D& region, int loop, int index)
{
    const int count = regionPointCount(region, loop);
    // Below three points a loop has no area: the outline would stop
    // triangulating and a hole would silently stop cutting anything.
    if (count <= 3)
        return false;

    ct::Vector<ct::Vector<Math::Vec2>> loops;
    const int loopCount = regionLoopCount(region);
    for (int l = 0; l < loopCount; ++l)
    {
        ct::Vector<Math::Vec2> points;
        const int c = regionPointCount(region, l);
        for (int i = 0; i < c; ++i)
            if (!(l == loop && i == index))
                points.push_back(regionPointAt(region, l, i));
        loops.push_back(points);
    }
    rebuildRegion(region, loops);
    return true;
}

void SceneViewportPanel::drawNavigationRegions(ImDrawList& drawList, GameObject& object, const ImVec2& origin)
{
    const size_t count = object.componentCount<NavigationRegion2D>();
    for (size_t i = 0; i < count; ++i)
    {
        NavigationRegion2D* region = object.getComponentAt<NavigationRegion2D>(i);
        if (!region || !region->active())
            continue;

        // Triangles are what the pathfinder actually walks, so a region that
        // failed to triangulate draws its outline but no fill - which is the
        // whole point of having this overlay.
        const ct::Vector<Math::Vec2>& triangles = region->triangles();
        for (size_t t = 0; t + 2 < triangles.size(); t += 3)
        {
            const ImVec2 a = objectLocalToScreen(object, triangles[t], origin);
            const ImVec2 b = objectLocalToScreen(object, triangles[t + 1], origin);
            const ImVec2 c = objectLocalToScreen(object, triangles[t + 2], origin);
            drawList.AddTriangleFilled(a, b, c, IM_COL32(70, 190, 255, 40));
            drawList.AddTriangle(a, b, c, IM_COL32(70, 190, 255, 90), 1.0f);
        }

        const ct::Vector<Math::Vec2>& outline = region->polygon();
        if (outline.size() >= 2)
            for (size_t p = 0; p < outline.size(); ++p)
            {
                const ImVec2 a = objectLocalToScreen(object, outline[p], origin);
                const ImVec2 b = objectLocalToScreen(object, outline[(p + 1) % outline.size()], origin);
                drawList.AddLine(a, b, IM_COL32(90, 210, 255, 230), 1.8f);
            }

        const ct::Vector<ct::Vector<Math::Vec2>>& holes = region->holes();
        for (size_t hole = 0; hole < holes.size(); ++hole)
        {
            const ct::Vector<Math::Vec2>& points = holes[hole];
            if (points.size() < 2)
                continue;
            for (size_t p = 0; p < points.size(); ++p)
            {
                const ImVec2 a = objectLocalToScreen(object, points[p], origin);
                const ImVec2 b = objectLocalToScreen(object, points[(p + 1) % points.size()], origin);
                drawList.AddLine(a, b, IM_COL32(255, 140, 70, 230), 1.8f);
            }
        }

        const bool selectedHere = app().selection().hasSelection() &&
                                  app().selection().objectId() == object.id() &&
                                  app().selection().componentId() == region->id();
        if (!selectedHere)
            continue;

        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        const int loops = regionLoopCount(*region);
        for (int loop = 0; loop < loops; ++loop)
        {
            const int pointCount = regionPointCount(*region, loop);
            for (int i = 0; i < pointCount; ++i)
            {
                const ImVec2 screen = objectLocalToScreen(object, regionPointAt(*region, loop, i), origin);
                const float dx = mousePos.x - screen.x;
                const float dy = mousePos.y - screen.y;
                const bool hovered = (dx * dx + dy * dy) <= kPointHandleHitRadius * kPointHandleHitRadius;
                const bool dragging =
                    mDraggedRegion == region && mDraggedRegionLoop == loop && mDraggedRegionPoint == i;
                const ImU32 handleColor = dragging ? IM_COL32(255, 235, 120, 255)
                                          : hovered ? IM_COL32(255, 255, 255, 255)
                                          : loop == 0 ? IM_COL32(90, 210, 255, 235)
                                                      : IM_COL32(255, 160, 80, 235);
                drawList.AddRectFilled(ImVec2(screen.x - kPointHandleHalfSize, screen.y - kPointHandleHalfSize),
                                       ImVec2(screen.x + kPointHandleHalfSize, screen.y + kPointHandleHalfSize),
                                       handleColor);
            }
        }
    }

    for (size_t i = 0; i < object.childCount(); ++i)
        drawNavigationRegions(drawList, *object.child(i), origin);
}

void SceneViewportPanel::drawCameraGizmo(ImDrawList& drawList, CameraComponent& camera, const ImVec2& origin) const
{
    float minX, minY, maxX, maxY;
    camera.visibleRect(minX, minY, maxX, maxY);
    if (maxX > minX && maxY > minY)
    {
        const ImVec2 corners[4] = {worldToScreen(minX, minY, origin), worldToScreen(maxX, minY, origin),
                                   worldToScreen(maxX, maxY, origin), worldToScreen(minX, maxY, origin)};
        drawList.AddPolyline(corners, 4, IM_COL32(90, 200, 255, 220), ImDrawFlags_Closed, 1.6f);
        drawList.AddText(ImVec2(corners[0].x + 4.0f, corners[0].y + 4.0f), IM_COL32(90, 200, 255, 220), "Viewport");
    }

    const Camera2D& camera2D = camera.camera();
    if (camera2D.limitEnabled)
    {
        const Math::Vec4& limits = camera2D.limits;
        const ImVec2 corners[4] = {
            worldToScreen(limits.x, limits.y, origin), worldToScreen(limits.z, limits.y, origin),
            worldToScreen(limits.z, limits.w, origin), worldToScreen(limits.x, limits.w, origin)};
        drawList.AddPolyline(corners, 4, IM_COL32(255, 170, 60, 220), ImDrawFlags_Closed, 1.6f);
        drawList.AddText(ImVec2(corners[0].x + 4.0f, corners[0].y - 16.0f), IM_COL32(255, 170, 60, 220), "Limits");
    }
}

void SceneViewportPanel::pickObject(GameObject& object, const ImVec2& mouse, const ImVec2& origin, GameObject*& best,
                                    float& bestDistance)
{
    const Math::Vec2 world = object.globalPosition();
    const ImVec2 point = worldToScreen(world.x, world.y, origin);
    const float dx = mouse.x - point.x;
    const float dy = mouse.y - point.y;
    const float distance = sqrtf(dx * dx + dy * dy);
    if (distance < bestDistance)
    {
        bestDistance = distance;
        best = &object;
    }
    for (size_t i = 0; i < object.childCount(); ++i)
        pickObject(*object.child(i), mouse, origin, best, bestDistance);
}

void SceneViewportPanel::drawGizmo(ImDrawList& drawList, GameObject& selected, const ImVec2& origin) const
{
    const Math::Vec2 world = selected.globalPosition();
    const ImVec2 center = worldToScreen(world.x, world.y, origin);

    if (mTool == 1 || mTool == 3)
    {
        const ImVec2 xTip(center.x + kGizmoAxisLength, center.y);
        const ImVec2 yTip(center.x, center.y + kGizmoAxisLength);
        const ImU32 xColor = mGizmoAxis == 0 ? IM_COL32(255, 235, 120, 255) : IM_COL32(230, 70, 70, 255);
        const ImU32 yColor = mGizmoAxis == 1 ? IM_COL32(255, 235, 120, 255) : IM_COL32(80, 210, 100, 255);
        const ImU32 centerColor = mGizmoAxis == 2 ? IM_COL32(255, 235, 120, 255) : IM_COL32(230, 230, 235, 255);

        drawList.AddLine(center, xTip, xColor, 2.5f);
        drawList.AddLine(center, yTip, yColor, 2.5f);

        if (mTool == 1)
        {
            drawList.AddTriangleFilled(ImVec2(xTip.x - 9.0f, xTip.y - 5.0f), ImVec2(xTip.x - 9.0f, xTip.y + 5.0f),
                                       ImVec2(xTip.x + 3.0f, xTip.y), xColor);
            drawList.AddTriangleFilled(ImVec2(yTip.x - 5.0f, yTip.y - 9.0f), ImVec2(yTip.x + 5.0f, yTip.y - 9.0f),
                                       ImVec2(yTip.x, yTip.y + 3.0f), yColor);
        }
        else
        {
            drawList.AddRectFilled(ImVec2(xTip.x - 4.0f, xTip.y - 4.0f), ImVec2(xTip.x + 4.0f, xTip.y + 4.0f), xColor);
            drawList.AddRectFilled(ImVec2(yTip.x - 4.0f, yTip.y - 4.0f), ImVec2(yTip.x + 4.0f, yTip.y + 4.0f), yColor);
        }

        drawList.AddCircleFilled(center, kGizmoCenterRadius * 0.5f, centerColor);
    }
    else if (mTool == 2)
    {
        const ImU32 ringColor = mGizmoAxis == 0 ? IM_COL32(255, 235, 120, 255) : IM_COL32(120, 170, 230, 255);
        drawList.AddCircle(center, kGizmoRingRadius, ringColor, 48, 2.0f);
    }
}

int SceneViewportPanel::hitTestGizmo(GameObject& selected, const ImVec2& mouse, const ImVec2& origin) const
{
    const Math::Vec2 world = selected.globalPosition();
    const ImVec2 center = worldToScreen(world.x, world.y, origin);

    if (mTool == 1 || mTool == 3)
    {
        const float dx = mouse.x - center.x;
        const float dy = mouse.y - center.y;
        if (sqrtf(dx * dx + dy * dy) <= kGizmoCenterRadius)
            return 2;
        const ImVec2 xTip(center.x + kGizmoAxisLength, center.y);
        if (distanceToSegment(mouse, center, xTip) <= kGizmoHitRadius)
            return 0;
        const ImVec2 yTip(center.x, center.y + kGizmoAxisLength);
        if (distanceToSegment(mouse, center, yTip) <= kGizmoHitRadius)
            return 1;
        return -1;
    }
    if (mTool == 2)
    {
        const float dx = mouse.x - center.x;
        const float dy = mouse.y - center.y;
        const float distance = sqrtf(dx * dx + dy * dy);
        return fabsf(distance - kGizmoRingRadius) <= kGizmoHitRadius ? 0 : -1;
    }
    return -1;
}

void SceneViewportPanel::drawContents()
{
    app().settings().viewportPan = Math::Vec2(mPan.x, mPan.y);
    app().settings().viewportZoom = mZoom;
    app().settings().viewportTool = mTool;
    app().settings().viewportSnap = mSnap;
    app().settings().viewportShowGrid = mShowGrid;
    mGridSize = app().settings().viewportGridSize;
    app().settings().viewportLivePreview = mLivePreview;

    const char* toolIcons[] = {ICON_MDI_CURSOR_DEFAULT, ICON_MDI_ARROW_ALL, ICON_MDI_ROTATE_ORBIT,
                               ICON_MDI_ARROW_EXPAND_ALL, ICON_MDI_HAND};
    const char* tooltips[] = {"Select", "Move", "Rotate", "Scale", "Pan"};
    for (int tool = 0; tool < 5; ++tool)
    {
        if (tool != 0)
            toolbarSameLine();
        ImGui::PushID(tool);
        if (toolbarIcon("tool", toolIcons[tool], tooltips[tool], mTool == tool) && mTool != tool)
        {
            mTool = tool;
            ct::String message("Scene tool: ");
            message += tooltips[tool];
            app().log(message);
        }
        ImGui::PopID();
    }
    toolbarDivider();
    if (toolbarIcon("snap", ICON_MDI_MAGNET, "Snap to the Scene grid", mSnap))
    {
        mSnap = !mSnap;
        app().log(mSnap ? "Snap enabled" : "Snap disabled");
    }
    toolbarSameLine();
    if (toolbarIcon("grid", ICON_MDI_GRID, "Show grid", mShowGrid))
    {
        mShowGrid = !mShowGrid;
        app().log(mShowGrid ? "Grid shown" : "Grid hidden");
    }
    toolbarSameLine();
    if (toolbarIcon("livepreview", ICON_MDI_ANIMATION_PLAY_OUTLINE,
                    "Live preview (animates particles/animations while editing)", mLivePreview))
    {
        mLivePreview = !mLivePreview;
        app().log(mLivePreview ? "Live preview enabled" : "Live preview paused");
    }
    toolbarSameLine();
    if (toolbarIcon("restartpreview", ICON_MDI_RESTART, "Restart particle/animation preview"))
    {
        app().restartEditPreview();
        app().log("Preview restarted");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%.0f%%", mZoom * 100.0f);
    ImGui::Separator();

    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 size = ImGui::GetContentRegionAvail();
    const ImVec2 max(min.x + maxValue(size.x, 1.0f), min.y + maxValue(size.y, 1.0f));

    const int fboWidth = static_cast<int>(max.x - min.x);
    const int fboHeight = static_cast<int>(max.y - min.y);
    ensureFramebuffer(fboWidth, fboHeight);
    renderScene(fboWidth, fboHeight);

    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    if (mCanvasReady)
        drawList.AddImage((ImTextureID)(intptr_t)mColorTexture, min, max, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    else
        drawList.AddRectFilled(min, max, IM_COL32(25, 28, 34, 255));
    drawList.PushClipRect(min, max, true);
    if (mShowGrid)
        drawGrid(drawList, min, max);

    const ImVec2 origin((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    const ImVec2 axis = worldToScreen(0.0f, 0.0f, origin);
    drawList.AddLine(ImVec2(min.x, axis.y), ImVec2(max.x, axis.y), IM_COL32(150, 60, 60, 180));
    drawList.AddLine(ImVec2(axis.x, min.y), ImVec2(axis.x, max.y), IM_COL32(60, 150, 80, 180));
    drawObject(drawList, app().scene().root(), origin);
    GameObject* selected = app().selection().resolve(app().scene());
    // The overlay is a debug toggle, but the point handles are how a selected
    // collider is edited, so the selected object's colliders draw either way.
    if (app().settings().showColliders)
    {
        drawColliders(drawList, app().scene().root(), origin);
        drawNavigationRegions(drawList, app().scene().root(), origin);
    }
    else if (selected && app().selection().componentId() != 0)
        drawColliders(drawList, *selected, origin);

    const bool gizmoActive = selected && selected != &app().scene().root() && !selected->locked() && mTool >= 1 &&
                             mTool <= 3 && !mDraggedPointCollider && !mDraggedBatch && !mDraggedRegion;
    if (gizmoActive)
        drawGizmo(drawList, *selected, origin);

    Collider2D* selectedCollider = nullptr;
    SpriteBatch* selectedBatch = nullptr;
    if (selected && app().selection().componentId() != 0)
    {
        const size_t colliderCount = selected->componentCount<Collider2D>();
        for (size_t i = 0; i < colliderCount; ++i)
        {
            Collider2D* candidate = selected->getComponentAt<Collider2D>(i);
            if (candidate && candidate->id() == app().selection().componentId())
            {
                selectedCollider = candidate;
                break;
            }
        }

        const size_t batchCount = selected->componentCount<SpriteBatch>();
        for (size_t i = 0; i < batchCount; ++i)
        {
            SpriteBatch* candidate = selected->getComponentAt<SpriteBatch>(i);
            if (candidate && candidate->id() == app().selection().componentId())
            {
                selectedBatch = candidate;
                break;
            }
        }
    }
    NavigationRegion2D* selectedRegion = nullptr;
    if (selected && app().selection().componentId() != 0)
    {
        const size_t regionCount = selected->componentCount<NavigationRegion2D>();
        for (size_t i = 0; i < regionCount; ++i)
        {
            NavigationRegion2D* candidate = selected->getComponentAt<NavigationRegion2D>(i);
            if (candidate && candidate->id() == app().selection().componentId())
            {
                selectedRegion = candidate;
                break;
            }
        }
    }
    // The region's own overlay only runs behind the debug toggle, so draw the
    // selected one here too or its handles vanish with that checkbox.
    if (selectedRegion && !app().settings().showColliders)
        drawNavigationRegions(drawList, *selected, origin);

    if (selectedBatch)
        drawSpriteBatchEntries(drawList, *selected, *selectedBatch, origin);
    if (selected)
        if (CameraComponent* selectedCamera = selected->getComponent<CameraComponent>())
            drawCameraGizmo(drawList, *selectedCamera, origin);
    drawList.PopClipRect();

    ImGui::InvisibleButton("##scene_canvas", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle |
                               ImGuiButtonFlags_MouseButtonRight);
    handlePrefabDrop(origin);
    handleImageDrop(origin);
    const bool hovered = ImGui::IsItemHovered();
    const bool panDrag = ImGui::IsMouseDragging(ImGuiMouseButton_Middle) ||
                         (mTool == 4 && ImGui::IsMouseDragging(ImGuiMouseButton_Left));
    if (hovered && panDrag)
    {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        mPan.x += delta.x;
        mPan.y += delta.y;
    }
    if (hovered && ImGui::GetIO().MouseWheel != 0.0f)
        mZoom = clampValue(mZoom * (1.0f + ImGui::GetIO().MouseWheel * 0.1f), 0.1f, 8.0f);
    if (hovered && selectedRegion && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        int loop = -1;
        int point = -1;
        if (hitTestRegionPoint(*selected, *selectedRegion, ImGui::GetIO().MousePos, origin, loop, point))
        {
            const EditorApplication::SceneChange before = app().beginChange();
            if (removeRegionPoint(*selectedRegion, loop, point))
                app().commitChange("Remove Navigation Point", before);
            else
                app().toasts().info("A navigation loop needs at least three points");
        }
        else
        {
            Math::Vec2 localPoint(0.0f, 0.0f);
            int segmentLoop = -1;
            const int segment = hitTestRegionSegment(*selected, *selectedRegion, ImGui::GetIO().MousePos, origin,
                                                     segmentLoop, localPoint);
            if (segment != -1)
            {
                const EditorApplication::SceneChange before = app().beginChange();
                if (insertRegionPoint(*selectedRegion, segmentLoop, segment, localPoint))
                    app().commitChange("Insert Navigation Point", before);
            }
        }
    }

    if (hovered && selectedCollider && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        const int hitPoint =
            hitTestColliderPoint(*selected, *selectedCollider, ImGui::GetIO().MousePos, origin);
        if (hitPoint != -1)
        {
            const EditorApplication::SceneChange before = app().beginChange();
            if (removeColliderPoint(*selectedCollider, hitPoint))
                app().commitChange("Remove Collider Point", before);
            else
                app().toasts().info("A collider needs its remaining points");
        }
        else
        {
            Math::Vec2 localPoint(0.0f, 0.0f);
            const int segment =
                hitTestColliderSegment(*selected, *selectedCollider, ImGui::GetIO().MousePos, origin, localPoint);
            if (segment != -1)
            {
                const EditorApplication::SceneChange before = app().beginChange();
                if (insertColliderPoint(*selectedCollider, segment, localPoint))
                    app().commitChange("Insert Collider Point", before);
            }
        }
    }

    if (hovered && mTool != 4 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        int regionLoop = -1;
        int regionPoint = -1;
        const bool hitRegion =
            selectedRegion &&
            hitTestRegionPoint(*selected, *selectedRegion, ImGui::GetIO().MousePos, origin, regionLoop, regionPoint);
        const int hitPoint = (!hitRegion && selectedCollider)
                                 ? hitTestColliderPoint(*selected, *selectedCollider, ImGui::GetIO().MousePos, origin)
                                 : -1;
        const int hitBatchEntry = (!hitRegion && hitPoint == -1 && selectedBatch)
                                     ? hitTestBatchEntry(*selected, *selectedBatch, ImGui::GetIO().MousePos, origin)
                                     : -1;
        const int hitAxis = (!hitRegion && hitPoint == -1 && hitBatchEntry == -1 && gizmoActive)
                               ? hitTestGizmo(*selected, ImGui::GetIO().MousePos, origin)
                               : -1;
        if (hitRegion)
        {
            mDraggedRegion = selectedRegion;
            mDraggedRegionLoop = regionLoop;
            mDraggedRegionPoint = regionPoint;
            app().beginTransaction("Move Navigation Point", app().beginChange());
        }
        else if (hitPoint != -1)
        {
            mDraggedPointCollider = selectedCollider;
            mDraggedPointIndex = hitPoint;
            app().beginTransaction("Move Collider Point", app().beginChange());
        }
        else if (hitBatchEntry != -1)
        {
            mDraggedBatch = selectedBatch;
            mDraggedBatchIndex = hitBatchEntry;
            app().beginTransaction("Move Batch Entry", app().beginChange());
        }
        else if (hitAxis != -1)
        {
            mGizmoAxis = hitAxis;
            mGizmoStartPosition = selected->position();
            mGizmoStartMouse = ImGui::GetIO().MousePos;
            const char* label = mTool == 1 ? "Move GameObject" : mTool == 2 ? "Rotate GameObject" : "Scale GameObject";
            app().beginTransaction(label, app().beginChange());
        }
        else
        {
            mGizmoAxis = -1;
            GameObject* best = nullptr;
            float bestDistance = 14.0f;
            GameObject& pickRoot = app().scene().root();
            for (size_t i = 0; i < pickRoot.childCount(); ++i)
                pickObject(*pickRoot.child(i), ImGui::GetIO().MousePos, origin, best, bestDistance);
            app().selection().select(best);
            mDraggedBone = best ? best->getComponent<Bone2D>() : nullptr;
            mDraggedBoneMoved = false;
            if (mDraggedBone)
            {
                mDraggedBoneStartRotation = best->rotationDegrees();
                app().beginTransaction("Pose Bone", app().beginChange());
            }
            if (best)
            {
                ct::String message("Selected in Scene: ");
                message += best->name();
                app().log(message);
            }
            else
            {
                app().log("Scene selection cleared");
            }
        }
    }

    if (hovered && gizmoActive && mGizmoAxis != -1 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (mTool == 1)
        {
            const ImVec2 totalDelta(ImGui::GetIO().MousePos.x - mGizmoStartMouse.x,
                                    ImGui::GetIO().MousePos.y - mGizmoStartMouse.y);
            Math::Vec2 position = mGizmoStartPosition;
            if (mGizmoAxis == 0 || mGizmoAxis == 2)
                position.x += totalDelta.x / mZoom;
            if (mGizmoAxis == 1 || mGizmoAxis == 2)
                position.y += totalDelta.y / mZoom;
            if (mSnap)
            {
                if ((mGizmoAxis == 0 || mGizmoAxis == 2) && mGridSize.x > 0.0f)
                    position.x = roundf(position.x / mGridSize.x) * mGridSize.x;
                if ((mGizmoAxis == 1 || mGizmoAxis == 2) && mGridSize.y > 0.0f)
                    position.y = roundf(position.y / mGridSize.y) * mGridSize.y;
            }
            selected->setPosition(position);
        }
        else if (mTool == 2)
        {
            selected->setRotationDegrees(selected->rotationDegrees() + delta.x * 0.5f);
        }
        else if (mTool == 3)
        {
            Math::Vec2 scale = selected->scale();
            if (mGizmoAxis == 0 || mGizmoAxis == 2)
                scale.x = maxValue(0.01f, scale.x + delta.x * 0.01f);
            if (mGizmoAxis == 1 || mGizmoAxis == 2)
                scale.y = maxValue(0.01f, scale.y + (mGizmoAxis == 2 ? delta.x : delta.y) * 0.01f);
            selected->setScale(scale);
        }
    }

    if (hovered && mDraggedPointCollider && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        GameObject* pointObject = mDraggedPointCollider->owner();
        if (pointObject)
        {
            Math::Vec2 local = screenToObjectLocal(*pointObject, ImGui::GetIO().MousePos, origin);
            const Math::Vec2 offset = mDraggedPointCollider->offset();
            local.x -= offset.x;
            local.y -= offset.y;
            if (mSnap)
            {
                if (mGridSize.x > 0.0f)
                    local.x = roundf(local.x / mGridSize.x) * mGridSize.x;
                if (mGridSize.y > 0.0f)
                    local.y = roundf(local.y / mGridSize.y) * mGridSize.y;
            }
            applyColliderPointDrag(*mDraggedPointCollider, mDraggedPointIndex, local);
        }
    }

    if (hovered && mDraggedRegion && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        GameObject* regionObject = mDraggedRegion->owner();
        if (regionObject)
        {
            Math::Vec2 local = screenToObjectLocal(*regionObject, ImGui::GetIO().MousePos, origin);
            if (mSnap)
            {
                if (mGridSize.x > 0.0f)
                    local.x = roundf(local.x / mGridSize.x) * mGridSize.x;
                if (mGridSize.y > 0.0f)
                    local.y = roundf(local.y / mGridSize.y) * mGridSize.y;
            }
            applyRegionPointDrag(*mDraggedRegion, mDraggedRegionLoop, mDraggedRegionPoint, local);
        }
    }

    if (hovered && mDraggedBatch && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        GameObject* batchObject = mDraggedBatch->owner();
        SpriteBatch::Entry* entry = mDraggedBatch->entryAt(mDraggedBatchIndex);
        if (batchObject && entry)
        {
            Math::Vec2 local = screenToObjectLocal(*batchObject, ImGui::GetIO().MousePos, origin);
            local.x -= entry->size.x * 0.5f;
            local.y -= entry->size.y * 0.5f;
            if (mSnap)
            {
                if (mGridSize.x > 0.0f)
                    local.x = roundf(local.x / mGridSize.x) * mGridSize.x;
                if (mGridSize.y > 0.0f)
                    local.y = roundf(local.y / mGridSize.y) * mGridSize.y;
            }
            entry->position = local;
        }
    }

    if (hovered && mDraggedBone && mGizmoAxis == -1 && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        GameObject* boneObject = mDraggedBone->owner();
        if (boneObject)
        {
            const Math::Vec2 bonePosition = boneObject->globalPosition();
            const Math::Vec2 cursor = screenToWorld(ImGui::GetIO().MousePos, origin);
            const float angle = atan2f(cursor.y - bonePosition.y, cursor.x - bonePosition.x) * 57.295779513f;
            boneObject->setRotationDegrees(angle);
            mDraggedBoneMoved = fabsf(angle - mDraggedBoneStartRotation) > 0.01f;
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        if (mDraggedBone)
        {
            if (mDraggedBoneMoved)
            {
                GameObject* boneObject = mDraggedBone->owner();
                if (boneObject)
                    recordBoneKey(*boneObject);
            }
            app().commitTransaction();
            mDraggedBone = nullptr;
            mDraggedBoneMoved = false;
        }
        if (mGizmoAxis != -1)
            app().commitTransaction();
        mGizmoAxis = -1;
        if (mDraggedPointCollider)
        {
            app().commitTransaction();
            mDraggedPointCollider = nullptr;
            mDraggedPointIndex = -1;
        }
        if (mDraggedBatch)
        {
            app().commitTransaction();
            mDraggedBatch = nullptr;
            mDraggedBatchIndex = -1;
        }
        if (mDraggedRegion)
        {
            app().commitTransaction();
            mDraggedRegion = nullptr;
            mDraggedRegionLoop = -1;
            mDraggedRegionPoint = -1;
        }
    }
}

} // namespace k2d::editor
