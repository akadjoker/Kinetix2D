#include "GamePanel.h"

#include "core/EditorApplication.h"

#include <k2d/CameraComponent.h>
#include <k2d/NavigationAgent2D.h>
#include <k2d/GameObject.h>
#include <k2d/GameViewport.h>
#include <k2d/Scene.h>
#include <k2d/MouseCursor.h>
#include <k2d/ScreenFade.h>
#include <k2d/ZenScriptComponent.h>
#include <k2d/UiControls.h>


#include <cstdint>

namespace k2d::editor
{

GamePanel::GamePanel(EditorApplication& application) : EditorPanel("Game", application)
{
    mCanvasInitialized = mCanvas.Init();
}

GamePanel::~GamePanel()
{
    destroyFramebuffer();
    if (mCanvasInitialized)
        mCanvas.Shutdown();
}

void GamePanel::ensureFramebuffer(int width, int height)
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

void GamePanel::destroyFramebuffer()
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

void GamePanel::renderScene(int width, int height, const GameViewport& viewport)
{
    if (!mCanvasInitialized || !mCanvasReady)
        return;

    GLint savedViewport[4];
    glGetIntegerv(GL_VIEWPORT, savedViewport);
    GLint savedFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFbo);

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(static_cast<int>(viewport.x + 0.5f), static_cast<int>(viewport.y + 0.5f),
               static_cast<int>(viewport.width + 0.5f), static_cast<int>(viewport.height + 0.5f));

    Scene& scene = app().scene();
    CameraComponent* camera = scene.activeCamera();
    Camera2D& defaultCamera = mDefaultCamera;
    if (camera)
    {
        camera->setRenderViewport(viewport.virtualWidth, viewport.virtualHeight);
        mCanvas.SetProjection(camera->projection());
        SetZenScriptGameCamera(&camera->camera());
        scene.setRenderCamera(&camera->camera(), viewport.virtualWidth, viewport.virtualHeight);
    }
    else
    {
        mCanvas.SetProjection(defaultCamera.Projection(viewport.virtualWidth, viewport.virtualHeight));
        SetZenScriptGameCamera(&defaultCamera);
        scene.setRenderCamera(&defaultCamera, viewport.virtualWidth, viewport.virtualHeight);
    }
    scene.render(mCanvas);
    if (app().settings().showPhysicsDebug && scene.simulationEnabled())
    {
        // Physics is an editor overlay. scene.render() has already
        // submitted its batch; make the overlay independent from any
        // depth state a material/shadow pass may have changed.
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        scene.debugDrawBodies(mCanvas, k2d::DebugDrawShapes | k2d::DebugDrawAABBs | k2d::DebugDrawContacts |
                                           k2d::DebugDrawJoints);
    }
    GetScreenFade().Draw(mCanvas, viewport.virtualWidth, viewport.virtualHeight);
    GetMouseCursor().draw(mCanvas, viewport.virtualWidth, viewport.virtualHeight);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(savedFbo));
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
}

void GamePanel::drawAgentPaths(const ImVec2& position, const GameViewport& viewport)
{
    Scene& scene = app().scene();
    CameraComponent* camera = scene.activeCamera();
    if (!camera)
        return;

    // ScreenToWorld is the camera transform; the paths are world space, so the
    // overlay needs its inverse to land on the blitted game image.
    const Matrix2D toScreen = camera->camera().CameraXform(viewport.virtualWidth, viewport.virtualHeight).AffineInverse();
    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    const ImVec2 viewportPosition(position.x + viewport.x, position.y + viewport.y);

    drawList.PushClipRect(viewportPosition,
                          ImVec2(viewportPosition.x + viewport.width, viewportPosition.y + viewport.height), true);
    drawAgentPathsIn(scene.root(), toScreen, viewportPosition, viewport.width / viewport.virtualWidth,
                     viewport.height / viewport.virtualHeight, drawList);
    drawList.PopClipRect();
}

