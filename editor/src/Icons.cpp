#include "Icons.h"

#include "ToolBar.hpp"
#include "WidgetApp.hpp"

#include <k2d/Image.h>

#include <ct/hashmap.hpp>
#include <ct/string.hpp>

#include <platform.h>

#include <cstdlib>

#include "k2d_editor_icons_png.h"
#include "k2d_editor_icons_txt.h"

namespace rt = ig::retained;

namespace k2d::editor
{

namespace
{
struct State
{
    ct::HashMap<ct::String, rt::IconId> icons;
    rt::TextureHandle texture;
    bool loaded = false;
};

State& state()
{
    static State instance;
    return instance;
}

bool nextToken(const char*& cursor, const char* end, char* out, size_t capacity)
{
    while (cursor < end && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r'))
        ++cursor;
    if (cursor >= end || *cursor == '\n')
        return false;
    size_t length = 0;
    while (cursor < end && *cursor != ' ' && *cursor != '\t' && *cursor != '\r' && *cursor != '\n')
    {
        if (length + 1 >= capacity)
            return false;
        out[length++] = *cursor++;
    }
    out[length] = '\0';
    return length > 0;
}
} // namespace

bool Icons::load()
{
    State& current = state();
    if (current.loaded)
        return true;
    int width = 0;
    int height = 0;
    unsigned char* pixels = DecodeImageRgba(kEditorIconsPng, kEditorIconsPngSize, width, height);
    if (!pixels)
    {
        log_error("editor icons: cannot decode the atlas");
        return false;
    }
    current.texture = rt::WidgetApp::instance().uploadTexture(pixels, width, height);
    FreeImage(pixels);
    if (!current.texture)
        return false;

    const char* cursor = reinterpret_cast<const char*>(kEditorIconsTxt);
    const char* end = cursor + kEditorIconsTxtSize;
    while (cursor < end)
    {
        char name[96];
        char numbers[4][24];
        bool good = nextToken(cursor, end, name, sizeof name);
        for (int i = 0; i < 4 && good; ++i)
            good = nextToken(cursor, end, numbers[i], sizeof numbers[i]);
        while (cursor < end && *cursor != '\n')
            ++cursor;
        if (cursor < end)
            ++cursor;
        if (!good)
            continue;
        const float x = static_cast<float>(std::atoi(numbers[0]));
        const float y = static_cast<float>(std::atoi(numbers[1]));
        const float w = static_cast<float>(std::atoi(numbers[2]));
        const float h = static_cast<float>(std::atoi(numbers[3]));
        const rt::Rect uv{x / width, y / height, w / width, h / height};
        current.icons[ct::String(name)] = rt::registerImageIcon(current.texture, uv);
    }
    current.loaded = true;
    return true;
}

void Icons::unload()
{
    State& current = state();
    if (current.texture)
        rt::WidgetApp::instance().destroyTexture(current.texture);
    current.texture = rt::TextureHandle();
    current.icons.clear();
    current.loaded = false;
}

rt::IconId Icons::get(const char* name)
{
    const rt::IconId* icon = state().icons.find(ct::String(name));
    return icon ? *icon : rt::IconId::None;
}

rt::ToolBar* iconButton(rt::Widget* parent, rt::IconId icon, const char* tooltip, ct::Function<void()> clicked)
{
    auto* bar = parent->createChild<rt::ToolBar>();
    bar->setDraggable(false);
    bar->addButton(icon, tooltip);
    bar->buttonClicked.connect([clicked](int) {
        if (clicked)
            clicked();
    });
    return bar;
}

} // namespace k2d::editor
