#include "SceneViewportPanel.h"

#include "core/EditorApplication.h"
#include "panels/AssetsPanel.h"
#include "widgets/EditorToolbar.h"

#include <k2d/Assets.h>
#include <k2d/Camera2D.h>
#include <k2d/GameObject.h>
#include <k2d/ParticleComponent.h>
#include <k2d/Prefab.h>
#include <k2d/Scene.h>
#include <IconsMaterialDesignIcons.h>

#include <glad/glad.h>

#include <cstdint>
#include <math.h>

namespace k2d::editor
{

namespace
{
float maxValue(float a, float b) { return a > b ? a : b; }
float clampValue(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

float distanceToSegment(const ImVec2 &p, const ImVec2 &a, const ImVec2 &b)
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
}

SceneViewportPanel::SceneViewportPanel(EditorApplication &application)
    : EditorPanel("Scene", application)
{
    const EditorSettings &settings = application.settings();
    mPan = ImVec2(settings.viewportPan.x, settings.viewportPan.y);
    mZoom = settings.viewportZoom;
    mTool = settings.viewportTool;
    mSnap = settings.viewportSnap;
    mShowGrid = settings.viewportShowGrid;

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
    tickParticlePreview(app().scene().root(), ImGui::GetIO().DeltaTime);
    app().scene().render(mCanvas);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(savedFbo));
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
}

void SceneViewportPanel::tickParticlePreview(GameObject &object, float deltaTime)
{
    if (!object.isActiveInHierarchy())
        return;
    const size_t count = object.componentCount<ParticleComponent>();
    for (size_t i = 0; i < count; ++i)
    {
        ParticleComponent *particle = object.getComponentAt<ParticleComponent>(i);
        if (!particle || !particle->active())
            continue;
        if (particle->followOwner())
            particle->system().SetEmitterPosition(object.globalPosition());
        particle->system().Update(deltaTime);
    }
    for (size_t i = 0; i < object.childCount(); ++i)
        tickParticlePreview(*object.child(i), deltaTime);
}

ImVec2 SceneViewportPanel::worldToScreen(float x, float y, const ImVec2 &origin) const
{
    return ImVec2(origin.x + mPan.x + x * mZoom,
                  origin.y + mPan.y + y * mZoom);
}

Math::Vec2 SceneViewportPanel::screenToWorld(const ImVec2 &screen, const ImVec2 &origin) const
{
    return Math::Vec2((screen.x - origin.x - mPan.x) / mZoom,
                      (screen.y - origin.y - mPan.y) / mZoom);
}

void SceneViewportPanel::handlePrefabDrop(const ImVec2 &origin)
{
    if (!ImGui::BeginDragDropTarget())
        return;

    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kPrefabDragDropPayload))
    {
        const char *path = static_cast<const char *>(payload->Data);
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
            GameObject *created =
                prefab.Instantiate(app().scene(), &app().scene().root(), &app().assets());
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
                    world.x = roundf(world.x / 32.0f) * 32.0f;
                    world.y = roundf(world.y / 32.0f) * 32.0f;
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

void SceneViewportPanel::drawGrid(ImDrawList &drawList, const ImVec2 &min, const ImVec2 &max) const
{
    const float step = 32.0f * mZoom;
    if (step < 6.0f)
        return;
    float x = fmodf(min.x + mPan.x, step);
    if (x < 0.0f)
        x += step;
    for (; min.x + x < max.x; x += step)
        drawList.AddLine(ImVec2(min.x + x, min.y), ImVec2(min.x + x, max.y), IM_COL32(47, 52, 62, 150));
    float y = fmodf(min.y + mPan.y, step);
    if (y < 0.0f)
        y += step;
    for (; min.y + y < max.y; y += step)
        drawList.AddLine(ImVec2(min.x, min.y + y), ImVec2(max.x, min.y + y), IM_COL32(47, 52, 62, 150));
}

void SceneViewportPanel::drawObject(ImDrawList &drawList, GameObject &object, const ImVec2 &origin)
{
    const Math::Vec2 world = object.globalPosition();
    const ImVec2 point = worldToScreen(world.x, world.y, origin);
    if (object.parent())
    {
        const Math::Vec2 parentWorld = object.parent()->globalPosition();
        const ImVec2 parent = worldToScreen(parentWorld.x, parentWorld.y, origin);
        drawList.AddLine(parent, point, IM_COL32(100, 110, 125, 170), 1.0f);
    }

    const bool selected = app().selection().hasSelection() &&
                          app().selection().objectId() == object.id();
    drawList.AddCircleFilled(point, selected ? 6.0f : 4.0f,
                             selected ? IM_COL32(255, 205, 65, 255) : IM_COL32(70, 180, 235, 255));
    drawList.AddText(ImVec2(point.x + 8.0f, point.y - 8.0f), IM_COL32(220, 225, 235, 230),
                     object.name().empty() ? "GameObject" : object.name().c_str());

    for (size_t i = 0; i < object.childCount(); ++i)
        drawObject(drawList, *object.child(i), origin);
}

void SceneViewportPanel::pickObject(GameObject &object, const ImVec2 &mouse, const ImVec2 &origin,
                                    GameObject *&best, float &bestDistance)
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

void SceneViewportPanel::drawGizmo(ImDrawList &drawList, GameObject &selected, const ImVec2 &origin) const
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
            drawList.AddTriangleFilled(ImVec2(xTip.x - 9.0f, xTip.y - 5.0f),
                                       ImVec2(xTip.x - 9.0f, xTip.y + 5.0f), ImVec2(xTip.x + 3.0f, xTip.y), xColor);
            drawList.AddTriangleFilled(ImVec2(yTip.x - 5.0f, yTip.y - 9.0f),
                                       ImVec2(yTip.x + 5.0f, yTip.y - 9.0f), ImVec2(yTip.x, yTip.y + 3.0f), yColor);
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

int SceneViewportPanel::hitTestGizmo(GameObject &selected, const ImVec2 &mouse, const ImVec2 &origin) const
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

    const char *toolIcons[] = {
        ICON_MDI_CURSOR_DEFAULT, ICON_MDI_ARROW_ALL, ICON_MDI_ROTATE_ORBIT,
        ICON_MDI_ARROW_EXPAND_ALL, ICON_MDI_HAND
    };
    const char *tooltips[] = {"Select", "Move", "Rotate", "Scale", "Pan"};
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
    if (toolbarIcon("snap", ICON_MDI_MAGNET, "Snap to 32 pixel grid", mSnap))
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

    ImDrawList &drawList = *ImGui::GetWindowDrawList();
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

    GameObject *selected = app().selection().resolve(app().scene());
    const bool gizmoActive = selected && selected != &app().scene().root() && !selected->locked() &&
                             mTool >= 1 && mTool <= 3;
    if (gizmoActive)
        drawGizmo(drawList, *selected, origin);
    drawList.PopClipRect();

    ImGui::InvisibleButton("##scene_canvas", size,
                           ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    handlePrefabDrop(origin);
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
    if (hovered && mTool != 4 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        const int hitAxis = gizmoActive ? hitTestGizmo(*selected, ImGui::GetIO().MousePos, origin) : -1;
        if (hitAxis != -1)
        {
            mGizmoAxis = hitAxis;
            const char *label = mTool == 1 ? "Move GameObject" : mTool == 2 ? "Rotate GameObject" : "Scale GameObject";
            app().beginTransaction(label, app().beginChange());
        }
        else
        {
            mGizmoAxis = -1;
            GameObject *best = nullptr;
            float bestDistance = 14.0f;
            pickObject(app().scene().root(), ImGui::GetIO().MousePos, origin, best, bestDistance);
            app().selection().select(best);
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
            Math::Vec2 position = selected->position();
            if (mGizmoAxis == 0 || mGizmoAxis == 2)
                position.x += delta.x / mZoom;
            if (mGizmoAxis == 1 || mGizmoAxis == 2)
                position.y += delta.y / mZoom;
            if (mSnap)
            {
                position.x = roundf(position.x / 32.0f) * 32.0f;
                position.y = roundf(position.y / 32.0f) * 32.0f;
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

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        if (mGizmoAxis != -1)
            app().commitTransaction();
        mGizmoAxis = -1;
    }
}

}
