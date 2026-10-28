#pragma once

#include <cmath>
#include <cstring>

namespace k2d
{

// How the camera's resolution meets the window. Fit keeps it whole with bars;
// Expand shows more of the world on the longer side; Crop fills the window and
// cuts the overflow; Stretch fills it out of proportion; FitWidth and
// FitHeight keep one side and show more or less on the other; Native is one
// unit to a pixel, the resolution being the window's own.
enum class ViewportScaleMode
{
    Fit,
    Expand,
    Crop,
    Stretch,
    FitWidth,
    FitHeight,
    Native
};

inline const char* ViewportScaleModeName(ViewportScaleMode mode)
{
    static const char* const names[] = {"fit", "expand", "crop", "stretch", "fit_width", "fit_height", "native"};
    const int index = static_cast<int>(mode);
    return index >= 0 && index < 7 ? names[index] : "fit";
}

inline ViewportScaleMode ParseViewportScaleMode(const char* name)
{
    for (int i = 0; i < 7; ++i)
        if (name && std::strcmp(name, ViewportScaleModeName(static_cast<ViewportScaleMode>(i))) == 0)
            return static_cast<ViewportScaleMode>(i);
    return ViewportScaleMode::Fit;
}

// Maps a stable game-space resolution onto a real window or editor panel.
// x/y/width/height are output pixels; virtualWidth/virtualHeight are the
// coordinates used by the camera, UI and scripts.
struct GameViewport
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float virtualWidth = 0.0f;
    float virtualHeight = 0.0f;

    bool valid() const
    {
        return width > 0.0f && height > 0.0f && virtualWidth > 0.0f && virtualHeight > 0.0f;
    }

    float toVirtualX(float outputX) const
    {
        return valid() ? (outputX - x) * virtualWidth / width : outputX;
    }

    float toVirtualY(float outputY) const
    {
        return valid() ? (outputY - y) * virtualHeight / height : outputY;
    }
};

inline GameViewport CalculateGameViewport(float outputWidth, float outputHeight,
                                          float requestedWidth, float requestedHeight,
                                          ViewportScaleMode mode = ViewportScaleMode::Fit,
                                          bool integerScale = false)
{
    GameViewport result;
    if (outputWidth <= 0.0f || outputHeight <= 0.0f)
        return result;

    const float virtualWidth = requestedWidth > 0.0f ? requestedWidth : outputWidth;
    const float virtualHeight = requestedHeight > 0.0f ? requestedHeight : outputHeight;
    result.virtualWidth = virtualWidth;
    result.virtualHeight = virtualHeight;

    result.width = outputWidth;
    result.height = outputHeight;
    switch (mode)
    {
    case ViewportScaleMode::Stretch:
        return result;
    case ViewportScaleMode::Native:
        result.virtualWidth = outputWidth;
        result.virtualHeight = outputHeight;
        return result;
    case ViewportScaleMode::FitWidth:
        result.virtualHeight = outputHeight * virtualWidth / outputWidth;
        return result;
    case ViewportScaleMode::FitHeight:
        result.virtualWidth = outputWidth * virtualHeight / outputHeight;
        return result;
    case ViewportScaleMode::Crop:
    {
        const float scale = std::fmax(outputWidth / virtualWidth, outputHeight / virtualHeight);
        result.width = virtualWidth * scale;
        result.height = virtualHeight * scale;
        result.x = (outputWidth - result.width) * 0.5f;
        result.y = (outputHeight - result.height) * 0.5f;
        return result;
    }
    default:
        break;
    }

    if (mode == ViewportScaleMode::Expand)
    {
        const float outputAspect = outputWidth / outputHeight;
        const float virtualAspect = virtualWidth / virtualHeight;
        if (outputAspect > virtualAspect)
            result.virtualWidth = virtualHeight * outputAspect;
        else
            result.virtualHeight = virtualWidth / outputAspect;
        result.width = outputWidth;
        result.height = outputHeight;
        return result;
    }

    float scale = outputWidth / virtualWidth;
    const float heightScale = outputHeight / virtualHeight;
    if (heightScale < scale)
        scale = heightScale;
    if (integerScale && scale >= 1.0f)
        scale = std::floor(scale);

    result.width = virtualWidth * scale;
    result.height = virtualHeight * scale;
    result.x = (outputWidth - result.width) * 0.5f;
    result.y = (outputHeight - result.height) * 0.5f;
    return result;
}

} // namespace k2d
