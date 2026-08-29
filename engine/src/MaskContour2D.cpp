#include "k2d/MaskContour2D.h"

#include <cmath>
#include <cstddef>

namespace k2d
{

namespace
{

bool SampleSolid(const unsigned char *pixels, int width, int bpp, int x, int y, unsigned char threshold)
{
    const unsigned char *p = pixels + (std::size_t)(y * width + x) * (std::size_t)bpp;
    if (bpp == 4)
        return p[3] > threshold;
    if (bpp == 1)
        return p[0] > threshold;
    const int luminance = (p[0] * 77 + p[1] * 151 + p[2] * 28) >> 8;
    return (unsigned char)luminance > threshold;
}

double PerpendicularDistance(const Math::Vec2 &p, const Math::Vec2 &a, const Math::Vec2 &b)
{
    const double abx = (double)b.x - (double)a.x;
    const double aby = (double)b.y - (double)a.y;
    const double length = std::sqrt(abx * abx + aby * aby);
    if (length < 1e-9)
    {
        const double dx = (double)p.x - (double)a.x;
        const double dy = (double)p.y - (double)a.y;
        return std::sqrt(dx * dx + dy * dy);
    }
    const double apx = (double)p.x - (double)a.x;
    const double apy = (double)p.y - (double)a.y;
    const double cross = apx * aby - apy * abx;
    return std::fabs(cross) / length;
}

struct DPRange
{
    std::size_t start;
    std::size_t end;
};

void DouglasPeucker(const ct::Vector<Math::Vec2> &points, float epsilon, ct::Vector<unsigned char> &keep)
{
    const std::size_t n = points.size();
    keep.resize(n, 0);
    if (n == 0)
        return;
    keep[0] = 1;
    keep[n - 1] = 1;
    if (n < 3)
        return;

    ct::Vector<DPRange> stack;
    stack.push_back(DPRange{0, n - 1});

    while (!stack.empty())
    {
        const DPRange range = stack.back();
        stack.pop_back();
        if (range.end <= range.start + 1)
            continue;

        const Math::Vec2 &a = points[range.start];
        const Math::Vec2 &b = points[range.end];
        double maxDist = -1.0;
        std::size_t maxIndex = range.start;
        for (std::size_t i = range.start + 1; i < range.end; ++i)
        {
            const double d = PerpendicularDistance(points[i], a, b);
            if (d > maxDist)
            {
                maxDist = d;
                maxIndex = i;
            }
        }

        if (maxDist >= (double)epsilon)
        {
            keep[maxIndex] = 1;
            stack.push_back(DPRange{range.start, maxIndex});
            stack.push_back(DPRange{maxIndex, range.end});
        }
    }
}

void SimplifyClosedLoop(const ct::Vector<Math::Vec2> &loop, float epsilon, ct::Vector<Math::Vec2> &out)
{
    out.clear();
    const std::size_t n = loop.size();
    if (n < 3)
    {
        for (std::size_t i = 0; i < n; ++i)
            out.push_back(loop[i]);
        return;
    }

    ct::Vector<Math::Vec2> open;
    open.reserve(n + 1);
    for (std::size_t i = 0; i < n; ++i)
        open.push_back(loop[i]);
    open.push_back(loop[0]);

    ct::Vector<unsigned char> keep;
    DouglasPeucker(open, epsilon, keep);

    for (std::size_t i = 0; i < n; ++i)
        if (keep[i])
            out.push_back(loop[i]);

    // The walk keeps the arbitrary point it started at; fold it away if the closed loop shows it is collinear.
    if (out.size() > 3)
    {
        const double d = PerpendicularDistance(out[0], out.back(), out[1]);
        if (d < (double)epsilon)
            out.erase(out.begin());
    }
}

double ShoelaceArea(const ct::Vector<Math::Vec2> &points)
{
    const std::size_t n = points.size();
    if (n < 3)
        return 0.0;
    double area = 0.0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const std::size_t j = (i + 1) % n;
        area += (double)points[i].x * (double)points[j].y - (double)points[j].x * (double)points[i].y;
    }
    return area * 0.5;
}

}

