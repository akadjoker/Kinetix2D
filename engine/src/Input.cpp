#include "k2d/Input.h"

#include <platform.h>

#include <cmath>

namespace k2d
{

    Input::Input()
        : mMouseX(0.0f), mMouseY(0.0f), mWheelY(0.0f)
    {
        for (int i = 0; i < MAX_KEYS; ++i)
        {
            mKeys[i] = false;
            mKeysPressed[i] = false;
            mKeysReleased[i] = false;
            mVirtualKeys[i] = false;
            mVirtualKeysPressed[i] = false;
            mVirtualKeysReleased[i] = false;
        }
        for (int i = 0; i < MAX_BUTTONS; ++i)
        {
            mButtons[i] = false;
            mButtonsPressed[i] = false;
            mButtonsReleased[i] = false;
        }
        for (int i = 0; i < MAX_TOUCH; ++i)
        {
            mTouch[i].id = 0;
            mTouch[i].x = 0.0f;
            mTouch[i].y = 0.0f;
            mTouch[i].active = false;
        }
    }

    void Input::NewFrame()
    {
        for (int i = 0; i < MAX_KEYS; ++i)
        {
            mKeysPressed[i] = false;
            mKeysReleased[i] = false;
            mVirtualKeysPressed[i] = false;
            mVirtualKeysReleased[i] = false;
        }
        for (int i = 0; i < MAX_BUTTONS; ++i)
        {
            mButtonsPressed[i] = false;
            mButtonsReleased[i] = false;
        }
        mWheelY = 0.0f;
    }

    void Input::OnKey(int scancode, bool down, bool repeat)
    {
        if (scancode < 0 || scancode >= MAX_KEYS)
            return;

        if (repeat)
        {
            mKeys[scancode] = down;
            return;
        }

        if (down)
        {
            mKeys[scancode] = true;
            mKeysPressed[scancode] = true;
        }
        else
        {
            mKeys[scancode] = false;
            mKeysReleased[scancode] = !mVirtualKeys[scancode];
        }
    }

    void Input::OnMouseButton(int button, bool down)
    {
        if (button < 0 || button >= MAX_BUTTONS)
            return;

        if (down)
        {
            mButtons[button] = true;
            mButtonsPressed[button] = true;
        }
        else
        {
            mButtons[button] = false;
            mButtonsReleased[button] = true;
        }
    }

    void Input::OnMouseMove(float x, float y)
    {
        mMouseX = x;
        mMouseY = y;
    }

    void Input::OnWheel(float y)
    {
        mWheelY += y;
    }

    void Input::OnTouch(long long id, float x, float y, bool down, bool up)
    {
        if (mTouchAsMouse && (mMouseTouchActive ? id == mMouseTouch : down))
        {
            OnMouseMove(x, y);
            if (down || up)
                OnMouseButton(0, down);
            mMouseTouchActive = down || (!up && mMouseTouchActive);
            mMouseTouch = id;
        }
        if (down)
        {
            int freeSlot = -1;
            for (int i = 0; i < MAX_TOUCH; ++i)
            {
                if (mTouch[i].active && mTouch[i].id == id)
                {
                    freeSlot = i;
                    break;
                }
                if (freeSlot == -1 && !mTouch[i].active)
                    freeSlot = i;
            }
            if (freeSlot != -1)
            {
                mTouch[freeSlot].id = id;
                mTouch[freeSlot].x = x;
                mTouch[freeSlot].y = y;
                mTouch[freeSlot].active = true;
            }
            return;
        }

        if (up)
        {
            for (int i = 0; i < MAX_TOUCH; ++i)
            {
                if (mTouch[i].active && mTouch[i].id == id)
                {
                    mTouch[i].active = false;
                    mTouch[i].x = x;
                    mTouch[i].y = y;
                    break;
                }
            }
            return;
        }

        for (int i = 0; i < MAX_TOUCH; ++i)
        {
            if (mTouch[i].active && mTouch[i].id == id)
            {
                mTouch[i].x = x;
                mTouch[i].y = y;
                break;
            }
        }
    }

    void Input::SetVirtualKey(int scancode, bool down)
    {
        if (scancode < 0 || scancode >= MAX_KEYS || mVirtualKeys[scancode] == down)
            return;

        mVirtualKeys[scancode] = down;
        if (down)
            mVirtualKeysPressed[scancode] = true;
        else if (!mKeys[scancode])
            mVirtualKeysReleased[scancode] = true;
    }

    bool Input::KeyDown(int scancode) const
    {
        if (scancode < 0 || scancode >= MAX_KEYS)
            return false;
        return mKeys[scancode] || mVirtualKeys[scancode];
    }

    bool Input::KeyPressed(int scancode) const
    {
        if (scancode < 0 || scancode >= MAX_KEYS)
            return false;
        return mKeysPressed[scancode] || mVirtualKeysPressed[scancode];
    }

