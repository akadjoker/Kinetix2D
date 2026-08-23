#include "kx/internal/imageshape.h"
#include "kx/world.h"

#include <cmath>

namespace kx
{

    namespace
    {

        float SampleDensity(const unsigned char *pixels, int width, int height, int bpp, int x, int y)
        {
            if (x < 0 || y < 0 || x >= width || y >= height)
                return 0.0f;
            if (bpp == 4)
                return (float)pixels[(y * width + x) * 4 + 3];
            return (float)pixels[y * width + x];
        }

        void CollectSegment(ct::Vector<Polyline> &lines, const glm::vec2 &v0, const glm::vec2 &v1)
        {
            int before = -1;
            int after = -1;
            for (size_t i = 0; i < lines.size(); ++i)
            {
                const ct::Vector<glm::vec2> &pts = lines[i].points;
                if (before < 0 && pts.back() == v0)
                    before = (int)i;
                if (after < 0 && pts[0] == v1)
                    after = (int)i;
            }

            if (before >= 0 && after >= 0)
            {
                if (before == after)
                {
                    lines[(size_t)before].points.push_back(v1);
                }
                else
                {
                    ct::Vector<glm::vec2> &lbefore = lines[(size_t)before].points;
                    const ct::Vector<glm::vec2> &lafter = lines[(size_t)after].points;
                    for (size_t i = 0; i < lafter.size(); ++i)
                        lbefore.push_back(lafter[i]);
                    lines[(size_t)after] = lines.back();
                    lines.pop_back();
                }
            }
            else if (before >= 0)
            {
                lines[(size_t)before].points.push_back(v1);
            }
            else if (after >= 0)
            {
                ct::Vector<glm::vec2> &pts = lines[(size_t)after].points;
                ct::Vector<glm::vec2> shifted;
                shifted.reserve(pts.size() + 1);
                shifted.push_back(v0);
                for (size_t i = 0; i < pts.size(); ++i)
                    shifted.push_back(pts[i]);
                pts = shifted;
            }
            else
            {
                Polyline line;
                line.points.push_back(v0);
                line.points.push_back(v1);
                lines.push_back(line);
            }
        }

        void Seg(ct::Vector<Polyline> &lines, const glm::vec2 &a, const glm::vec2 &b)
        {
            if (a != b)
                CollectSegment(lines, b, a);
        }

        void Segs(ct::Vector<Polyline> &lines, const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c)
        {
            Seg(lines, b, c);
            Seg(lines, a, b);
        }

        void MarchCellHard(ct::Vector<Polyline> &lines,
                           float a, float b, float c, float d, float t,
                           float x0, float x1, float y0, float y1)
        {
            float xm = 0.5f * (x0 + x1);
            float ym = 0.5f * (y0 + y1);

            int code = (a > t ? 1 : 0) | (b > t ? 2 : 0) | (c > t ? 4 : 0) | (d > t ? 8 : 0);

            switch (code)
            {
            case 0x1:
                Segs(lines, glm::vec2(x0, ym), glm::vec2(xm, ym), glm::vec2(xm, y0));
                break;
            case 0x2:
                Segs(lines, glm::vec2(xm, y0), glm::vec2(xm, ym), glm::vec2(x1, ym));
                break;
            case 0x3:
                Seg(lines, glm::vec2(x0, ym), glm::vec2(x1, ym));
                break;
            case 0x4:
                Segs(lines, glm::vec2(xm, y1), glm::vec2(xm, ym), glm::vec2(x0, ym));
                break;
            case 0x5:
                Seg(lines, glm::vec2(xm, y1), glm::vec2(xm, y0));
                break;
            case 0x6:
                Segs(lines, glm::vec2(xm, y0), glm::vec2(xm, ym), glm::vec2(x0, ym));
                Segs(lines, glm::vec2(xm, y1), glm::vec2(xm, ym), glm::vec2(x1, ym));
                break;
            case 0x7:
                Segs(lines, glm::vec2(xm, y1), glm::vec2(xm, ym), glm::vec2(x1, ym));
                break;
            case 0x8:
                Segs(lines, glm::vec2(x1, ym), glm::vec2(xm, ym), glm::vec2(xm, y1));
                break;
            case 0x9:
                Segs(lines, glm::vec2(x1, ym), glm::vec2(xm, ym), glm::vec2(xm, y0));
                Segs(lines, glm::vec2(x0, ym), glm::vec2(xm, ym), glm::vec2(xm, y1));
                break;
            case 0xA:
                Seg(lines, glm::vec2(xm, y0), glm::vec2(xm, y1));
                break;
            case 0xB:
                Segs(lines, glm::vec2(x0, ym), glm::vec2(xm, ym), glm::vec2(xm, y1));
                break;
            case 0xC:
                Seg(lines, glm::vec2(x1, ym), glm::vec2(x0, ym));
                break;
            case 0xD:
                Segs(lines, glm::vec2(x1, ym), glm::vec2(xm, ym), glm::vec2(xm, y0));
                break;
            case 0xE:
                Segs(lines, glm::vec2(xm, y0), glm::vec2(xm, ym), glm::vec2(x0, ym));
                break;
            default:
                break;
            }
        }

