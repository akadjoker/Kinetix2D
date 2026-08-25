#include "k2d/Device.h"

#include "k2d/FileSystem.h"

#include <SDL.h>
#include <glad/glad.h>
#include <cstdio>
#include <cstring>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define MSF_GIF_IMPL
#include <msf_gif.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_opengl3.h>

namespace k2d
{

    Device::Device()
        : mWindow(nullptr), mGLContext(nullptr), mWidth(0), mHeight(0), mDrawableWidth(0), mDrawableHeight(0),
          mResized(false), mLastCounter(0), mDeltaTime(0.0f),
          mFps(0.0f), mFpsAccumulator(0.0f), mFpsFrames(0),
          mImGuiWantsMouse(false), mImGuiWantsKeyboard(false),
          mUiInitialized(false),
          mGifCapturing(false), mGifFrameRate(60), mGifFrameCounter(0),
          mGifHandle(nullptr), mScreenshotIndex(1), mGifFileIndex(1)
    {
    }

    Device::~Device()
    {
        Shutdown();
    }

    bool Device::Init(const char *title, int width, int height, bool vsync, bool enableUi)
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        {
            std::printf("SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }

        FileSystem::Instance().Init();

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        // A 1280x720 runner on a smaller desktop gets centered with part of
        // the window off-screen. Clamp only the initial size; the window stays
        // resizable and the game continues to use its actual drawable size.
        int initialWidth = width;
        int initialHeight = height;
        SDL_Rect usableBounds{};
        if (SDL_GetDisplayUsableBounds(0, &usableBounds) == 0)
        {
            const int margin = 48;
            if (usableBounds.w > margin)
                initialWidth = initialWidth < usableBounds.w - margin ? initialWidth : usableBounds.w - margin;
            if (usableBounds.h > margin)
                initialHeight = initialHeight < usableBounds.h - margin ? initialHeight : usableBounds.h - margin;
        }

        mWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    initialWidth, initialHeight, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
        if (!mWindow)
        {
            std::printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
            return false;
        }

        SDL_GLContext ctx = SDL_GL_CreateContext(mWindow);
        if (!ctx)
        {
            std::printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
            return false;
        }
        mGLContext = ctx;

        SDL_GL_MakeCurrent(mWindow, ctx);

        if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress))
        {
            std::printf("gladLoadGLES2Loader failed\n");
            return false;
        }

        SDL_GL_SetSwapInterval(vsync ? 1 : 0);

        std::printf("GL_VERSION: %s\n", (const char *)glGetString(GL_VERSION));
        std::printf("GL_RENDERER: %s\n", (const char *)glGetString(GL_RENDERER));

        int w = 0;
        int h = 0;
        SDL_GetWindowSize(mWindow, &w, &h);
        mWidth = w;
        mHeight = h;
        mResized = false;
        int drawableWidth = 0;
        int drawableHeight = 0;
        SDL_GL_GetDrawableSize(mWindow, &drawableWidth, &drawableHeight);
        mDrawableWidth = drawableWidth;
        mDrawableHeight = drawableHeight;
        glViewport(0, 0, drawableWidth, drawableHeight);

        mLastCounter = SDL_GetPerformanceCounter();
        mDeltaTime = 0.0f;
        mFps = 0.0f;
        mFpsAccumulator = 0.0f;
        mFpsFrames = 0;

        if (enableUi)
        {
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGui_ImplSDL2_InitForOpenGL(mWindow, mGLContext);
            ImGui_ImplOpenGL3_Init("#version 300 es");
            mUiInitialized = true;
        }

        mWindowTitle = title;

