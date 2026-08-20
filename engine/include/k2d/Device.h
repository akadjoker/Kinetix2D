#pragma once

#include "k2d/Input.h"

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
        void Shutdown();

        bool PollEvents();
        void Swap();

        int Width() const { return mWidth; }
        int Height() const { return mHeight; }
        bool WasResized() const { return mResized; }
        float DeltaTime() const { return mDeltaTime; }

        Input &GetInput() { return mInput; }
        const Input &GetInput() const { return mInput; }

    private:
        SDL_Window *mWindow;
        void *mGLContext;
        Input mInput;
        int mWidth;
        int mHeight;
        bool mResized;
        unsigned long long mLastCounter;
        float mDeltaTime;
    };

}
