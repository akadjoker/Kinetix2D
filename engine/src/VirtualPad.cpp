#include "k2d/VirtualPad.h"

#include "k2d/Assets.h"
#include "k2d/CanvasRenderer.h"
#include "k2d/Color.h"
#include "k2d/Input.h"
#include "k2d/RenderQueue.h"
#include "k2d/Texture.h"

#include "k2d_default_virtual_pad_png.h"

#include <cmath>

namespace k2d
{
    namespace
    {
        float distanceSquared(float x0, float y0, float x1, float y1)
        {
            const float x = x1 - x0;
            const float y = y1 - y0;
            return x * x + y * y;
        }

        void addAtlasRect(RenderItem &item, const Texture &texture, float x, float y, float width, float height,
                          float sourceX, float sourceY, float sourceWidth, float sourceHeight, const Color &color)
        {
            RenderCommand command = RenderCommand::MakeRect(texture.Id(), x, y, width, height);
            command.pivotX = 0.0f;
            command.pivotY = 0.0f;
            command.srcX = sourceX;
            command.srcY = sourceY;
            command.srcW = sourceWidth;
            command.srcH = sourceHeight;
            command.texWidth = texture.Width();
            command.texHeight = texture.Height();
            command.color = color;
            item.commands.push_back(command);
        }
    }

    VirtualPad::VirtualPad()
        // Touch controls are opt-in. A game enables them explicitly through
        // its scene's `virtualPad.enabled` setting, rather than showing an
        // overlay just because it happens to run on the Web or on a phone.
        : mEnabled(false), mCustomKeysVisible(true), mTexture(nullptr),
          mLeftKey(-1), mRightKey(-1), mUpKey(-1), mDownKey(-1), mPrimaryKey(-1), mSecondaryKey(-1),
          mScale(1.0f), mOpacity(0.58f), mIdleOpacity(0.14f), mStickOpacity(0.14f),
          mPrimaryOpacity(0.14f), mSecondaryOpacity(0.14f), mStick(0.0f),
          mStickTouchId(0), mStickCaptured(false),
          mStickUsesMouse(false), mPrimaryDown(false), mSecondaryDown(false), mCustomKeys(), mRetiredCustomKeys()
    {
    }

    Texture *VirtualPad::DefaultTexture(Assets &assets)
    {
        static const char *const name = "__k2d_default_virtual_pad";
        if (Texture *texture = assets.GetTexture(name))
            return texture;
        return assets.LoadTextureMemory(name, kDefaultVirtualPadPng, kDefaultVirtualPadPngSize, true, false);
    }

    void VirtualPad::SetKeyBindings(int left, int right, int up, int down, int primary, int secondary)
    {
        mLeftKey = left;
        mRightKey = right;
        mUpKey = up;
        mDownKey = down;
        mPrimaryKey = primary;
        mSecondaryKey = secondary;
    }

    void VirtualPad::AddVirtualKey(int scancode, float x, float y, float width, float height)
    {
        if (scancode < 0 || width <= 0.0f || height <= 0.0f)
            return;
        VirtualKey key;
        key.scancode = scancode;
        key.x = x;
        key.y = y;
        key.width = width;
        key.height = height;
        key.down = false;
        mCustomKeys.push_back(key);
    }

    void VirtualPad::ClearVirtualKeys()
    {
        for (std::size_t i = 0; i < mCustomKeys.size(); ++i)
            mRetiredCustomKeys.push_back(mCustomKeys[i].scancode);
        mCustomKeys.clear();
    }

    void VirtualPad::Update(Input &input, float screenWidth, float screenHeight, float deltaTime)
    {
        Update(input, 0.0f, 0.0f, screenWidth, screenHeight, screenWidth, screenHeight, deltaTime);
    }

