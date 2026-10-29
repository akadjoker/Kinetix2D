#pragma once

#include "k2d/Input.h"

#include <ct/string.hpp>

struct PlatformWindow;

namespace k2d
{
    // A platform key or mouse button as the Input knows it. Device feeds its own
    // window's events through these; a host that reads the window itself (the
    // editor, running the game in a panel) feeds an Input of its own the same way.
    int InputKeyFromPlatform(int platformKey);
    int InputButtonFromPlatform(int platformButton);

    class Device
    {
    public:
        Device();
        ~Device();

        Device(const Device &) = delete;
        Device &operator=(const Device &) = delete;

        bool Init(const char *title, int width, int height, bool vsync = true);
        void Focus();
        void Shutdown();
        bool PollEvents();
        void Swap();
        void ShowCursor(bool visible);

        int Width() const { return mWidth; }
        int Height() const { return mHeight; }
        int DrawableWidth() const { return mDrawableWidth; }
        int DrawableHeight() const { return mDrawableHeight; }
        bool WasResized() const { return mResized; }
        // False while the system has taken the window's surface (an Android app in
        // the background): nothing can be drawn until it comes back.
        bool HasSurface() const { return mHasSurface; }
        float DeltaTime() const { return mDeltaTime; }
        float FPS() const { return mFps; }
        int DisplayIndex() const;
        bool SetDisplayIndex(int displayIndex);
        static double TimeSeconds();

        Input &GetInput() { return mInput; }
        const Input &GetInput() const { return mInput; }
        PlatformWindow *Window() const { return mWindow; }

        void CaptureScreenshot();
        void StartGifCapture(int frameRate = 60);
        void StopGifCapture();
        void CaptureGifFrame();
        bool IsGifCapturing() const { return mGifCapturing; }

        bool init(const char *title, int width, int height, bool vsync = true)
        { return Init(title, width, height, vsync); }
        void focus() { Focus(); }
        void shutdown() { Shutdown(); }
        bool pollEvents() { return PollEvents(); }
        void swap() { Swap(); }
        void showCursor(bool visible) { ShowCursor(visible); }
        int width() const { return Width(); }
        int height() const { return Height(); }
        int drawableWidth() const { return DrawableWidth(); }
        int drawableHeight() const { return DrawableHeight(); }
        bool wasResized() const { return WasResized(); }
        float deltaTime() const { return DeltaTime(); }
        float fps() const { return FPS(); }
        int displayIndex() const { return DisplayIndex(); }
        bool setDisplayIndex(int displayIndex) { return SetDisplayIndex(displayIndex); }
        Input &input() { return GetInput(); }
        const Input &input() const { return GetInput(); }
        PlatformWindow *window() const { return Window(); }

    private:
        void UpdateWindowTitle();
        void UpdateSizes();

        PlatformWindow *mWindow;
        Input mInput;
        int mWidth;
        int mHeight;
        int mDrawableWidth;
        int mDrawableHeight;
        bool mResized;
        bool mHasSurface = true;
        double mLastTime;
        float mDeltaTime;
        float mFps;
        float mFpsAccumulator;
        unsigned int mFpsFrames;
        bool mGifCapturing;
        int mGifFrameRate;
        int mGifFrameCounter;
        void *mGifHandle;
        unsigned int mScreenshotIndex;
        unsigned int mGifFileIndex;
        ct::String mWindowTitle;
    };
}