int TraceMaskContours(const unsigned char *pixels, int width, int height, int bpp,
                      const MaskContourOptions &options, ct::Vector<ct::Vector<Math::Vec2>> &outLoops)
{
    outLoops.clear();

    if (!pixels || width <= 0 || height <= 0)
        return 0;
    if (bpp != 1 && bpp != 3 && bpp != 4)
        return 0;

    const unsigned char threshold = options.threshold;

    ct::Vector<unsigned char> solidBuffer((std::size_t)width * (std::size_t)height, 0);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            solidBuffer[(std::size_t)y * (std::size_t)width + (std::size_t)x] =
                SampleSolid(pixels, width, bpp, x, y, threshold) ? 1 : 0;

    auto solid = [&](int x, int y) -> bool
    {
        if (x < 0 || y < 0 || x >= width || y >= height)
            return true;
        return solidBuffer[(std::size_t)y * (std::size_t)width + (std::size_t)x] != 0;
    };

    // vertEdge(xLattice, y) is the vertical lattice segment (xLattice,y)-(xLattice,y+1).
    // horizEdge(x, yLattice) is the horizontal lattice segment (x,yLattice)-(x+1,yLattice).
    const int vertStride = width + 1;
    const int horizStride = width;
    ct::Vector<unsigned char> vertEdge((std::size_t)vertStride * (std::size_t)height, 0);
    ct::Vector<unsigned char> horizEdge((std::size_t)horizStride * (std::size_t)(height + 1), 0);

    auto vertRef = [&](int xLattice, int y) -> unsigned char & { return vertEdge[(std::size_t)y * (std::size_t)vertStride + (std::size_t)xLattice]; };
    auto horizRef = [&](int x, int yLattice) -> unsigned char & { return horizEdge[(std::size_t)yLattice * (std::size_t)horizStride + (std::size_t)x]; };

    for (int y = 0; y < height; ++y)
        for (int x = -1; x < width; ++x)
            if (solid(x, y) != solid(x + 1, y))
                vertRef(x + 1, y) = 1;

    for (int x = 0; x < width; ++x)
        for (int y = -1; y < height; ++y)
            if (solid(x, y) != solid(x, y + 1))
                horizRef(x, y + 1) = 1;

    auto popNeighbor = [&](int x, int y, int &nx, int &ny) -> bool
    {
        if (x < width && horizRef(x, y))
        {
            horizRef(x, y) = 0;
            nx = x + 1;
            ny = y;
            return true;
        }
        if (y < height && vertRef(x, y))
        {
            vertRef(x, y) = 0;
            nx = x;
            ny = y + 1;
            return true;
        }
        if (x > 0 && horizRef(x - 1, y))
        {
            horizRef(x - 1, y) = 0;
            nx = x - 1;
            ny = y;
            return true;
        }
        if (y > 0 && vertRef(x, y - 1))
        {
            vertRef(x, y - 1) = 0;
            nx = x;
            ny = y - 1;
            return true;
        }
        return false;
    };

    const float halfWidth = (float)width * options.scale * 0.5f;
    const float halfHeight = (float)height * options.scale * 0.5f;

    for (int y = 0; y <= height; ++y)
    {
        for (int x = 0; x <= width; ++x)
        {
            int nx, ny;
            while (popNeighbor(x, y, nx, ny))
            {
                ct::Vector<Math::Vec2> loop;
                loop.push_back(Math::Vec2((float)x, (float)y));
                int cx = nx;
                int cy = ny;
                while (cx != x || cy != y)
                {
                    loop.push_back(Math::Vec2((float)cx, (float)cy));
                    int tx, ty;
                    if (!popNeighbor(cx, cy, tx, ty))
                        break;
                    cx = tx;
                    cy = ty;
                }

                ct::Vector<Math::Vec2> simplified;
                SimplifyClosedLoop(loop, options.simplifyTolerance, simplified);

                if (simplified.size() < 3)
                    continue;
                if (std::fabs(ShoelaceArea(simplified)) < (double)options.minArea)
                    continue;

                ct::Vector<Math::Vec2> world;
                world.reserve(simplified.size());
                for (std::size_t i = 0; i < simplified.size(); ++i)
                    world.push_back(Math::Vec2(simplified[i].x * options.scale - halfWidth,
                                               simplified[i].y * options.scale - halfHeight));
                outLoops.push_back(world);
            }
        }
    }

    return (int)outLoops.size();
}

}
