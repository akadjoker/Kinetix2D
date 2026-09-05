#pragma once

#include <cmath>

namespace k2d
{

enum class ViewportScaleMode
{
    Fit,
    Expand
};

inline const char* ViewportScaleModeName(ViewportScaleMode mode)
{
    return mode == ViewportScaleMode::Expand ? "expand" : "fit";
}

inline ViewportScaleMode ParseViewportScaleMode(const char* name)
{
    return name && name[0] == 'e' ? ViewportScaleMode::Expand : ViewportScaleMode::Fit;
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
