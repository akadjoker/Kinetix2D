#pragma once

namespace k2d
{

    class BatchRenderer;
    class CanvasRenderer;

    class ScreenFade
    {
    public:
        ScreenFade();

        void SetColor(unsigned char r, unsigned char g, unsigned char b);

        void FadeOut(float duration);

        void FadeIn(float duration);

        void SetOpaque();
        void SetClear();

        void Update(float deltaTime);

        void Draw(BatchRenderer &batch, float screenWidth, float screenHeight) const;
        // Draws above a CanvasRenderer scene. Call this after scene.render().
        void Draw(CanvasRenderer &canvas, float screenWidth, float screenHeight) const;

        float Alpha() const { return mAlpha; }
        float Progress() const { return mDuration > 0.0f ? (mElapsed >= mDuration ? 1.0f : mElapsed / mDuration) : 1.0f; }
        bool IsFading() const { return mDuration > 0.0f && mElapsed < mDuration; }
        bool IsOpaque() const { return mAlpha >= 1.0f; }
        bool IsClear() const { return mAlpha <= 0.0f; }

        // Preferred camelCase API. PascalCase names above remain compatible.
        void setColor(unsigned char r, unsigned char g, unsigned char b) { SetColor(r, g, b); }
        void fadeOut(float duration) { FadeOut(duration); }
        void fadeIn(float duration) { FadeIn(duration); }
        void setOpaque() { SetOpaque(); }
        void setClear() { SetClear(); }
        void update(float deltaTime) { Update(deltaTime); }
        void draw(BatchRenderer &batch, float screenWidth, float screenHeight) const { Draw(batch, screenWidth, screenHeight); }
        void draw(CanvasRenderer &canvas, float screenWidth, float screenHeight) const { Draw(canvas, screenWidth, screenHeight); }
        float alpha() const { return Alpha(); }
        float progress() const { return Progress(); }
        bool isFading() const { return IsFading(); }
        bool isOpaque() const { return IsOpaque(); }
        bool isClear() const { return IsClear(); }

    private:
        unsigned char mColorR, mColorG, mColorB;
        float mAlpha;
        float mFromAlpha;
        float mToAlpha;
        float mDuration;
        float mElapsed;
    };

    // Process-wide fade used by the runner and scripting API. It is cleared
    // automatically when a game session ends.
    ScreenFade &GetScreenFade();
    void FadeIn(float duration);
    void FadeOut(float duration);
    bool IsFading();
    float FadeProgress();

    inline void fade_in(float duration) { FadeIn(duration); }
    inline void fade_out(float duration) { FadeOut(duration); }
    inline bool is_fading() { return IsFading(); }
    inline float fade_progress() { return FadeProgress(); }

}