        return true;
    }

    void Device::Shutdown()
    {
        if (mUiInitialized)
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext();
            mUiInitialized = false;
        }
        if (mGLContext)
        {
            SDL_GL_DeleteContext(mGLContext);
            mGLContext = nullptr;
        }
        if (mWindow)
        {
            SDL_DestroyWindow(mWindow);
            mWindow = nullptr;
        }
        FileSystem::Instance().Shutdown();
        SDL_Quit();
    }

    void Device::BeginUI()
    {
        if (mUiInitialized)
        {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();
        }
    }

    void Device::EndUI()
    {
        if (mUiInitialized)
        {
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
    }

    void Device::Focus()
    {
        if (!mWindow)
            return;
        SDL_RaiseWindow(mWindow);
        SDL_SetWindowInputFocus(mWindow);
    }

    int Device::DisplayIndex() const
    {
        if (!mWindow)
            return 0;
        const int display = SDL_GetWindowDisplayIndex(mWindow);
        return display >= 0 ? display : 0;
    }

    bool Device::SetDisplayIndex(int displayIndex)
    {
        if (!mWindow || displayIndex < 0 || displayIndex >= SDL_GetNumVideoDisplays())
            return false;
        SDL_Rect bounds{};
        if (SDL_GetDisplayUsableBounds(displayIndex, &bounds) != 0)
            return false;
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(mWindow, &width, &height);
        SDL_SetWindowPosition(mWindow, bounds.x + (bounds.w - width) / 2,
                              bounds.y + (bounds.h - height) / 2);
        return true;
    }

    bool Device::PollEvents()
    {
        mInput.NewFrame();
        mResized = false;

        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (mUiInitialized)
                ImGui_ImplSDL2_ProcessEvent(&e);

            switch (e.type)
            {
            case SDL_QUIT:
                return false;

            case SDL_KEYDOWN:
                mInput.OnKey(e.key.keysym.scancode, true, e.key.repeat != 0);
                break;

            case SDL_KEYUP:
                mInput.OnKey(e.key.keysym.scancode, false, e.key.repeat != 0);
                break;

            case SDL_MOUSEBUTTONDOWN:
                mInput.OnMouseButton(e.button.button - 1, true);
                break;

            case SDL_MOUSEBUTTONUP:
                mInput.OnMouseButton(e.button.button - 1, false);
                break;

            case SDL_MOUSEMOTION:
                mInput.OnMouseMove((float)e.motion.x, (float)e.motion.y);
                break;

            case SDL_MOUSEWHEEL:
                mInput.OnWheel((float)e.wheel.y);
                break;

            case SDL_FINGERDOWN:
                mInput.OnTouch((long long)e.tfinger.fingerId,
                                e.tfinger.x * mWidth, e.tfinger.y * mHeight, true, false);
                break;

            case SDL_FINGERUP:
                mInput.OnTouch((long long)e.tfinger.fingerId,
                                e.tfinger.x * mWidth, e.tfinger.y * mHeight, false, true);
                break;

            case SDL_FINGERMOTION:
                mInput.OnTouch((long long)e.tfinger.fingerId,
                                e.tfinger.x * mWidth, e.tfinger.y * mHeight, false, false);
                break;

            case SDL_WINDOWEVENT:
                if (e.window.event == SDL_WINDOWEVENT_RESIZED ||
                    e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    int w = 0;
                    int h = 0;
                    SDL_GetWindowSize(mWindow, &w, &h);
                    if (w != mWidth || h != mHeight)
                    {
                        mWidth = w;
                        mHeight = h;
                        mResized = true;
                        int drawableWidth = 0;
                        int drawableHeight = 0;
                        SDL_GL_GetDrawableSize(mWindow, &drawableWidth, &drawableHeight);
                        mDrawableWidth = drawableWidth;
                        mDrawableHeight = drawableHeight;
                        glViewport(0, 0, drawableWidth, drawableHeight);
                    }
                }
                break;

            default:
                break;
            }
        }

        int windowWidth = 0;
        int windowHeight = 0;
        SDL_GetWindowSize(mWindow, &windowWidth, &windowHeight);
        if (windowWidth != mWidth || windowHeight != mHeight)
        {
            mWidth = windowWidth;
            mHeight = windowHeight;
            mResized = true;
        }

        int drawableWidth = 0;
        int drawableHeight = 0;
        SDL_GL_GetDrawableSize(mWindow, &drawableWidth, &drawableHeight);
        mDrawableWidth = drawableWidth;
        mDrawableHeight = drawableHeight;
        glViewport(0, 0, drawableWidth, drawableHeight);

        unsigned long long now = SDL_GetPerformanceCounter();
        unsigned long long freq = SDL_GetPerformanceFrequency();
        float dt = (float)((double)(now - mLastCounter) / (double)freq);
        mLastCounter = now;
        if (dt > 0.25f)
            dt = 0.25f;
        mDeltaTime = dt;
        mFpsAccumulator += dt;
        ++mFpsFrames;
        // Sample over half a second instead of displaying 1 / dt. It is stable
        // under vsync and still responds quickly when a scene becomes expensive.
        if (mFpsAccumulator >= 0.5f)
        {
            mFps = static_cast<float>(mFpsFrames) / mFpsAccumulator;
            mFpsAccumulator = 0.0f;
            mFpsFrames = 0;
        }

        if (mUiInitialized)
        {
            ImGuiIO &io = ImGui::GetIO();
            mImGuiWantsMouse = io.WantCaptureMouse;
            mImGuiWantsKeyboard = io.WantCaptureKeyboard;
        }
        else
        {
            mImGuiWantsMouse = false;
            mImGuiWantsKeyboard = false;
        }

        return true;
    }

    double Device::TimeSeconds()
    {
        return (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
    }

    void Device::Swap()
    {
        SDL_GL_SwapWindow(mWindow);
    }

    void Device::UpdateWindowTitle()
    {
        ct::String title = mWindowTitle;
        if (mGifCapturing)
            title += " [RECORDING]";
        SDL_SetWindowTitle(mWindow, title.c_str());
    }

    void Device::CaptureScreenshot()
    {
        unsigned char *pixels = new unsigned char[mWidth * mHeight * 4];
        glReadPixels(0, 0, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        char filename[256];
        std::snprintf(filename, sizeof(filename), "screenshot_%04u.png", mScreenshotIndex++);

        int stride = mWidth * 4;
        for (int y = 0; y < mHeight / 2; ++y)
        {
            unsigned char *top = pixels + y * stride;
            unsigned char *bottom = pixels + (mHeight - 1 - y) * stride;
            for (int x = 0; x < stride; ++x)
            {
                unsigned char tmp = top[x];
                top[x] = bottom[x];
                bottom[x] = tmp;
            }
        }

        stbi_write_png(filename, mWidth, mHeight, 4, pixels, stride);
        std::printf("Screenshot saved: %s\n", filename);

        delete[] pixels;
    }

    void Device::StartGifCapture(int frameRate)
    {
        if (mGifCapturing)
            return;

        mGifFrameRate = frameRate;
        mGifFrameCounter = 0;
        mGifCapturing = true;

        MsfGifState *gifState = new MsfGifState;
        msf_gif_begin(gifState, mWidth, mHeight);
        mGifHandle = (void *)gifState;

        UpdateWindowTitle();
        std::printf("Started GIF capture (%d fps)\n", frameRate);
    }

    void Device::StopGifCapture()
    {
        if (!mGifCapturing)
            return;

        mGifCapturing = false;
        UpdateWindowTitle();

        MsfGifState *gifState = static_cast<MsfGifState *>(mGifHandle);

        MsfGifResult result = msf_gif_end(gifState);
        if (result.data)
        {
            char filename[256];
            std::snprintf(filename, sizeof(filename), "capture_%04u.gif", mGifFileIndex++);

            FILE *fp = std::fopen(filename, "wb");
            if (fp)
            {
                std::fwrite(result.data, 1, result.dataSize, fp);
                std::fclose(fp);
                std::printf("GIF saved: %s (%d frames)\n", filename, mGifFrameCounter);
            }
            msf_gif_free(result);
        }

        delete gifState;
        mGifHandle = nullptr;
    }

    void Device::CaptureGifFrame()
    {
        if (!mGifCapturing)
            return;

        MsfGifState *gifState = static_cast<MsfGifState *>(mGifHandle);

        unsigned char *screenData = new unsigned char[mWidth * mHeight * 4];
        glReadPixels(0, 0, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, screenData);

        unsigned char *flipped = new unsigned char[mWidth * mHeight * 4];
        int stride = mWidth * 4;

        for (int y = mHeight - 1; y >= 0; --y)
        {
            for (int x = 0; x < stride; ++x)
            {
                flipped[((mHeight - 1) - y) * stride + x] = screenData[(y * stride) + x];

                if (((x + 1) % 4) == 0)
                    flipped[((mHeight - 1) - y) * stride + x] = 255;
            }
        }

        int centiSecondsPerFrame = (int)(100.0f / mGifFrameRate + 0.5f);
        msf_gif_frame(gifState, (uint8_t *)flipped, centiSecondsPerFrame, 16, stride);

        delete[] screenData;
        delete[] flipped;
        mGifFrameCounter++;
    }

}
