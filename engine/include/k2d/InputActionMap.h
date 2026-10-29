#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d
{
    class Input;

    // Maps named gameplay actions to keys, gamepad buttons and the directions
    // of gamepad axes, on any pad. Virtual-pad keys use the same Input state and
    // therefore need no special bindings here.
    class InputActionMap
    {
    public:
        void Bind(const char *action, int scancode);
        void BindPadButton(const char *action, int button);
        // direction +1 or -1: the axis pushed that way past half its travel.
        void BindPadAxis(const char *action, int axis, int direction);
        // The keyboard and gamepad defaults the runner and the editor's Play
        // share: move_forward, move_backward, turn_left, turn_right, primary
        // and secondary.
        void BindDefaults();
        void Unbind(const char *action, int scancode);
        void Clear(const char *action);
        void Clear();

        bool Down(const Input &input, const char *action) const;
        bool Pressed(const Input &input, const char *action) const;
        bool Released(const Input &input, const char *action) const;
        // 0..1: 1 for a key or a button held, how far an axis is pushed its way.
        float Strength(const Input &input, const char *action) const;

        void bind(const char *action, int scancode) { Bind(action, scancode); }
        void unbind(const char *action, int scancode) { Unbind(action, scancode); }
        void clear(const char *action) { Clear(action); }
        void clear() { Clear(); }
        bool down(const Input &input, const char *action) const { return Down(input, action); }
        bool pressed(const Input &input, const char *action) const { return Pressed(input, action); }
        bool released(const Input &input, const char *action) const { return Released(input, action); }

    private:
        enum class Source : unsigned char
        {
            Key,
            PadButton,
            PadAxis
        };
        struct Binding
        {
            ct::String action;
            int scancode = -1;
            Source source = Source::Key;
            int direction = 0;
        };
        void add(const char *action, Source source, int code, int direction);
        float value(const Input &input, const Binding &binding, bool previous) const;
        ct::Vector<Binding> mBindings;
    };

    InputActionMap &GetInputActions();
}