    void VirtualPad::Update(Input &input, float viewportX, float viewportY, float viewportWidth, float viewportHeight,
                            float virtualWidth, float virtualHeight, float deltaTime)
    {
        const float screenWidth = virtualWidth;
        const float screenHeight = virtualHeight;
        const auto mapX = [viewportX, viewportWidth, virtualWidth](float value)
        {
            return viewportWidth > 0.0f ? (value - viewportX) * virtualWidth / viewportWidth : value;
        };
        const auto mapY = [viewportY, viewportHeight, virtualHeight](float value)
        {
            return viewportHeight > 0.0f ? (value - viewportY) * virtualHeight / viewportHeight : value;
        };
        mStick = Math::Vec2(0.0f);
        mPrimaryDown = false;
        mSecondaryDown = false;
        for (std::size_t i = 0; i < mCustomKeys.size(); ++i)
            mCustomKeys[i].down = false;

        if (screenWidth > 0.0f && screenHeight > 0.0f)
        {
            const auto handleCustomKeys = [this](float x, float y)
            {
                for (std::size_t i = 0; i < mCustomKeys.size(); ++i)
                {
                    VirtualKey &key = mCustomKeys[i];
                    if (x >= key.x && x <= key.x + key.width && y >= key.y && y <= key.y + key.height)
                        key.down = true;
                }
            };

            for (int index = 0; index < Input::MAX_TOUCH; ++index)
            {
                const Input::Touch &touch = input.GetTouch(index);
                if (touch.active)
                    handleCustomKeys(mapX(touch.x), mapY(touch.y));
            }
            if (input.MouseDown(0))
                handleCustomKeys(mapX(input.MouseX()), mapY(input.MouseY()));

            if (mEnabled)
            {
            const float radius = (screenHeight < screenWidth ? screenHeight : screenWidth) * 0.115f * mScale;
            const float margin = radius * 0.55f;
            const float stickX = margin + radius;
            const float stickY = screenHeight - margin - radius;
            const float primaryX = screenWidth - margin - radius;
            const float primaryY = stickY;
            const float secondaryX = primaryX - radius * 2.15f;
            const float buttonRadius = radius * 0.66f;

            const auto setStick = [this, radius, stickX, stickY](float x, float y)
            {
                const float dx = x - stickX;
                const float dy = y - stickY;
                const float length = std::sqrt(dx * dx + dy * dy);
                if (length > radius * 0.18f)
                {
                    const float scale = length > radius ? radius / length : 1.0f;
                    mStick = Math::Vec2(dx * scale / radius, dy * scale / radius);
                }
            };

            const auto handleButtons = [this, primaryX, primaryY, secondaryX, buttonRadius](float x, float y)
            {
                if (distanceSquared(x, y, primaryX, primaryY) <= buttonRadius * buttonRadius)
                    mPrimaryDown = true;
                if (distanceSquared(x, y, secondaryX, primaryY) <= buttonRadius * buttonRadius)
                    mSecondaryDown = true;
            };

            // A stick captures the pointer that began inside it. The captured
            // pointer is then clamped to the edge even when it moves outside,
            // which keeps a held direction instead of snapping back to zero.
            bool stickPointerActive = false;
            if (mStickCaptured)
            {
                if (mStickUsesMouse)
                {
                    if (input.MouseDown(0))
                    {
                        setStick(mapX(input.MouseX()), mapY(input.MouseY()));
                        stickPointerActive = true;
                    }
                }
                else
                {
                    for (int index = 0; index < Input::MAX_TOUCH; ++index)
                    {
                        const Input::Touch &touch = input.GetTouch(index);
                        if (touch.active && touch.id == mStickTouchId)
                        {
                            setStick(mapX(touch.x), mapY(touch.y));
                            stickPointerActive = true;
                            break;
                        }
                    }
                }
                if (!stickPointerActive)
                    mStickCaptured = false;
            }

            for (int index = 0; index < Input::MAX_TOUCH; ++index)
            {
                const Input::Touch &touch = input.GetTouch(index);
                if (!touch.active)
                    continue;
                const float touchX = mapX(touch.x);
                const float touchY = mapY(touch.y);
                handleButtons(touchX, touchY);
                if (!mStickCaptured && distanceSquared(touchX, touchY, stickX, stickY) <= radius * radius * 1.45f)
                {
                    mStickCaptured = true;
                    mStickUsesMouse = false;
                    mStickTouchId = touch.id;
                    setStick(touchX, touchY);
                    stickPointerActive = true;
                }
            }

            // Mouse makes the pad testable on desktop and acts as the
            // single-pointer fallback on web platforms without touch input.
            if (input.MouseDown(0))
            {
                const float mouseX = mapX(input.MouseX());
                const float mouseY = mapY(input.MouseY());
                handleButtons(mouseX, mouseY);
                if (!mStickCaptured && distanceSquared(mouseX, mouseY, stickX, stickY) <= radius * radius * 1.45f)
                {
                    mStickCaptured = true;
                    mStickUsesMouse = true;
                    setStick(mouseX, mouseY);
                    stickPointerActive = true;
                }
            }
            }
        }

        const auto customKeyDown = [this](int scancode)
        {
            for (std::size_t i = 0; i < mCustomKeys.size(); ++i)
                if (mCustomKeys[i].scancode == scancode && mCustomKeys[i].down)
                    return true;
            return false;
        };
        input.SetVirtualKey(mLeftKey, mStick.x < -0.35f || customKeyDown(mLeftKey));
        input.SetVirtualKey(mRightKey, mStick.x > 0.35f || customKeyDown(mRightKey));
        input.SetVirtualKey(mUpKey, mStick.y < -0.35f || customKeyDown(mUpKey));
        input.SetVirtualKey(mDownKey, mStick.y > 0.35f || customKeyDown(mDownKey));
        input.SetVirtualKey(mPrimaryKey, mPrimaryDown || customKeyDown(mPrimaryKey));
        input.SetVirtualKey(mSecondaryKey, mSecondaryDown || customKeyDown(mSecondaryKey));
        for (std::size_t i = 0; i < mCustomKeys.size(); ++i)
        {
            const int key = mCustomKeys[i].scancode;
            if (key != mLeftKey && key != mRightKey && key != mUpKey && key != mDownKey && key != mPrimaryKey && key != mSecondaryKey)
                input.SetVirtualKey(key, customKeyDown(key));
        }
        for (std::size_t i = 0; i < mRetiredCustomKeys.size(); ++i)
            input.SetVirtualKey(mRetiredCustomKeys[i], false);
        mRetiredCustomKeys.clear();

        const float step = (deltaTime > 0.0f ? deltaTime : 0.0f) * 5.0f;
        const auto fadeTo = [step](float &current, float target)
        {
            if (current < target)
                current = current + step < target ? current + step : target;
            else if (current > target)
                current = current - step > target ? current - step : target;
        };
        fadeTo(mStickOpacity, mEnabled && mStickCaptured ? mOpacity : mIdleOpacity);
        fadeTo(mPrimaryOpacity, mEnabled && mPrimaryDown ? mOpacity : mIdleOpacity);
        fadeTo(mSecondaryOpacity, mEnabled && mSecondaryDown ? mOpacity : mIdleOpacity);
    }

