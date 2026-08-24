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

        // Preferred camelCase API. PascalCase names above remain compatible.
        void setColor(unsigned char r, unsigned char g, unsigned char b) { SetColor(r, g, b); }
        void fadeOut(float duration) { FadeOut(duration); }
        void fadeIn(float duration) { FadeIn(duration); }
        void setOpaque() { SetOpaque(); }
        void setClear() { SetClear(); }
        void update(float deltaTime) { Update(deltaTime); }
        void draw(BatchRenderer &batch, float screenWidth, float screenHeight) const { Draw(batch, screenWidth, screenHeight); }
        float alpha() const { return Alpha(); }
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

}