void GamePanel::drawAgentPathsIn(GameObject& object, const Matrix2D& toScreen, const ImVec2& position,
                                 float scaleX, float scaleY, ImDrawList& drawList)
{
    if (!object.isActiveInHierarchy())
        return;

    const auto screenPoint = [&](const Math::Vec2& world)
    {
        const Math::Vec2 point = toScreen.Transform(world);
        return ImVec2(position.x + point.x * scaleX, position.y + point.y * scaleY);
    };

    const size_t count = object.componentCount<NavigationAgent2D>();
    for (size_t i = 0; i < count; ++i)
    {
        const NavigationAgent2D* agent = object.getComponentAt<NavigationAgent2D>(i);
        if (!agent || !agent->active())
            continue;

        const ct::Vector<Math::Vec2>& path = agent->path();
        const size_t next = agent->pathIndex();
        const ImVec2 owner = screenPoint(object.globalPosition());
        Math::Vec2 targetWorld = agent->targetPosition();
        bool hasDebugTarget = agent->hasTarget();
        if (agent->hasFollowTarget())
        {
            if (GameObject* followed = app().scene().find(agent->followTargetName().c_str()))
            {
                targetWorld = followed->globalPosition();
                hasDebugTarget = true;
            }
        }
        if (agent->hasPath() && next < path.size())
        {
            ImVec2 previous = owner;
            for (size_t pointIndex = next; pointIndex < path.size(); ++pointIndex)
            {
                const ImVec2 point = screenPoint(path[pointIndex]);
                drawList.AddLine(previous, point, IM_COL32(70, 225, 255, 235), 2.5f);
                drawList.AddCircleFilled(point, 3.5f, IM_COL32(70, 225, 255, 245));
                previous = point;
            }
            const ImVec2 waypoint = screenPoint(path[next]);
            const ImVec2 pathLabel((owner.x + waypoint.x) * 0.5f + 5.0f,
                                   (owner.y + waypoint.y) * 0.5f - 16.0f);
            drawList.AddText(pathLabel, IM_COL32(70, 225, 255, 255), "PATH");
            drawList.AddCircle(waypoint, 7.0f, IM_COL32(255, 225, 70, 255), 16, 2.5f);
            drawList.AddText(ImVec2(waypoint.x + 8.0f, waypoint.y - 8.0f), IM_COL32(255, 225, 70, 255), "NEXT");
        }
        else if (hasDebugTarget && path.empty())
        {
            const ImVec2 target = screenPoint(targetWorld);
            drawList.AddLine(owner, target, IM_COL32(255, 75, 75, 190), 2.0f);
            drawList.AddText(ImVec2(owner.x + 8.0f, owner.y - 20.0f), IM_COL32(255, 90, 90, 255), "NO PATH");
        }

        if (hasDebugTarget)
        {
            const ImVec2 target = screenPoint(targetWorld);
            constexpr float arm = 7.0f;
            drawList.AddLine(ImVec2(target.x - arm, target.y), ImVec2(target.x + arm, target.y),
                             IM_COL32(255, 90, 220, 255), 2.5f);
            drawList.AddLine(ImVec2(target.x, target.y - arm), ImVec2(target.x, target.y + arm),
                             IM_COL32(255, 90, 220, 255), 2.5f);
            drawList.AddCircle(target, 10.0f, IM_COL32(255, 90, 220, 230), 20, 2.0f);
            drawList.AddText(ImVec2(target.x + 12.0f, target.y + 4.0f), IM_COL32(255, 120, 225, 255), "TARGET");
        }
    }

    for (size_t i = 0; i < object.childCount(); ++i)
        drawAgentPathsIn(*object.child(i), toScreen, position, scaleX, scaleY, drawList);
}

void GamePanel::drawContents()
{
    if (!app().playing())
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 position = ImGui::GetCursorScreenPos();
        const float width = available.x < 1.0f ? 1.0f : available.x;
        const float height = available.y < 1.0f ? 1.0f : available.y;
        ImGui::GetWindowDrawList()->AddRectFilled(position, ImVec2(position.x + width, position.y + height),
                                                  IM_COL32(12, 14, 18, 255));
        const char* message = "Press Play to run the edited scene";
        const ImVec2 textSize = ImGui::CalcTextSize(message);
        ImGui::SetCursorScreenPos(
            ImVec2(position.x + (width - textSize.x) * 0.5f, position.y + (height - textSize.y) * 0.5f));
        ImGui::TextDisabled("%s", message);
        return;
    }

    if (ImGui::Checkbox("Physics Debug", &app().settings().showPhysicsDebug))
        app().log(app().settings().showPhysicsDebug ? "Physics debug in Game on" : "Physics debug in Game off");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Overlay the live physics simulation in this Game view");
    ImGui::SameLine();
    if (ImGui::Checkbox("Navigation Debug", &app().settings().showNavigationDebug))
        app().log(app().settings().showNavigationDebug ? "Navigation debug on" : "Navigation debug off");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Overlay agent paths, next waypoints and targets in this Game view");

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const float width = available.x < 1.0f ? 1.0f : available.x;
    const float height = available.y < 1.0f ? 1.0f : available.y;
    const int fboWidth = static_cast<int>(width);
    const int fboHeight = static_cast<int>(height);
    ensureFramebuffer(fboWidth, fboHeight);
    CameraComponent* camera = app().scene().activeCamera();
    const GameViewport viewport = camera
                                      ? CalculateGameViewport(width, height, camera->viewportWidth(),
                                                              camera->viewportHeight(), camera->viewportScaleMode(),
                                                              camera->integerScale())
                                      : CalculateGameViewport(width, height, width, height);
    // UI and script input use the visible output rectangle but expose stable
    // virtual coordinates to the running scene.
    SetUiViewport(position.x + viewport.x, position.y + viewport.y, viewport.width, viewport.height,
                  viewport.virtualWidth, viewport.virtualHeight);
    SetZenScriptGameViewport(position.x + viewport.x, position.y + viewport.y, viewport.width, viewport.height,
                             viewport.virtualWidth, viewport.virtualHeight);
    const ImVec2 mouse = ImGui::GetMousePos();
    GetMouseCursor().setPosition(Math::Vec2(viewport.toVirtualX(mouse.x - position.x),
                                           viewport.toVirtualY(mouse.y - position.y)));
    renderScene(fboWidth, fboHeight, viewport);

    ImDrawList& drawList = *ImGui::GetWindowDrawList();
    if (mCanvasReady)
    {
        drawList.AddImage((ImTextureID)(intptr_t)mColorTexture, position,
                          ImVec2(position.x + width, position.y + height), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    }
    else
    {
        drawList.AddRectFilled(position, ImVec2(position.x + width, position.y + height), IM_COL32(12, 14, 18, 255));
    }
    if (app().settings().showNavigationDebug)
        drawAgentPaths(position, viewport);
    if (app().paused())
    {
        ImGui::SetCursorScreenPos(ImVec2(position.x + viewport.x + 8.0f, position.y + viewport.y + 8.0f));
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "PAUSED");
    }
}

} // namespace k2d::editor
