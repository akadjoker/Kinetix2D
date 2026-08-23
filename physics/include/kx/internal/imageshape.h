#pragma once

#include <glm/glm.hpp>
#include <cstdint>

#include <ct/vector.hpp>

namespace kx
{

    struct Polyline
    {
        ct::Vector<glm::vec2> points;
    };

    int TraceImageOutlines(const unsigned char *pixels, int width, int height, int bpp,
                           unsigned char threshold, ct::Vector<Polyline> &outLines);

    void SimplifyOutline(const Polyline &line, float toleranceRadians, Polyline &outSimplified);

} 