#pragma once

#include <mathc.h>

#include <ct/vector.hpp>

namespace k2d
{

struct MaskContourOptions
{
    unsigned char threshold = 127;
    float simplifyTolerance = 1.0f;
    float scale = 1.0f;
    float minArea = 16.0f;
};

int TraceMaskContours(const unsigned char *pixels, int width, int height, int bpp,
                      const MaskContourOptions &options, ct::Vector<ct::Vector<Math::Vec2>> &outLoops);

}
