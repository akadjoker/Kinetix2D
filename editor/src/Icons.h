#pragma once

#include "Retained.hpp"

namespace k2d::editor
{

class Icons
{
public:
    static bool load();
    static void unload();
    static ig::retained::IconId get(const char* name);
};

} // namespace k2d::editor
