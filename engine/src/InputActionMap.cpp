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

void InputActionMap::Bind(const char *action, int scancode)
{
    if (!action || !action[0] || scancode < 0 || scancode >= Input::MAX_KEYS)
        return;
    for (const Binding &binding : mBindings)
        if (matches(binding.action, action) && binding.scancode == scancode)
            return;
    mBindings.push_back({action, scancode});
}

void InputActionMap::Unbind(const char *action, int scancode)
{
    for (auto it = mBindings.begin(); it != mBindings.end(); ++it)
    {
        if (matches(it->action, action) && it->scancode == scancode)
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

bool InputActionMap::Down(const Input &input, const char *action) const
{
    for (const Binding &binding : mBindings)
        if (matches(binding.action, action) && input.KeyDown(binding.scancode))
            return true;
    return false;
}

bool InputActionMap::Pressed(const Input &input, const char *action) const
{
    for (const Binding &binding : mBindings)
        if (matches(binding.action, action) && input.KeyPressed(binding.scancode))
            return true;
    return false;
}

bool InputActionMap::Released(const Input &input, const char *action) const
{
    for (const Binding &binding : mBindings)
        if (matches(binding.action, action) && input.KeyReleased(binding.scancode))
            return true;
    return false;
}

InputActionMap &GetInputActions()
{
    static InputActionMap actions;
    return actions;
}
}
