#pragma once

#include "k2d/Input.h"

#include <ct/string.hpp>

struct SDL_Window;

namespace k2d
{

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

        int Width() const { return mWidth; }
        int Height() const { return mHeight; }
        bool WasResized() const { return mResized; }
        float DeltaTime() const { return mDeltaTime; }
        static double TimeSeconds();

        Input &GetInput() { return mInput; }
        const Input &GetInput() const { return mInput; }

        void BeginUI();
        void EndUI();

        bool ImGuiWantsMouse() const { return mImGuiWantsMouse; }
        bool ImGuiWantsKeyboard() const { return mImGuiWantsKeyboard; }

        void CaptureScreenshot();
        void StartGifCapture(int frameRate = 60);
        void StopGifCapture();
        void CaptureGifFrame();
        bool IsGifCapturing() const { return mGifCapturing; }

    private:
        SDL_Window *mWindow;
        void *mGLContext;
        Input mInput;
        int mWidth;
        int mHeight;
        bool mResized;
        unsigned long long mLastCounter;
        float mDeltaTime;
        bool mImGuiWantsMouse;
        bool mImGuiWantsKeyboard;

        bool mGifCapturing;
        int mGifFrameRate;
        int mGifFrameCounter;
        void *mGifHandle;
        unsigned int mScreenshotIndex;
        unsigned int mGifFileIndex;

        ct::String mWindowTitle;
        void UpdateWindowTitle();
    };

}
