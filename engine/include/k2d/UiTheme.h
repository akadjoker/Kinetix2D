#pragma once

#include <mathc.h>

namespace k2d
{
    class Assets;
    class Texture;

    // A reference to a rectangle inside a UI atlas. Controls will serialize
    // their chosen theme/region, not duplicate pixels or create one texture
    // per button state.
    struct TextureRegion
    {
        Texture *texture = nullptr;
        Math::Vec4 rect = Math::Vec4(0.0f);

        bool valid() const { return texture != nullptr && rect.z > 0.0f && rect.w > 0.0f; }
    };

    // The engine fallback theme. Its `menu.png` atlas is embedded in k2d so
    // a project may freely replace its UI assets without making the default
    // controls depend on files beside the executable.
    class UiTheme
    {
    public:
        static Texture *DefaultTexture(Assets &assets);
        static TextureRegion DefaultAtlas(Assets &assets);
    };
}
