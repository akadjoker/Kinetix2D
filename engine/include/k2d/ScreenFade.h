#pragma once

namespace k2d
{

    class BatchRenderer;

    // Full-screen fade transition, unaffected by the game camera -- Godot's
    // usual pattern for this is a ColorRect in a CanvasLayer animated by a
    // Tween. K2D has no CanvasLayer yet, so this draws through BatchRenderer
    // instead: that renderer is already screen-space (Device/demo loops give
    // it a plain glm::ortho(0,w,h,0,-1,1) projection, unrelated to whatever
    // Camera2D the CanvasRenderer scene pass uses), which is exactly what a
    // fullscreen overlay needs and needs no CanvasRenderer/shader changes.
    //
    // Usage, once per frame, after the world/CanvasRenderer pass:
    //   fade.Update(dt);
    //   ... scene.render(canvas); canvas draw calls ...
    //   fade.Draw(batch, (float)device.Width(), (float)device.Height());
    class ScreenFade
    {
    public:
        ScreenFade();

        void SetColor(unsigned char r, unsigned char g, unsigned char b);

        // Ramps alpha 0 -> 1 over `duration` seconds (screen goes opaque).
        void FadeOut(float duration);
        // Ramps alpha 1 -> 0 over `duration` seconds (screen goes clear).
        void FadeIn(float duration);
        // Jumps straight to opaque/clear, cancelling any ramp in progress.
        void SetOpaque();
        void SetClear();

        void Update(float deltaTime);
        // No-op (and no draw call) once alpha reaches 0 -- safe to call every
        // frame unconditionally.
        void Draw(BatchRenderer &batch, float screenWidth, float screenHeight) const;

        float Alpha() const { return mAlpha; }
        bool IsFading() const { return mDuration > 0.0f && mElapsed < mDuration; }
        bool IsOpaque() const { return mAlpha >= 1.0f; }
        bool IsClear() const { return mAlpha <= 0.0f; }

    private:
        unsigned char mColorR, mColorG, mColorB;
        float mAlpha;
        float mFromAlpha;
        float mToAlpha;
        float mDuration;
        float mElapsed;
    };

}
