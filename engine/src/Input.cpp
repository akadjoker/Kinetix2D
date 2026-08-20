#include "k2d/Input.h"

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
            mKeysReleased[scancode] = true;
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

    bool Input::KeyDown(int scancode) const
    {
        if (scancode < 0 || scancode >= MAX_KEYS)
            return false;
        return mKeys[scancode];
    }

    bool Input::KeyPressed(int scancode) const
    {
        if (scancode < 0 || scancode >= MAX_KEYS)
            return false;
        return mKeysPressed[scancode];
    }

    bool Input::KeyReleased(int scancode) const
    {
        if (scancode < 0 || scancode >= MAX_KEYS)
            return false;
        return mKeysReleased[scancode];
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
