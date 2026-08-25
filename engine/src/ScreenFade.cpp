#include "k2d/ScreenFade.h"

#include "k2d/Batch.h"
#include "k2d/CanvasRenderer.h"
#include "k2d/RenderQueue.h"

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

    void ScreenFade::Draw(CanvasRenderer &canvas, float screenWidth, float screenHeight) const
    {
        if (mAlpha <= 0.0f || screenWidth <= 0.0f || screenHeight <= 0.0f)
            return;

        // The overlay is explicitly screen-space: it must cover the final
        // frame regardless of the active world camera.
        canvas.SetOrtho(screenWidth, screenHeight);
        RenderQueue overlay;
        RenderItem &item = overlay.AddItem(0);
        RenderCommand command = RenderCommand::MakeRect(0, 0.0f, 0.0f, screenWidth, screenHeight);
        command.pivotX = 0.0f;
        command.pivotY = 0.0f;
        command.color = Color::FromBytes(mColorR, mColorG, mColorB,
                                         static_cast<unsigned char>(mAlpha >= 1.0f ? 255.0f : mAlpha * 255.0f));
        item.commands.push_back(command);
        overlay.Flush(canvas);
    }

    ScreenFade &GetScreenFade()
    {
        static ScreenFade fade;
        return fade;
    }

    void FadeIn(float duration) { GetScreenFade().FadeIn(duration); }
    void FadeOut(float duration) { GetScreenFade().FadeOut(duration); }
    bool IsFading() { return GetScreenFade().IsFading(); }
    float FadeProgress() { return GetScreenFade().Progress(); }

}
