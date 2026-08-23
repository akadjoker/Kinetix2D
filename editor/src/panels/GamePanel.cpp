#include "GamePanel.h"

#include "core/EditorApplication.h"

#include <k2d/CameraComponent.h>
#include <k2d/GameObject.h>
#include <k2d/Scene.h>

#include <glad/glad.h>

#include <cstdint>

namespace k2d::editor
{

namespace
{
CameraComponent *findFirstCamera(GameObject &object)
{
    if (CameraComponent *camera = object.getComponent<CameraComponent>())
        return camera;
    for (size_t i = 0; i < object.childCount(); ++i)
        if (CameraComponent *found = findFirstCamera(*object.child(i)))
            return found;
    return nullptr;
}
}

GamePanel::GamePanel(EditorApplication &application) : EditorPanel("Game", application)
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

    Scene &scene = app().runtimeScene();
    CameraComponent *camera = findFirstCamera(scene.root());
    if (camera)
    {
        camera->setViewport(static_cast<float>(width), static_cast<float>(height));
        mCanvas.SetProjection(camera->projection());
    }
    else
    {
        Camera2D defaultCamera;
        mCanvas.SetProjection(defaultCamera.Projection(static_cast<float>(width), static_cast<float>(height)));
    }
    scene.render(mCanvas);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(savedFbo));
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
}

void GamePanel::drawContents()
{
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const float width = available.x < 1.0f ? 1.0f : available.x;
    const float height = available.y < 1.0f ? 1.0f : available.y;

    if (!app().playing())
    {
        ImGui::GetWindowDrawList()->AddRectFilled(
            position, ImVec2(position.x + width, position.y + height), IM_COL32(12, 14, 18, 255));
        const char *message = "Press Play to run the edited scene";
        const ImVec2 textSize = ImGui::CalcTextSize(message);
        ImGui::SetCursorScreenPos(ImVec2(position.x + (width - textSize.x) * 0.5f,
                                        position.y + (height - textSize.y) * 0.5f));
        ImGui::TextDisabled("%s", message);
        return;
    }

    const int fboWidth = static_cast<int>(width);
    const int fboHeight = static_cast<int>(height);
    ensureFramebuffer(fboWidth, fboHeight);
    renderScene(fboWidth, fboHeight);

    ImDrawList &drawList = *ImGui::GetWindowDrawList();
    if (mCanvasReady)
    {
        drawList.AddImage((ImTextureID)(intptr_t)mColorTexture, position,
                          ImVec2(position.x + width, position.y + height), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    }
    else
    {
        drawList.AddRectFilled(position, ImVec2(position.x + width, position.y + height),
                               IM_COL32(12, 14, 18, 255));
    }
    if (app().paused())
    {
        ImGui::SetCursorScreenPos(ImVec2(position.x + 8.0f, position.y + 8.0f));
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "PAUSED");
    }
}

}
