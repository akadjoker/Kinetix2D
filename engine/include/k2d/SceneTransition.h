#pragma once

#include "k2d/RenderTarget.h"

namespace k2d
{

    class CanvasRenderer;

    enum class SceneTransitionKind
    {
        None,
        Fade,
        SlideLeft,
        SlideRight,
        SlideUp,
        SlideDown,
        ZoomIn,
        ZoomOut,
        WipeLeft,
        WipeRight
    };

    // The last frame of the old scene, kept as a texture and animated away over
    // the new one, which runs underneath from its first frame.
    class SceneTransition
    {
    public:
        // "fade", "slide_left", ... ; None for an empty or unknown name.
        static SceneTransitionKind KindFromName(const char *name);

        // Copies the area of the bound framebuffer that the viewport covers: call
        // after the old scene is drawn and before the overlays.
        bool Capture();
        void Start(SceneTransitionKind kind, float seconds);
        void Update(float deltaTime);
        // Over the new scene, in screen space, before the fade and the overlays.
        void Draw(CanvasRenderer &canvas, float screenWidth, float screenHeight) const;
        void Clear();
        // Frees the texture while the GL context is still there.
        void Release();

        bool Active() const { return mKind != SceneTransitionKind::None; }
        float Progress() const;

    private:
        RenderTarget mSnapshot;
        SceneTransitionKind mKind = SceneTransitionKind::None;
        bool mCaptured = false;
        float mSeconds = 0.0f;
        float mElapsed = 0.0f;
    };

    SceneTransition &GetSceneTransition();

}
