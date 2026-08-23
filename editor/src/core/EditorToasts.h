#pragma once

#include <ct/string.hpp>
#include <ct/vector.hpp>

namespace k2d::editor
{

enum class ToastKind
{
    Info,
    Success,
    Warning,
    Error
};

class EditorToasts
{
public:
    void push(ToastKind kind, const ct::String &message);
    void info(const ct::String &message);
    void success(const ct::String &message);
    void warning(const ct::String &message);
    void error(const ct::String &message);

    void update(float deltaTime);
    void draw();
    void clear();

private:
    struct Toast
    {
        ToastKind kind = ToastKind::Info;
        ct::String message;
        float remaining = 0.0f;
        float total = 0.0f;
    };

    ct::Vector<Toast> mToasts;
};

}
