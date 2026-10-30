#include "k2d/InputActionMap.h"

#include "k2d/Input.h"

#include <cstring>

namespace k2d
{
namespace
{
bool matches(const ct::String &a, const char *b)
{
    return b && a == b;
}
}

const float kAxisPressed = 0.5f;

void InputActionMap::add(const char *action, Source source, int code, int direction)
{
    if (!action || !action[0] || code < 0)
        return;
    for (const Binding &binding : mBindings)
        if (matches(binding.action, action) && binding.source == source && binding.scancode == code &&
            binding.direction == direction)
            return;
    Binding binding;
    binding.action = action;
    binding.scancode = code;
    binding.source = source;
    binding.direction = direction;
    mBindings.push_back(binding);
}

void InputActionMap::Bind(const char *action, int scancode)
{
    if (scancode < Input::MAX_KEYS)
        add(action, Source::Key, scancode, 0);
}

void InputActionMap::BindPadButton(const char *action, int button)
{
    if (button < Input::PAD_BUTTONS)
        add(action, Source::PadButton, button, 0);
}

void InputActionMap::BindPadAxis(const char *action, int axis, int direction)
{
    if (axis < Input::PAD_AXES && direction != 0)
        add(action, Source::PadAxis, axis, direction > 0 ? 1 : -1);
}

void InputActionMap::BindDefaults()
{
    Clear();
    Bind("move_forward", (int)Key::W);
    Bind("move_forward", (int)Key::Up);
    BindPadButton("move_forward", (int)PadButton::DpadUp);
    BindPadAxis("move_forward", (int)PadAxis::LeftY, -1);
    Bind("move_backward", (int)Key::S);
    Bind("move_backward", (int)Key::Down);
    BindPadButton("move_backward", (int)PadButton::DpadDown);
    BindPadAxis("move_backward", (int)PadAxis::LeftY, 1);
    Bind("turn_left", (int)Key::A);
    Bind("turn_left", (int)Key::Left);
    BindPadButton("turn_left", (int)PadButton::DpadLeft);
    BindPadAxis("turn_left", (int)PadAxis::LeftX, -1);
    Bind("turn_right", (int)Key::D);
    Bind("turn_right", (int)Key::Right);
    BindPadButton("turn_right", (int)PadButton::DpadRight);
    BindPadAxis("turn_right", (int)PadAxis::LeftX, 1);
    Bind("primary", (int)Key::Space);
    BindPadButton("primary", (int)PadButton::A);
    Bind("secondary", (int)Key::LeftCtrl);
    BindPadButton("secondary", (int)PadButton::B);
}

float InputActionMap::value(const Input &input, const Binding &binding, bool previous) const
{
    switch (binding.source)
    {
    case Source::Key:
        if (previous)
            return input.KeyDown(binding.scancode) && !input.KeyPressed(binding.scancode)
                           ? 1.0f
                           : (input.KeyReleased(binding.scancode) ? 1.0f : 0.0f);
        return input.KeyDown(binding.scancode) ? 1.0f : 0.0f;
    case Source::PadButton:
        if (previous)
            return input.PadDown(binding.scancode) && !input.PadPressed(binding.scancode)
                       ? 1.0f
                       : (input.PadReleased(binding.scancode) ? 1.0f : 0.0f);
        return input.PadDown(binding.scancode) ? 1.0f : 0.0f;
    case Source::PadAxis:
    {
        const float v = (previous ? input.PadAxisPrevious(binding.scancode) : input.PadAxisValue(binding.scancode)) *
                        static_cast<float>(binding.direction);
        return v > 0.0f ? v : 0.0f;
    }
    }
    return 0.0f;
}

void InputActionMap::Unbind(const char *action, int scancode)
{
    for (auto it = mBindings.begin(); it != mBindings.end(); ++it)
    {
        if (matches(it->action, action) && it->source == Source::Key && it->scancode == scancode)
        {
            mBindings.erase(it);
            return;
        }
    }
}

void InputActionMap::Clear(const char *action)
{
    if (!action)
        return;
    for (auto it = mBindings.begin(); it != mBindings.end();)
    {
        if (matches(it->action, action))
            it = mBindings.erase(it);
        else
            ++it;
    }
}

void InputActionMap::Clear()
{
    mBindings.clear();
}

// An action is held while any of its bindings is; pressed the frame the first
// one comes on, released the frame the last one goes off.
bool InputActionMap::Down(const Input &input, const char *action) const
{
    for (const Binding &binding : mBindings)
        if (matches(binding.action, action) && value(input, binding, false) >= kAxisPressed)
            return true;
    return false;
}

bool InputActionMap::Pressed(const Input &input, const char *action) const
{
    bool now = false, before = false;
    for (const Binding &binding : mBindings)
        if (matches(binding.action, action))
        {
            now = now || value(input, binding, false) >= kAxisPressed;
            before = before || value(input, binding, true) >= kAxisPressed;
        }
    return now && !before;
}

bool InputActionMap::Released(const Input &input, const char *action) const
{
    bool now = false, before = false;
    for (const Binding &binding : mBindings)
        if (matches(binding.action, action))
        {
            now = now || value(input, binding, false) >= kAxisPressed;
            before = before || value(input, binding, true) >= kAxisPressed;
        }
    return before && !now;
}

float InputActionMap::Strength(const Input &input, const char *action) const
{
    float strength = 0.0f;
    for (const Binding &binding : mBindings)
        if (matches(binding.action, action))
        {
            const float v = value(input, binding, false);
            strength = v > strength ? v : strength;
        }
    return strength > 1.0f ? 1.0f : strength;
}

InputActionMap &GetInputActions()
{
    static InputActionMap actions;
    return actions;
}
}
