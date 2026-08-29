#pragma once

namespace k2d
{

    template <typename T> constexpr const T &Min(const T &a, const T &b)
    {
        return b < a ? b : a;
    }

    template <typename T> constexpr const T &Max(const T &a, const T &b)
    {
        return a < b ? b : a;
    }

    template <typename T> constexpr const T &Clamp(const T &value, const T &low, const T &high)
    {
        return value < low ? low : (high < value ? high : value);
    }

}
