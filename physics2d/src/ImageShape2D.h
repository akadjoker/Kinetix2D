#pragma once

#include <mathc.h>

#include <ct/vector.hpp>

namespace k2d
{

struct Polyline
{
    ct::Vector<Math::Vec2> points;
};

int TraceImageOutlines(const unsigned char *pixels, int width, int height, int bpp,
                       unsigned char threshold, ct::Vector<Polyline> &outLines);

void SimplifyOutline(const Polyline &line, float toleranceRadians, Polyline &outSimplified);

double OutlineArea2(const Polyline &line);

}
