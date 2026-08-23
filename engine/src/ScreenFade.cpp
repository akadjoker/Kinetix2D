#include "k2d/ScreenFade.h"

#include "k2d/Batch.h"

namespace k2d
{

    ScreenFade::ScreenFade()
        : mColorR(0), mColorG(0), mColorB(0), mAlpha(0.0f),
          mFromAlpha(0.0f), mToAlpha(0.0f), mDuration(0.0f), mElapsed(0.0f)
    {
    }

    void ScreenFade::SetColor(unsigned char r, unsigned char g, unsigned char b)
    {
        mColorR = r;
        mColorG = g;
        mColorB = b;
    }

    void ScreenFade::FadeOut(float duration)
    {
        mFromAlpha = mAlpha;
        mToAlpha = 1.0f;
        mDuration = duration > 0.0f ? duration : 0.0f;
        mElapsed = 0.0f;
        if (mDuration <= 0.0f)
            mAlpha = 1.0f;
    }

    void ScreenFade::FadeIn(float duration)
    {
        mFromAlpha = mAlpha;
        mToAlpha = 0.0f;
        mDuration = duration > 0.0f ? duration : 0.0f;
        mElapsed = 0.0f;
        if (mDuration <= 0.0f)
            mAlpha = 0.0f;
    }

    void ScreenFade::SetOpaque()
    {
        mAlpha = 1.0f;
        mDuration = 0.0f;
    }

    void ScreenFade::SetClear()
    {
        mAlpha = 0.0f;
        mDuration = 0.0f;
    }

    void ScreenFade::Update(float deltaTime)
    {
        if (mDuration <= 0.0f || mElapsed >= mDuration)
            return;

        mElapsed += deltaTime;
        float t = mElapsed >= mDuration ? 1.0f : mElapsed / mDuration;
        mAlpha = mFromAlpha + (mToAlpha - mFromAlpha) * t;
    }

    void ScreenFade::Draw(BatchRenderer &batch, float screenWidth, float screenHeight) const
    {
        if (mAlpha <= 0.0f)
            return;

        unsigned char a = (unsigned char)(mAlpha >= 1.0f ? 255.0f : mAlpha * 255.0f);
        batch.SetColor(mColorR, mColorG, mColorB, a);
        batch.DrawRect(0.0f, 0.0f, screenWidth, screenHeight, true);
    }

}