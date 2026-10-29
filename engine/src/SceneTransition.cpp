#include "k2d/SceneTransition.h"

#include "k2d/CanvasRenderer.h"
#include "k2d/OpenGL.h"
#include "k2d/RenderQueue.h"

#include <cstring>

namespace k2d
{

    SceneTransitionKind SceneTransition::KindFromName(const char *name)
    {
        struct Named
        {
            const char *name;
            SceneTransitionKind kind;
        };
        static const Named kNames[] = {
            {"fade", SceneTransitionKind::Fade},           {"slide_left", SceneTransitionKind::SlideLeft},
            {"slide_right", SceneTransitionKind::SlideRight}, {"slide_up", SceneTransitionKind::SlideUp},
            {"slide_down", SceneTransitionKind::SlideDown}, {"zoom_in", SceneTransitionKind::ZoomIn},
            {"zoom_out", SceneTransitionKind::ZoomOut},     {"wipe_left", SceneTransitionKind::WipeLeft},
            {"wipe_right", SceneTransitionKind::WipeRight}};
        if (!name)
            return SceneTransitionKind::None;
        for (const Named &named : kNames)
            if (std::strcmp(name, named.name) == 0)
                return named.kind;
        return SceneTransitionKind::None;
    }

    bool SceneTransition::Capture()
    {
        GLint framebuffer = 0;
        GLint viewport[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
        glGetIntegerv(GL_VIEWPORT, viewport);
        mCaptured = false;
        if (viewport[2] <= 0 || viewport[3] <= 0 || !mSnapshot.resize(viewport[2], viewport[3]))
            return false;
        const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
        glDisable(GL_SCISSOR_TEST);
        while (glGetError() != GL_NO_ERROR)
        {
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mSnapshot.framebuffer());
        glBlitFramebuffer(viewport[0], viewport[1], viewport[0] + viewport[2], viewport[1] + viewport[3], 0, 0,
                          viewport[2], viewport[3], GL_COLOR_BUFFER_BIT, GL_NEAREST);
        mCaptured = glGetError() == GL_NO_ERROR;
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        if (scissor)
            glEnable(GL_SCISSOR_TEST);
        return mCaptured;
    }

    void SceneTransition::Start(SceneTransitionKind kind, float seconds)
    {
        if (!mCaptured || kind == SceneTransitionKind::None || seconds <= 0.0f)
        {
            Clear();
            return;
        }
        mKind = kind;
        mSeconds = seconds;
        mElapsed = 0.0f;
        mCaptured = false;
    }

    void SceneTransition::Update(float deltaTime)
    {
        if (mKind == SceneTransitionKind::None)
            return;
        mElapsed += deltaTime;
        if (mElapsed >= mSeconds)
            mKind = SceneTransitionKind::None;
    }

    float SceneTransition::Progress() const
    {
        if (mKind == SceneTransitionKind::None || mSeconds <= 0.0f)
            return 1.0f;
        return mElapsed >= mSeconds ? 1.0f : mElapsed / mSeconds;
    }

    void SceneTransition::Draw(CanvasRenderer &canvas, float screenWidth, float screenHeight) const
    {
        if (mKind == SceneTransitionKind::None || screenWidth <= 0.0f || screenHeight <= 0.0f)
            return;
        const float t = Progress();
        const float e = t * t * (3.0f - 2.0f * t);
        const float textureWidth = static_cast<float>(mSnapshot.width());
        const float textureHeight = static_cast<float>(mSnapshot.height());

        float x = screenWidth * 0.5f, y = screenHeight * 0.5f;
        float width = screenWidth, height = screenHeight;
        float srcX = 0.0f, srcW = textureWidth;
        float alpha = 1.0f;
        switch (mKind)
        {
        case SceneTransitionKind::Fade:
            alpha = 1.0f - e;
            break;
        case SceneTransitionKind::SlideLeft:
            x -= e * screenWidth;
            break;
        case SceneTransitionKind::SlideRight:
            x += e * screenWidth;
            break;
        case SceneTransitionKind::SlideUp:
            y -= e * screenHeight;
            break;
        case SceneTransitionKind::SlideDown:
            y += e * screenHeight;
            break;
        case SceneTransitionKind::ZoomIn:
            width *= 1.0f + e;
            height *= 1.0f + e;
            alpha = 1.0f - e;
            break;
        case SceneTransitionKind::ZoomOut:
            width *= 1.0f - e;
            height *= 1.0f - e;
            break;
        case SceneTransitionKind::WipeLeft:
            width *= 1.0f - e;
            srcW *= 1.0f - e;
            x = width * 0.5f;
            break;
        case SceneTransitionKind::WipeRight:
            width *= 1.0f - e;
            srcX = textureWidth * e;
            srcW -= srcX;
            x = screenWidth - width * 0.5f;
            break;
        case SceneTransitionKind::None:
            break;
        }
        if (width <= 0.0f || height <= 0.0f || alpha <= 0.0f)
            return;

        canvas.SetOrtho(screenWidth, screenHeight);
        RenderQueue overlay;
        RenderItem &item = overlay.AddItem(0);
        RenderCommand &command = item.commands.emplace_back();
        command.type = RenderCommand::kRect;
        command.textureId = mSnapshot.texture();
        command.x = x;
        command.y = y;
        command.width = width;
        command.height = height;
        command.texWidth = mSnapshot.width();
        command.texHeight = mSnapshot.height();
        command.srcX = srcX;
        command.srcY = 0.0f;
        command.srcW = srcW;
        command.srcH = textureHeight;
        command.flags = 2;
        command.color = Color(1.0f, 1.0f, 1.0f, alpha);
        command.lightMask = kUnlitLightMask;
        overlay.Flush(canvas);
    }

    void SceneTransition::Clear()
    {
        mKind = SceneTransitionKind::None;
        mCaptured = false;
        mElapsed = 0.0f;
    }

    void SceneTransition::Release()
    {
        Clear();
        mSnapshot.destroy();
    }

    SceneTransition &GetSceneTransition()
    {
        static SceneTransition transition;
        return transition;
    }

}
