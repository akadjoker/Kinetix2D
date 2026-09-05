#pragma once

#include <ct/vector.hpp>
#include <mathc.h>

namespace k2d
{
    class Assets;
    class CanvasRenderer;
    class Input;
    class Texture;

    // Touch/mouse controller for mobile and web builds. It synthesizes
    // configurable keyboard scancodes, so existing gameplay input works
    // unchanged with physical keys, touch, or mouse dragging.
    class VirtualPad
    {
    public:
        VirtualPad();

        static Texture *DefaultTexture(Assets &assets);

        void SetEnabled(bool enabled) { mEnabled = enabled; }
        bool Enabled() const { return mEnabled; }
        void SetTexture(Texture *texture) { mTexture = texture; }
        void SetKeyBindings(int left, int right, int up, int down, int primary, int secondary);
        void SetScale(float scale) { mScale = scale > 0.25f ? scale : 0.25f; }
        void SetOpacity(float opacity) { mOpacity = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity); }
        void SetIdleOpacity(float opacity) { mIdleOpacity = opacity < 0.0f ? 0.0f : (opacity > 1.0f ? 1.0f : opacity); }

        // Script-defined rectangular touch buttons. They are independent from
        // the optional built-in stick/pair of action buttons.
        void AddVirtualKey(int scancode, float x, float y, float width, float height);
        void ClearVirtualKeys();
        void SetVirtualKeysVisible(bool visible) { mCustomKeysVisible = visible; }
        bool VirtualKeysVisible() const { return mCustomKeysVisible; }

        void Update(Input &input, float screenWidth, float screenHeight, float deltaTime = 1.0f / 60.0f);
        void Update(Input &input, float viewportX, float viewportY, float viewportWidth, float viewportHeight,
                    float virtualWidth, float virtualHeight, float deltaTime);
        void Draw(CanvasRenderer &canvas, float screenWidth, float screenHeight) const;

        Math::Vec2 Stick() const { return mStick; }
        bool PrimaryDown() const { return mPrimaryDown; }
        bool SecondaryDown() const { return mSecondaryDown; }

        // Preferred camelCase API. PascalCase names above remain compatible.
        static Texture *defaultTexture(Assets &assets) { return DefaultTexture(assets); }
        void setEnabled(bool enabled) { SetEnabled(enabled); }
        bool enabled() const { return Enabled(); }
        void setTexture(Texture *texture) { SetTexture(texture); }
        void setKeyBindings(int left, int right, int up, int down, int primary, int secondary)
        { SetKeyBindings(left, right, up, down, primary, secondary); }
        void setScale(float scale) { SetScale(scale); }
        void setOpacity(float opacity) { SetOpacity(opacity); }
        void setIdleOpacity(float opacity) { SetIdleOpacity(opacity); }
        void addVirtualKey(int scancode, float x, float y, float width, float height)
        { AddVirtualKey(scancode, x, y, width, height); }
        void clearVirtualKeys() { ClearVirtualKeys(); }
        void setVirtualKeysVisible(bool visible) { SetVirtualKeysVisible(visible); }
        bool virtualKeysVisible() const { return VirtualKeysVisible(); }
        void update(Input &input, float screenWidth, float screenHeight, float deltaTime = 1.0f / 60.0f)
        { Update(input, screenWidth, screenHeight, deltaTime); }
        void update(Input &input, float viewportX, float viewportY, float viewportWidth, float viewportHeight,
                    float virtualWidth, float virtualHeight, float deltaTime)
        { Update(input, viewportX, viewportY, viewportWidth, viewportHeight, virtualWidth, virtualHeight, deltaTime); }
        void draw(CanvasRenderer &canvas, float screenWidth, float screenHeight) const { Draw(canvas, screenWidth, screenHeight); }
        Math::Vec2 stick() const { return Stick(); }
        bool primaryDown() const { return PrimaryDown(); }
        bool secondaryDown() const { return SecondaryDown(); }

    private:
        struct VirtualKey
        {
            int scancode;
            float x;
            float y;
            float width;
            float height;
            bool down;
        };

        bool mEnabled;
        bool mCustomKeysVisible;
        Texture *mTexture;
        int mLeftKey;
        int mRightKey;
        int mUpKey;
        int mDownKey;
        int mPrimaryKey;
        int mSecondaryKey;
        float mScale;
        float mOpacity;
        float mIdleOpacity;
        float mStickOpacity;
        float mPrimaryOpacity;
        float mSecondaryOpacity;
        Math::Vec2 mStick;
        long long mStickTouchId;
        bool mStickCaptured;
        bool mStickUsesMouse;
        bool mPrimaryDown;
        bool mSecondaryDown;
        ct::Vector<VirtualKey> mCustomKeys;
        ct::Vector<int> mRetiredCustomKeys;
    };
}
