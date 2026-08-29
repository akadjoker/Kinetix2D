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
    // A hardness map is land all the way round, so treating the area outside
    // the image as solid is what closes the walkable region at the border.
    // A sprite with a transparent margin needs the opposite, or it traces a
    // spurious rectangle around the whole image.
    bool outsideIsSolid = true;
};

int TraceMaskContours(const unsigned char *pixels, int width, int height, int bpp,
                      const MaskContourOptions &options, ct::Vector<ct::Vector<Math::Vec2>> &outLoops);

}
