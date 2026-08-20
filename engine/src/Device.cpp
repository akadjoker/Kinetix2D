#include "k2d/Device.h"

#include <SDL.h>
#include <glad/glad.h>
#include <cstdio>

namespace k2d
{

    Device::Device()
        : mWindow(nullptr), mGLContext(nullptr), mWidth(0), mHeight(0),
          mResized(false), mLastCounter(0), mDeltaTime(0.0f)
    {
    }

    Device::~Device()
    {
        Shutdown();
    }

    bool Device::Init(const char *title, int width, int height, bool vsync)
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        {
            std::printf("SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

        mWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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

        mLastCounter = SDL_GetPerformanceCounter();
        mDeltaTime = 0.0f;

        return true;
    }

    void Device::Shutdown()
    {
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
        SDL_Quit();
    }

    bool Device::PollEvents()
    {
        mInput.NewFrame();
        mResized = false;

        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
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
                    }
                }
                break;

            default:
                break;
            }
        }

        unsigned long long now = SDL_GetPerformanceCounter();
        unsigned long long freq = SDL_GetPerformanceFrequency();
        float dt = (float)((double)(now - mLastCounter) / (double)freq);
        mLastCounter = now;
        if (dt > 0.25f)
            dt = 0.25f;
        mDeltaTime = dt;

        return true;
    }

    void Device::Swap()
    {
        SDL_GL_SwapWindow(mWindow);
    }

}
