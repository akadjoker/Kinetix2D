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
    // World-space radius of whatever has to walk here. The walkable area is
    // eroded by it before tracing, so a path can never hug a wall closer than
    // a body can stand, and a gap too narrow to fit through simply closes.
    float agentRadius = 0.0f;
};

int TraceMaskContours(const unsigned char *pixels, int width, int height, int bpp,
                      const MaskContourOptions &options, ct::Vector<ct::Vector<Math::Vec2>> &outLoops);

}
