#pragma once

namespace k2d
{

    struct IVec2
    {
        int x, y;

        IVec2() : x(0), y(0) {}
        IVec2(int x_, int y_) : x(x_), y(y_) {}

        IVec2 operator+(const IVec2 &o) const { return IVec2(x + o.x, y + o.y); }
        IVec2 operator-(const IVec2 &o) const { return IVec2(x - o.x, y - o.y); }
        bool operator==(const IVec2 &o) const { return x == o.x && y == o.y; }
        bool operator!=(const IVec2 &o) const { return !(*this == o); }
    };

}