        void MarchImage(const unsigned char *pixels, int width, int height, int bpp,
                        unsigned char threshold, ct::Vector<Polyline> &lines)
        {
            float t = (float)threshold;

            ct::Vector<float> buffer;
            buffer.reserve((size_t)(width + 1));
            for (int i = 0; i <= width; ++i)
                buffer.push_back(SampleDensity(pixels, width, height, bpp, i, 0));

            for (int j = 0; j < height; ++j)
            {
                float y0 = (float)j;
                float y1 = (float)(j + 1);

                float bVal = buffer[0];
                float dVal = SampleDensity(pixels, width, height, bpp, 0, j + 1);
                buffer[0] = dVal;

                for (int i = 0; i < width; ++i)
                {
                    float x0 = (float)i;
                    float x1 = (float)(i + 1);

                    float aVal = bVal;
                    bVal = buffer[(size_t)(i + 1)];
                    float cVal = dVal;
                    dVal = SampleDensity(pixels, width, height, bpp, i + 1, j + 1);
                    buffer[(size_t)(i + 1)] = dVal;

                    MarchCellHard(lines, aVal, bVal, cVal, dVal, t, x0, x1, y0, y1);
                }
            }
        }

    } 

    int TraceImageOutlines(const unsigned char *pixels, int width, int height, int bpp,
                           unsigned char threshold, ct::Vector<Polyline> &outLines)
    {
        outLines.clear();

        if (!pixels || width <= 0 || height <= 0 || (bpp != 4 && bpp != 1))
            return 0;

        MarchImage(pixels, width, height, bpp, threshold, outLines);
        return (int)outLines.size();
    }

    namespace
    {

        float Sharpness(const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &c)
        {
            return Dot(Normalize(a - b), Normalize(c - b));
        }

        bool IsClosed(const Polyline &line)
        {
            return line.points.size() > 1 && line.points[0] == line.points.back();
        }

    } 

    void SimplifyOutline(const Polyline &line, float toleranceRadians, Polyline &outSimplified)
    {
        outSimplified.points.clear();

        const ct::Vector<glm::vec2> &src = line.points;
        if (src.size() < 3)
        {
            outSimplified.points = src;
            return;
        }

        ct::Vector<glm::vec2> reduced;
        reduced.push_back(src[0]);
        reduced.push_back(src[1]);

        float minSharp = -std::cos(toleranceRadians);

        for (size_t i = 2; i < src.size(); ++i)
        {
            const glm::vec2 &vert = src[i];
            float sharp = Sharpness(reduced[reduced.size() - 2], reduced.back(), vert);

            if (sharp <= minSharp)
                reduced[reduced.size() - 1] = vert;
            else
                reduced.push_back(vert);
        }

        if (IsClosed(line) &&
            Sharpness(reduced[reduced.size() - 2], reduced[0], reduced[1]) < minSharp)
        {
            reduced[0] = reduced[reduced.size() - 2];
            reduced.pop_back();
        }

        outSimplified.points = reduced;
    }

    namespace
    {

        double OutlineArea2(const Polyline &line)
        {
            const ct::Vector<glm::vec2> &pts = line.points;
            size_t n = pts.size();
            if (n < 3)
                return 0.0;

            double area = 0.0;
            for (size_t i = 0; i < n; ++i)
            {
                size_t j = (i + 1) % n;
                area += (double)pts[i].x * pts[j].y - (double)pts[j].x * pts[i].y;
            }
            return area < 0.0 ? -area : area;
        }

    } 

    int Body::AddFromImage(const unsigned char *pixels, int width, int height, int bpp,
                           unsigned char threshold, float density, float scale, float simplifyDegrees)
    {
        if (!pixels || width <= 0 || height <= 0 || (bpp != 4 && bpp != 1))
            return 0;

        ct::Vector<Polyline> rawLines;
        int lineCount = TraceImageOutlines(pixels, width, height, bpp, threshold, rawLines);
        if (lineCount == 0)
            return 0;

        float toleranceRadians = simplifyDegrees * (3.14159265359f / 180.0f);

        ct::Vector<Polyline> simplified;
        simplified.reserve(rawLines.size());
        for (size_t i = 0; i < rawLines.size(); ++i)
        {
            Polyline out;
            SimplifyOutline(rawLines[i], toleranceRadians, out);
            if (out.points.size() >= 3)
                simplified.push_back(out);
        }

        if (simplified.size() == 0)
            return 0;

        for (size_t i = 0; i < simplified.size(); ++i)
        {
            size_t best = i;
            double bestArea = OutlineArea2(simplified[i]);
            for (size_t j = i + 1; j < simplified.size(); ++j)
            {
                double area = OutlineArea2(simplified[j]);
                if (area > bestArea)
                {
                    bestArea = area;
                    best = j;
                }
            }
            if (best != i)
            {
                Polyline tmp = simplified[i];
                simplified[i] = simplified[best];
                simplified[best] = tmp;
            }
        }

        glm::vec2 center((float)width * 0.5f, (float)height * 0.5f);
        int added = 0;

        for (size_t i = 0; i < simplified.size() && ShapeCount() < kMaxShapes; ++i)
        {
            const ct::Vector<glm::vec2> &pts = simplified[i].points;
            int count = (int)pts.size();
            if (count < 3)
                continue;

            int loopCount = count;
            if (pts[0] == pts.back())
                --loopCount;
            if (loopCount < 3)
                continue;

            glm::vec2 local[512];
            if (loopCount > 512)
                loopCount = 512;
            for (int k = 0; k < loopCount; ++k)
                local[k] = (pts[(size_t)k] - center) * scale;

            added += AddMesh(local, loopCount, density);
        }

        return added;
    }

    Body *World::CreateFromImage(const glm::vec2 &pos, const unsigned char *pixels, int width, int height, int bpp,
                                 unsigned char threshold, float density, float scale, float simplifyDegrees)
    {
        Body *body = CreateBody(BodyType::Dynamic, pos);
        int added = body->AddFromImage(pixels, width, height, bpp, threshold, density, scale, simplifyDegrees);

        if (added == 0)
        {
            Destroy(body);
            return nullptr;
        }

        return body;
    }

} 