#include "k2d/UiTheme.h"

#include "k2d/Assets.h"
#include "k2d/Texture.h"

#include "k2d_default_ui_theme_png.h"

namespace k2d
{
    Texture *UiTheme::DefaultTexture(Assets &assets)
    {
        static const char *const name = "__k2d_default_ui_theme";
        if (Texture *texture = assets.GetTexture(name))
            return texture;
        return assets.LoadTextureMemory(name, kDefaultUiThemePng, kDefaultUiThemePngSize, true, false);
    }

    TextureRegion UiTheme::DefaultAtlas(Assets &assets)
    {
        TextureRegion region;
        region.texture = DefaultTexture(assets);
        if (region.texture)
            region.rect = Math::Vec4(0.0f, 0.0f, static_cast<float>(region.texture->Width()),
                                     static_cast<float>(region.texture->Height()));
        return region;
    }
}
