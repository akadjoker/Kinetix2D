#pragma once

namespace k2d
{

    class Input
    {
    public:
        static const int MAX_KEYS = 512;
        static const int MAX_BUTTONS = 4;
        static const int MAX_TOUCH = 8;

        struct Touch
        {
            long long id;
            float x, y;
            bool active;
        };

        Input();

        void NewFrame();

        void OnKey(int scancode, bool down, bool repeat);
        void OnMouseButton(int button, bool down);
        void OnMouseMove(float x, float y);
        void OnWheel(float y);
        void OnTouch(long long id, float x, float y, bool down, bool up);
        // Synthesized input is kept independent from physical keyboard state,
        // allowing touch controls to share the same game actions safely.
        void SetVirtualKey(int scancode, bool down);

        bool KeyDown(int scancode) const;
        bool KeyPressed(int scancode) const;
        bool KeyReleased(int scancode) const;

        bool MouseDown(int button) const;
        bool MousePressed(int button) const;
        bool MouseReleased(int button) const;
        float MouseX() const { return mMouseX; }
        float MouseY() const { return mMouseY; }
        float WheelY() const { return mWheelY; }

        int TouchCount() const;
        const Touch &GetTouch(int index) const { return mTouch[index & (MAX_TOUCH - 1)]; }

        // Preferred camelCase API. PascalCase names above remain for source compatibility.
        void newFrame() { NewFrame(); }
        void onKey(int scancode, bool down, bool repeat) { OnKey(scancode, down, repeat); }
        void onMouseButton(int button, bool down) { OnMouseButton(button, down); }
        void onMouseMove(float x, float y) { OnMouseMove(x, y); }
        void onWheel(float y) { OnWheel(y); }
        void onTouch(long long id, float x, float y, bool down, bool up) { OnTouch(id, x, y, down, up); }
        void setVirtualKey(int scancode, bool down) { SetVirtualKey(scancode, down); }
        bool keyDown(int scancode) const { return KeyDown(scancode); }
        bool keyPressed(int scancode) const { return KeyPressed(scancode); }
        bool keyReleased(int scancode) const { return KeyReleased(scancode); }
        bool mouseDown(int button) const { return MouseDown(button); }
        bool mousePressed(int button) const { return MousePressed(button); }
        bool mouseReleased(int button) const { return MouseReleased(button); }
        float mouseX() const { return MouseX(); }
        float mouseY() const { return MouseY(); }
        float wheelY() const { return WheelY(); }
        int touchCount() const { return TouchCount(); }
        const Touch &touch(int index) const { return GetTouch(index); }

    private:
        bool mKeys[MAX_KEYS];
        bool mKeysPressed[MAX_KEYS];
        bool mKeysReleased[MAX_KEYS];
        bool mVirtualKeys[MAX_KEYS];
        bool mVirtualKeysPressed[MAX_KEYS];
        bool mVirtualKeysReleased[MAX_KEYS];
        bool mButtons[MAX_BUTTONS];
        bool mButtonsPressed[MAX_BUTTONS];
        bool mButtonsReleased[MAX_BUTTONS];
        float mMouseX;
        float mMouseY;
        float mWheelY;
        Touch mTouch[MAX_TOUCH];
    };

}
