#pragma once

namespace k2d
{

    class BatchRenderer;

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