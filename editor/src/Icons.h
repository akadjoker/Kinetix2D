#pragma once

#include "Retained.hpp"

#include <ct/function.hpp>

namespace ig::retained
{
class ToolBar;
class Widget;
}

namespace k2d::editor
{

class Icons
{
public:
    static bool load();
    static void unload();
    static ig::retained::IconId get(const char* name);
};

// A single icon button, drawn as the toolbars draw theirs; the text goes in
// the tooltip. The editor has no text buttons.
ig::retained::ToolBar* iconButton(ig::retained::Widget* parent, ig::retained::IconId icon, const char* tooltip,
                                  ct::Function<void()> clicked);
inline ig::retained::ToolBar* iconButton(ig::retained::Widget* parent, const char* icon, const char* tooltip,
                                         ct::Function<void()> clicked)
{
    return iconButton(parent, Icons::get(icon), tooltip, static_cast<ct::Function<void()>&&>(clicked));
}

} // namespace k2d::editor
