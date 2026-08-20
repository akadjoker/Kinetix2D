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

    private:
        bool mKeys[MAX_KEYS];
        bool mKeysPressed[MAX_KEYS];
        bool mKeysReleased[MAX_KEYS];
        bool mButtons[MAX_BUTTONS];
        bool mButtonsPressed[MAX_BUTTONS];
        bool mButtonsReleased[MAX_BUTTONS];
        float mMouseX;
        float mMouseY;
        float mWheelY;
        Touch mTouch[MAX_TOUCH];
    };

} // namespace k2d