    void VirtualPad::Draw(CanvasRenderer &canvas, float screenWidth, float screenHeight) const
    {
        if (screenWidth <= 0.0f || screenHeight <= 0.0f)
            return;

        if (!mEnabled && (!mCustomKeysVisible || mCustomKeys.empty()))
            return;

        const float radius = (screenHeight < screenWidth ? screenHeight : screenWidth) * 0.115f * mScale;
        const float margin = radius * 0.55f;
        const float stickX = margin + radius;
        const float stickY = screenHeight - margin - radius;
        const float primaryX = screenWidth - margin - radius;
        const float secondaryX = primaryX - radius * 2.15f;
        const float buttonRadius = radius * 0.66f;
        const float knobRadius = radius * 0.58f;
        canvas.SetOrtho(screenWidth, screenHeight);
        RenderQueue overlay;
        RenderItem &item = overlay.AddItem(0);
        if (mEnabled && mTexture)
        {
            const Color stickColor(1.0f, 1.0f, 1.0f, mStickOpacity);
            addAtlasRect(item, *mTexture, stickX - radius, stickY - radius, radius * 2.0f, radius * 2.0f,
                         0.0f, 0.0f, 256.0f, 256.0f, stickColor);
            addAtlasRect(item, *mTexture,
                         stickX + mStick.x * (radius - knobRadius) - knobRadius,
                         stickY + mStick.y * (radius - knobRadius) - knobRadius,
                         knobRadius * 2.0f, knobRadius * 2.0f,
                         256.0f, 0.0f, 256.0f, 256.0f, stickColor);
            addAtlasRect(item, *mTexture, secondaryX - buttonRadius, stickY - buttonRadius,
                         buttonRadius * 2.0f, buttonRadius * 2.0f,
                         0.0f, 256.0f, 170.0f, 170.0f,
                         Color(1.0f, 1.0f, 1.0f, mSecondaryOpacity));
            addAtlasRect(item, *mTexture, primaryX - buttonRadius, stickY - buttonRadius,
                         buttonRadius * 2.0f, buttonRadius * 2.0f,
                         256.0f, 256.0f, 170.0f, 170.0f,
                         Color(1.0f, 1.0f, 1.0f, mPrimaryOpacity));
        }
        if (mCustomKeysVisible)
            for (std::size_t i = 0; i < mCustomKeys.size(); ++i)
            {
                const VirtualKey &key = mCustomKeys[i];
                RenderCommand command = RenderCommand::MakeRect(0, key.x, key.y, key.width, key.height);
                command.pivotX = 0.0f;
                command.pivotY = 0.0f;
                command.color = key.down ? Color(1.0f, 0.55f, 0.2f, 0.78f) : Color(0.12f, 0.18f, 0.30f, 0.64f);
                item.commands.push_back(command);
            }
        overlay.Flush(canvas);
    }
}
