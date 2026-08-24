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

        bool Init(const char *title, int width, int height, bool vsync = true, bool enableUi = true);
        void Focus();
        void Shutdown();

        bool PollEvents();
        void Swap();

        int Width() const { return mWidth; }
        int Height() const { return mHeight; }
        bool WasResized() const { return mResized; }
        float DeltaTime() const { return mDeltaTime; }
        float FPS() const { return mFps; }
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

        // Preferred camelCase API. PascalCase names above remain compatible.
        bool init(const char *title, int width, int height, bool vsync = true, bool enableUi = true)
        { return Init(title, width, height, vsync, enableUi); }
        void focus() { Focus(); }
        void shutdown() { Shutdown(); }
        bool pollEvents() { return PollEvents(); }
        void swap() { Swap(); }
        int width() const { return Width(); }
        int height() const { return Height(); }
        bool wasResized() const { return WasResized(); }
        float deltaTime() const { return DeltaTime(); }
        float fps() const { return FPS(); }
        Input &input() { return GetInput(); }
        const Input &input() const { return GetInput(); }
        void beginUI() { BeginUI(); }
        void endUI() { EndUI(); }
        bool imguiWantsMouse() const { return ImGuiWantsMouse(); }
        bool imguiWantsKeyboard() const { return ImGuiWantsKeyboard(); }

    private:
        SDL_Window *mWindow;
        void *mGLContext;
        Input mInput;
        int mWidth;
        int mHeight;
        bool mResized;
        unsigned long long mLastCounter;
        float mDeltaTime;
        float mFps;
        float mFpsAccumulator;
        unsigned int mFpsFrames;
        bool mImGuiWantsMouse;
        bool mImGuiWantsKeyboard;
        bool mUiInitialized;

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
