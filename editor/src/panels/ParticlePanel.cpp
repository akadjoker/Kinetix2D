#include "ParticlePanel.h"

#include "AssetsPanel.h"
#include "core/EditorApplication.h"
#include "widgets/EditorToolbar.h"

#include <k2d/Camera2D.h>
#include <k2d/GameObject.h>
#include <k2d/ParticleComponent.h>
#include <k2d/Scene.h>

#include <glad/glad.h>
#include <IconsMaterialDesignIcons.h>

#include <cstdint>

namespace k2d::editor
{

namespace
{
ParticleComponent *findFirstParticle(GameObject &object)
{
    if (ParticleComponent *particle = object.getComponent<ParticleComponent>())
        return particle;
    for (size_t i = 0; i < object.childCount(); ++i)
        if (ParticleComponent *found = findFirstParticle(*object.child(i)))
            return found;
    return nullptr;
}

const char *modeName(ParticleMode mode)
{
    switch (mode)
    {
    case ParticleMode::OneShot: return "One Shot";
    case ParticleMode::Persistent: return "Persistent";
    case ParticleMode::Loop: return "Loop";
    }
    return "?";
}
}

ParticlePanel::ParticlePanel(EditorApplication &application) : EditorPanel("Particles", application)
{
    mCanvasInitialized = mCanvas.Init();
}

ParticlePanel::~ParticlePanel()
{
    destroyFramebuffer();
    if (mCanvasInitialized)
        mCanvas.Shutdown();
}

void ParticlePanel::ensureFramebuffer(int width, int height)
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

void ParticlePanel::destroyFramebuffer()
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

void ParticlePanel::renderPreview(ParticleComponent &particle, int width, int height)
{
    if (!mCanvasInitialized || !mCanvasReady)
        return;

    GLint savedViewport[4];
    glGetIntegerv(GL_VIEWPORT, savedViewport);
    GLint savedFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &savedFbo);

    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glViewport(0, 0, width, height);
    glClearColor(0.043f, 0.051f, 0.063f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    Camera2D camera;
    camera.position = particle.system().EmitterPosition();
    camera.zoom = Math::Vec2(mZoom, mZoom);
    mCanvas.SetProjection(camera.Projection(static_cast<float>(width), static_cast<float>(height)));

    mQueue.Clear();
    particle.system().Submit(mQueue, 0, particle.ySort(), particle.blendMode());
    mQueue.Flush(mCanvas);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(savedFbo));
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
}

void ParticlePanel::drawContents()
{
    GameObject *selected = app().selection().resolve(app().scene());
    ParticleComponent *particle = selected ? findFirstParticle(*selected) : nullptr;

    if (!particle)
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 position = ImGui::GetCursorScreenPos();
        const float width = available.x < 1.0f ? 1.0f : available.x;
        const float height = available.y < 1.0f ? 1.0f : available.y;
        ImGui::GetWindowDrawList()->AddRectFilled(
            position, ImVec2(position.x + width, position.y + height), IM_COL32(11, 13, 16, 255));
        const char *message = "Select an object with a Particle component";
        const ImVec2 textSize = ImGui::CalcTextSize(message);
        ImGui::SetCursorScreenPos(ImVec2(position.x + (width - textSize.x) * 0.5f,
                                        position.y + (height - textSize.y) * 0.5f));
        ImGui::TextDisabled("%s", message);
        return;
    }

    ParticleSystem &system = particle->system();

    if (toolbarIcon("restart", ICON_MDI_RESTART, "Restart emitter"))
        system.Reset();
    toolbarSameLine();
    if (toolbarIcon("burst", ICON_MDI_FLASH, "Emit a burst now"))
    {
        const size_t count = system.OneShotCount() > 0 ? system.OneShotCount() : 30;
        for (size_t i = 0; i < count; ++i)
            system.Emit(system.EmitterPosition(), system.GetPrefab());
    }
    toolbarSameLine();
    if (toolbarIcon("emitter", ICON_MDI_TARGET, "Show emitter marker", mShowEmitter))
        mShowEmitter = !mShowEmitter;
    toolbarSameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("##zoom", &mZoom, 0.25f, 4.0f, "%.2fx");
    ImGui::SameLine();
    ImGui::TextDisabled("%s | %d / %d alive%s",
                        modeName(system.GetMode()),
                        static_cast<int>(system.ActiveCount()),
                        static_cast<int>(system.Capacity()),
                        app().settings().viewportLivePreview ? "" : " | preview paused");
    ImGui::Separator();

    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const float width = available.x < 1.0f ? 1.0f : available.x;
    const float height = available.y < 1.0f ? 1.0f : available.y;
    const ImVec2 max(min.x + width, min.y + height);

    const int fboWidth = static_cast<int>(width);
    const int fboHeight = static_cast<int>(height);
    ensureFramebuffer(fboWidth, fboHeight);
    renderPreview(*particle, fboWidth, fboHeight);

    ImDrawList &drawList = *ImGui::GetWindowDrawList();
    if (mCanvasReady)
        drawList.AddImage((ImTextureID)(intptr_t)mColorTexture, min, max, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
    else
        drawList.AddRectFilled(min, max, IM_COL32(11, 13, 16, 255));

    if (mShowEmitter)
    {
        const ImVec2 center(min.x + width * 0.5f, min.y + height * 0.5f);
        drawList.AddCircle(center, 6.0f, IM_COL32(255, 200, 80, 200), 0, 1.5f);
        drawList.AddLine(ImVec2(center.x - 10.0f, center.y), ImVec2(center.x + 10.0f, center.y),
                         IM_COL32(255, 200, 80, 130));
        drawList.AddLine(ImVec2(center.x, center.y - 10.0f), ImVec2(center.x, center.y + 10.0f),
                         IM_COL32(255, 200, 80, 130));
    }

    if (!system.GetTexture())
    {
        const char *hint = "No texture - drag an image here";
        const ImVec2 textSize = ImGui::CalcTextSize(hint);
        ImGui::SetCursorScreenPos(ImVec2(min.x + (width - textSize.x) * 0.5f,
                                        min.y + (height - textSize.y) * 0.5f));
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "%s", hint);
        ImGui::SetCursorScreenPos(min);
    }

    ImGui::Dummy(ImVec2(width, height));
    if (ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(kTextureDragDropPayload))
        {
            const char *path = static_cast<const char *>(payload->Data);
            Texture *loaded = app().assets().GetTexture(path);
            if (!loaded)
                loaded = app().assets().LoadTexture(path, path, true, false);
            if (loaded)
            {
                const EditorApplication::SceneChange before = app().beginChange();
                system.SetTexture(loaded);
                app().commitChange("Set Particle Texture", before);
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f)
    {
        mZoom += ImGui::GetIO().MouseWheel * 0.1f * mZoom;
        if (mZoom < 0.25f)
            mZoom = 0.25f;
        if (mZoom > 4.0f)
            mZoom = 4.0f;
    }
}

}