    bool Input::KeyReleased(int scancode) const
    {
        if (scancode < 0 || scancode >= MAX_KEYS)
            return false;
        return mKeysReleased[scancode] || mVirtualKeysReleased[scancode];
    }

    bool Input::MouseDown(int button) const
    {
        if (button < 0 || button >= MAX_BUTTONS)
            return false;
        return mButtons[button];
    }

    bool Input::MousePressed(int button) const
    {
        if (button < 0 || button >= MAX_BUTTONS)
            return false;
        return mButtonsPressed[button];
    }

    bool Input::MouseReleased(int button) const
    {
        if (button < 0 || button >= MAX_BUTTONS)
            return false;
        return mButtonsReleased[button];
    }

    void Input::PollGamepads()
    {
        for (int pad = 0; pad < MAX_GAMEPADS; ++pad)
        {
            bool buttons[PAD_BUTTONS];
            float axes[PAD_AXES];
            const bool connected = gamepad_connected(pad);
            for (int b = 0; b < PAD_BUTTONS; ++b)
                buttons[b] = connected && gamepad_button_down(pad, b);
            for (int a = 0; a < PAD_AXES; ++a)
                axes[a] = connected ? gamepad_axis(pad, a) : 0.0f;
            SetGamepad(pad, connected, buttons, axes);
        }
    }

    void Input::SetGamepad(int pad, bool connected, const bool *buttons, const float *axes)
    {
        if (pad < 0 || pad >= MAX_GAMEPADS)
            return;
        Pad &state = mPads[pad];
        state.connected = connected;
        for (int b = 0; b < PAD_BUTTONS; ++b)
        {
            state.previous[b] = state.buttons[b];
            state.buttons[b] = connected && buttons && buttons[b];
        }
        for (int a = 0; a < PAD_AXES; ++a)
        {
            state.previousAxes[a] = state.axes[a];
            state.axes[a] = connected && axes ? axes[a] : 0.0f;
        }
    }

    bool Input::GamepadConnected(int pad) const
    {
        if (pad >= 0)
            return pad < MAX_GAMEPADS && mPads[pad].connected;
        for (const Pad &state : mPads)
            if (state.connected)
                return true;
        return false;
    }

    bool Input::PadDown(int button, int pad) const
    {
        if (button < 0 || button >= PAD_BUTTONS)
            return false;
        for (int i = 0; i < MAX_GAMEPADS; ++i)
            if ((pad < 0 || pad == i) && mPads[i].buttons[button])
                return true;
        return false;
    }

    bool Input::PadPressed(int button, int pad) const
    {
        if (button < 0 || button >= PAD_BUTTONS)
            return false;
        for (int i = 0; i < MAX_GAMEPADS; ++i)
            if ((pad < 0 || pad == i) && mPads[i].buttons[button] && !mPads[i].previous[button])
                return true;
        return false;
    }

    bool Input::PadReleased(int button, int pad) const
    {
        if (button < 0 || button >= PAD_BUTTONS)
            return false;
        for (int i = 0; i < MAX_GAMEPADS; ++i)
            if ((pad < 0 || pad == i) && !mPads[i].buttons[button] && mPads[i].previous[button])
                return true;
        return false;
    }

    float Input::padAxis(const float *axes, int axis) const
    {
        const float deadZone = 0.15f;
        const float v = axes[axis];
        const float magnitude = std::fabs(v);
        if (magnitude <= deadZone)
            return 0.0f;
        const float scaled = ((magnitude > 1.0f ? 1.0f : magnitude) - deadZone) / (1.0f - deadZone);
        return v < 0.0f ? -scaled : scaled;
    }

    float Input::PadAxisValue(int axis, int pad) const
    {
        if (axis < 0 || axis >= PAD_AXES)
            return 0.0f;
        float best = 0.0f;
        for (int i = 0; i < MAX_GAMEPADS; ++i)
            if (pad < 0 || pad == i)
            {
                const float v = padAxis(mPads[i].axes, axis);
                if (std::fabs(v) > std::fabs(best))
                    best = v;
            }
        return best;
    }

    float Input::PadAxisPrevious(int axis, int pad) const
    {
        if (axis < 0 || axis >= PAD_AXES)
            return 0.0f;
        float best = 0.0f;
        for (int i = 0; i < MAX_GAMEPADS; ++i)
            if (pad < 0 || pad == i)
            {
                const float v = padAxis(mPads[i].previousAxes, axis);
                if (std::fabs(v) > std::fabs(best))
                    best = v;
            }
        return best;
    }

    int Input::TouchCount() const
    {
        int count = 0;
        for (int i = 0; i < MAX_TOUCH; ++i)
        {
            if (mTouch[i].active)
                ++count;
        }
        return count;
    }

}
