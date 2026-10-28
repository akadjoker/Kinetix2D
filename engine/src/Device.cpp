#include "k2d/Device.h"

#include "k2d/FileSystem.h"
#include "k2d/OpenGL.h"

#include <platform.h>

#include <cstdio>
#include <cstring>

#include "k2d/Image.h"

#define MSF_GIF_IMPL
#include <msf_gif.h>

namespace k2d
{
namespace
{
int toScancode(int key)
{
    if (key >= KEY_A && key <= KEY_Z)
        return (int)Key::A + (key - KEY_A);
    if (key >= KEY_ONE && key <= KEY_NINE)
        return (int)Key::Num1 + (key - KEY_ONE);
    if (key >= KEY_F1 && key <= KEY_F12)
        return (int)Key::F1 + (key - KEY_F1);
    if (key >= KEY_KP_1 && key <= KEY_KP_9)
        return 89 + (key - KEY_KP_1);
    switch (key)
    {
    case KEY_ZERO: return (int)Key::Num0;
    case KEY_ENTER: return (int)Key::Enter;
    case KEY_ESCAPE: return (int)Key::Escape;
    case KEY_BACKSPACE: return (int)Key::Backspace;
    case KEY_TAB: return (int)Key::Tab;
    case KEY_SPACE: return (int)Key::Space;
    case KEY_MINUS: return 45;
    case KEY_EQUAL: return 46;
    case KEY_LEFT_BRACKET: return 47;
    case KEY_RIGHT_BRACKET: return 48;
    case KEY_BACKSLASH: return 49;
    case KEY_SEMICOLON: return 51;
    case KEY_APOSTROPHE: return 52;
    case KEY_GRAVE: return 53;
    case KEY_COMMA: return 54;
    case KEY_PERIOD: return 55;
    case KEY_SLASH: return 56;
    case KEY_CAPS_LOCK: return 57;
    case KEY_PRINT_SCREEN: return 70;
    case KEY_SCROLL_LOCK: return 71;
    case KEY_PAUSE: return 72;
    case KEY_INSERT: return 73;
    case KEY_HOME: return 74;
    case KEY_PAGE_UP: return 75;
    case KEY_DELETE: return 76;
    case KEY_END: return 77;
    case KEY_PAGE_DOWN: return 78;
    case KEY_RIGHT: return (int)Key::Right;
    case KEY_LEFT: return (int)Key::Left;
    case KEY_DOWN: return (int)Key::Down;
    case KEY_UP: return (int)Key::Up;
    case KEY_NUM_LOCK: return 83;
    case KEY_KP_DIVIDE: return 84;
    case KEY_KP_MULTIPLY: return 85;
    case KEY_KP_SUBTRACT: return 86;
    case KEY_KP_ADD: return 87;
    case KEY_KP_ENTER: return 88;
    case KEY_KP_0: return 98;
    case KEY_KP_DECIMAL: return 99;
    case KEY_MENU: return 101;
    case KEY_KP_EQUAL: return 103;
    case KEY_LEFT_CONTROL: return (int)Key::LeftCtrl;
    case KEY_LEFT_SHIFT: return (int)Key::LeftShift;
    case KEY_LEFT_ALT: return (int)Key::LeftAlt;
    case KEY_LEFT_SUPER: return 227;
    case KEY_RIGHT_CONTROL: return (int)Key::RightCtrl;
    case KEY_RIGHT_SHIFT: return (int)Key::RightShift;
    case KEY_RIGHT_ALT: return 230;
    case KEY_RIGHT_SUPER: return 231;
    default: return -1;
    }
}

int toMouseButton(int button)
{
    switch (button)
    {
    case MOUSE_LEFT: return 0;
    case MOUSE_MIDDLE: return 1;
    case MOUSE_RIGHT: return 2;
    default: return button;
    }
}

PlatformWindow* createWindow(WindowConfig& config)
{
    config.gl.profile = GL_PROFILE_ES;
    config.gl.major = 3;
    config.gl.minor = 0;
    if (PlatformWindow* window = window_create(&config))
        return window;
    log_warn("OpenGL ES 3.0 context unavailable (%s), falling back to OpenGL 3.3 core", platform_get_error());
    platform_clear_error();
    config.gl.profile = GL_PROFILE_CORE;
    config.gl.major = 3;
    config.gl.minor = 3;
    return window_create(&config);
}
} // namespace

Device::Device()
    : mWindow(nullptr), mWidth(0), mHeight(0), mDrawableWidth(0), mDrawableHeight(0), mResized(false),
      mLastTime(0.0), mDeltaTime(0.0f), mFps(0.0f), mFpsAccumulator(0.0f), mFpsFrames(0), mGifCapturing(false),
      mGifFrameRate(60), mGifFrameCounter(0), mGifHandle(nullptr), mScreenshotIndex(1), mGifFileIndex(1)
{
}

Device::~Device()
{
    Shutdown();
}

bool Device::Init(const char* title, int width, int height, bool vsync)
{
    if (!platform_init())
    {
        log_error("platform_init failed: %s", platform_get_error());
        return false;
    }

    FileSystem::Instance().Init();

    WindowConfig config = {};
    config.title = title;
#if defined(__EMSCRIPTEN__)
    // The game takes the canvas as the page lays it out.
    config.width = 0;
    config.height = 0;
    (void)width;
    (void)height;
#else
    config.width = width;
    config.height = height;
#endif
    MonitorInfo monitor = {};
    if (monitor_get_info(0, &monitor))
    {
        const int margin = 48;
        if (monitor.work_w > margin && config.width > monitor.work_w - margin)
            config.width = monitor.work_w - margin;
        if (monitor.work_h > margin && config.height > monitor.work_h - margin)
            config.height = monitor.work_h - margin;
    }
    config.x = WINDOW_POS_CENTERED;
    config.y = WINDOW_POS_CENTERED;
    config.render = RENDER_GL;
    config.resizable = true;
    config.vsync = vsync;

    mWindow = createWindow(config);
    if (!mWindow)
    {
        log_error("window_create failed: %s", platform_get_error());
        return false;
    }
    window_make_current(mWindow);

    if (!LoadOpenGL())
        return false;

    log_info("GL_VERSION: %s", (const char*)glGetString(GL_VERSION));
    log_info("GL_RENDERER: %s", (const char*)glGetString(GL_RENDERER));

    UpdateSizes();
    mResized = false;
    glViewport(0, 0, mDrawableWidth, mDrawableHeight);

    mLastTime = time_seconds();
    mDeltaTime = 0.0f;
    mFps = 0.0f;
    mFpsAccumulator = 0.0f;
    mFpsFrames = 0;
    mWindowTitle = title;
    return true;
}

void Device::Shutdown()
{
    if (mWindow)
    {
        window_destroy(mWindow);
        mWindow = nullptr;
        FileSystem::Instance().Shutdown();
        platform_shutdown();
    }
}

void Device::Focus()
{
    if (mWindow)
        window_focus(mWindow);
}

void Device::ShowCursor(bool visible)
{
    if (mWindow)
        mouse_set_mode(mWindow, visible ? MOUSE_MODE_NORMAL : MOUSE_MODE_HIDDEN);
}

int Device::DisplayIndex() const
{
    if (!mWindow)
        return 0;
    const int display = monitor_from_window(mWindow);
    return display >= 0 ? display : 0;
}

bool Device::SetDisplayIndex(int displayIndex)
{
    MonitorInfo monitor = {};
    if (!mWindow || displayIndex < 0 || displayIndex >= monitor_count() || !monitor_get_info(displayIndex, &monitor))
        return false;
    int width = 0;
    int height = 0;
    window_get_size(mWindow, &width, &height);
    window_set_position(mWindow, monitor.work_x + (monitor.work_w - width) / 2,
                        monitor.work_y + (monitor.work_h - height) / 2);
    return true;
}

void Device::UpdateSizes()
{
    int width = 0;
    int height = 0;
    window_get_size(mWindow, &width, &height);
    if (width != mWidth || height != mHeight)
    {
        mWidth = width;
        mHeight = height;
        mResized = true;
    }
    window_get_framebuffer_size(mWindow, &mDrawableWidth, &mDrawableHeight);
}

int InputKeyFromPlatform(int platformKey)
{
    return toScancode(platformKey);
}

int InputButtonFromPlatform(int platformButton)
{
    return toMouseButton(platformButton);
}

bool Device::PollEvents()
{
    mInput.NewFrame();
    mResized = false;

    window_begin_frame(mWindow);
    Event event;
    while (poll_event(mWindow, &event))
    {
        switch (event.type)
        {
        case EVENT_WINDOW_CLOSE:
            return false;
        case EVENT_KEY:
            mInput.OnKey(InputKeyFromPlatform(event.data.key.key), event.data.key.down, event.data.key.repeat);
            break;
        case EVENT_MOUSE_BUTTON:
            mInput.OnMouseButton(InputButtonFromPlatform(event.data.mouse.button), event.data.mouse.down);
            break;
        case EVENT_MOUSE_MOVE:
            mInput.OnMouseMove((float)event.data.mouse.x, (float)event.data.mouse.y);
            break;
        case EVENT_MOUSE_WHEEL:
            mInput.OnWheel(event.data.wheel.y);
            break;
        case EVENT_TOUCH:
            mInput.OnTouch(event.data.touch.id, event.data.touch.x, event.data.touch.y,
                           event.data.touch.phase == TOUCH_DOWN,
                           event.data.touch.phase == TOUCH_UP || event.data.touch.phase == TOUCH_CANCEL);
            break;
        default:
            break;
        }
    }
    if (window_should_close(mWindow))
        return false;

    UpdateSizes();
    glViewport(0, 0, mDrawableWidth, mDrawableHeight);

    const double now = time_seconds();
    float dt = (float)(now - mLastTime);
    mLastTime = now;
    if (dt > 0.25f)
        dt = 0.25f;
    mDeltaTime = dt;
    mFpsAccumulator += dt;
    ++mFpsFrames;
    if (mFpsAccumulator >= 0.5f)
    {
        mFps = static_cast<float>(mFpsFrames) / mFpsAccumulator;
        mFpsAccumulator = 0.0f;
        mFpsFrames = 0;
    }
    return true;
}

double Device::TimeSeconds()
{
    return time_seconds();
}

void Device::Swap()
{
    window_swap(mWindow);
}

void Device::UpdateWindowTitle()
{
    ct::String title = mWindowTitle;
    if (mGifCapturing)
        title += " [RECORDING]";
    window_set_title(mWindow, title.c_str());
}

void Device::CaptureScreenshot()
{
    unsigned char* pixels = new unsigned char[mWidth * mHeight * 4];
    glReadPixels(0, 0, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    char filename[256];
    std::snprintf(filename, sizeof(filename), "screenshot_%04u.png", mScreenshotIndex++);

    int stride = mWidth * 4;
    for (int y = 0; y < mHeight / 2; ++y)
    {
        unsigned char* top = pixels + y * stride;
        unsigned char* bottom = pixels + (mHeight - 1 - y) * stride;
        for (int x = 0; x < stride; ++x)
        {
            unsigned char tmp = top[x];
            top[x] = bottom[x];
            bottom[x] = tmp;
        }
    }

    WriteImagePng(filename, mWidth, mHeight, pixels, stride);
    log_info("Screenshot saved: %s", filename);

    delete[] pixels;
}

void Device::StartGifCapture(int frameRate)
{
    if (mGifCapturing)
        return;

    mGifFrameRate = frameRate;
    mGifFrameCounter = 0;
    mGifCapturing = true;

    MsfGifState* gifState = new MsfGifState;
    msf_gif_begin(gifState, mWidth, mHeight);
    mGifHandle = (void*)gifState;

    UpdateWindowTitle();
    log_info("Started GIF capture (%d fps)", frameRate);
}

void Device::StopGifCapture()
{
    if (!mGifCapturing)
        return;

    mGifCapturing = false;
    UpdateWindowTitle();

    MsfGifState* gifState = static_cast<MsfGifState*>(mGifHandle);

    MsfGifResult result = msf_gif_end(gifState);
    if (result.data)
    {
        char filename[256];
        std::snprintf(filename, sizeof(filename), "capture_%04u.gif", mGifFileIndex++);

        FILE* fp = std::fopen(filename, "wb");
        if (fp)
        {
            std::fwrite(result.data, 1, result.dataSize, fp);
            std::fclose(fp);
            log_info("GIF saved: %s (%d frames)", filename, mGifFrameCounter);
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

    MsfGifState* gifState = static_cast<MsfGifState*>(mGifHandle);

    unsigned char* screenData = new unsigned char[mWidth * mHeight * 4];
    glReadPixels(0, 0, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, screenData);

    unsigned char* flipped = new unsigned char[mWidth * mHeight * 4];
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
    msf_gif_frame(gifState, (uint8_t*)flipped, centiSecondsPerFrame, 16, stride);

    delete[] screenData;
    delete[] flipped;
    mGifFrameCounter++;
}

} // namespace k2d
