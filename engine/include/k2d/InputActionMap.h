#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{
    class Input;

    // Maps named gameplay actions to physical or synthesized keyboard keys.
    // Virtual-pad keys use the same Input state and therefore need no special
    // bindings here.
    class InputActionMap
    {
    public:
        void Bind(const char *action, int scancode);
        void Unbind(const char *action, int scancode);
        void Clear(const char *action);
        void Clear();

        bool Down(const Input &input, const char *action) const;
        bool Pressed(const Input &input, const char *action) const;
        bool Released(const Input &input, const char *action) const;

        void bind(const char *action, int scancode) { Bind(action, scancode); }
        void unbind(const char *action, int scancode) { Unbind(action, scancode); }
        void clear(const char *action) { Clear(action); }
        void clear() { Clear(); }
        bool down(const Input &input, const char *action) const { return Down(input, action); }
        bool pressed(const Input &input, const char *action) const { return Pressed(input, action); }
        bool released(const Input &input, const char *action) const { return Released(input, action); }

    private:
        struct Binding
        {
            ct::String action;
            int scancode = -1;
        };
        ct::Vector<Binding> mBindings;
    };

    InputActionMap &GetInputActions();
}
