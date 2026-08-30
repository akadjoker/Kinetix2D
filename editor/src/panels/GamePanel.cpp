#include "GamePanel.h"

#include "core/EditorApplication.h"

#include <k2d/CameraComponent.h>
#include <k2d/NavigationAgent2D.h>
#include <k2d/GameObject.h>
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

void GamePanel::renderScene(int width, int height)
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

    Scene& scene = app().scene();
    CameraComponent* camera = scene.activeCamera();
    Camera2D& defaultCamera = mDefaultCamera;
    if (camera)
    {
        camera->setViewport(static_cast<float>(width), static_cast<float>(height));
        mCanvas.SetProjection(camera->projection());
        SetZenScriptGameCamera(&camera->camera());
        scene.setRenderCamera(&camera->camera(), static_cast<float>(width), static_cast<float>(height));
    }
    else
    {
        mCanvas.SetProjection(defaultCamera.Projection(static_cast<float>(width), static_cast<float>(height)));
        SetZenScriptGameCamera(&defaultCamera);
        scene.setRenderCamera(&defaultCamera, static_cast<float>(width), static_cast<float>(height));
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
    GetScreenFade().Draw(mCanvas, static_cast<float>(width), static_cast<float>(height));
    GetMouseCursor().draw(mCanvas, static_cast<float>(width), static_cast<float>(height));

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(savedFbo));
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
}

void GamePanel::drawAgentPaths(const ImVec2& position, float width, float height)
{
    Scene& scene = app().scene();
    CameraComponent* camera = scene.activeCamera();
    if (!camera)
        return;

    // ScreenToWorld is the camera transform; the paths are world space, so the
    // overlay needs its inverse to land on the blitted game image.
    const Matrix2D toScreen = camera->camera().CameraXform(width, height).AffineInverse();
    ImDrawList& drawList = *ImGui::GetWindowDrawList();

    drawAgentPathsIn(scene.root(), toScreen, position, drawList);
}

void GamePanel::drawAgentPathsIn(GameObject& object, const Matrix2D& toScreen, const ImVec2& position,
                                 ImDrawList& drawList)
{
    const size_t count = object.componentCount<NavigationAgent2D>();
    for (size_t i = 0; i < count; ++i)
    {
        const NavigationAgent2D* agent = object.getComponentAt<NavigationAgent2D>(i);
        if (!agent || !agent->active())
            continue;
        const ct::Vector<Math::Vec2>& path = agent->path();
        if (path.size() < 2)
            continue;

        for (size_t p = 0; p + 1 < path.size(); ++p)
        {
            const Math::Vec2 a = toScreen.Transform(path[p]);
            const Math::Vec2 b = toScreen.Transform(path[p + 1]);
            drawList.AddLine(ImVec2(position.x + a.x, position.y + a.y),
                             ImVec2(position.x + b.x, position.y + b.y), IM_COL32(255, 240, 120, 220), 2.0f);
        }
        for (size_t p = 0; p < path.size(); ++p)
        {
            const Math::Vec2 point = toScreen.Transform(path[p]);
            drawList.AddCircleFilled(ImVec2(position.x + point.x, position.y + point.y), 3.0f,
                                     IM_COL32(255, 240, 120, 235));
        }
        const Math::Vec2 goal = toScreen.Transform(path[path.size() - 1]);
        drawList.AddCircle(ImVec2(position.x + goal.x, position.y + goal.y), 7.0f, IM_COL32(255, 120, 90, 240), 12,
                           2.0f);
    }

    for (size_t i = 0; i < object.childCount(); ++i)
        drawAgentPathsIn(*object.child(i), toScreen, position, drawList);
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

    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const float width = available.x < 1.0f ? 1.0f : available.x;
    const float height = available.y < 1.0f ? 1.0f : available.y;
    const int fboWidth = static_cast<int>(width);
    const int fboHeight = static_cast<int>(height);
    ensureFramebuffer(fboWidth, fboHeight);
    // The scene renders into this framebuffer, so its screen-space UI must
    // use its dimensions before the render queue is built.
    SetUiViewport(position.x, position.y, width, height);
    const ImVec2 mouse = ImGui::GetMousePos();
    GetMouseCursor().setPosition(Math::Vec2(mouse.x - position.x, mouse.y - position.y));
    renderScene(fboWidth, fboHeight);

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
    if (app().settings().showPhysicsDebug)
        drawAgentPaths(position, width, height);
    SetZenScriptGameViewport(position.x, position.y, width, height);
    if (app().paused())
    {
        ImGui::SetCursorScreenPos(ImVec2(position.x + 8.0f, position.y + 8.0f));
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "PAUSED");
    }
}

} // namespace k2d::editor
