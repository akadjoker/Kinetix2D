#pragma once

#include <cstdint>
#include <ctime>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace zen
{

inline double platform_wall_seconds()
{
#if defined(_WIN32)
    FILETIME file_time;
    GetSystemTimeAsFileTime(&file_time);
    ULARGE_INTEGER ticks;
    ticks.LowPart = file_time.dwLowDateTime;
    ticks.HighPart = file_time.dwHighDateTime;
    return static_cast<double>(ticks.QuadPart - 116444736000000000ULL) * 1e-7;
#else
    timespec time_spec;
    clock_gettime(CLOCK_REALTIME, &time_spec);
    return static_cast<double>(time_spec.tv_sec) + static_cast<double>(time_spec.tv_nsec) * 1e-9;
#endif
}

inline double platform_monotonic_seconds()
{
#if defined(_WIN32)
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return static_cast<double>(counter.QuadPart) / static_cast<double>(frequency.QuadPart);
#else
    timespec time_spec;
    clock_gettime(CLOCK_MONOTONIC, &time_spec);
    return static_cast<double>(time_spec.tv_sec) + static_cast<double>(time_spec.tv_nsec) * 1e-9;
#endif
}

inline void platform_sleep_seconds(double seconds)
{
    if (seconds <= 0.0)
    {
        return;
    }

#if defined(_WIN32)
    const double milliseconds = seconds * 1000.0;
    const DWORD duration =
        milliseconds >= static_cast<double>(MAXDWORD) ? MAXDWORD : static_cast<DWORD>(milliseconds + 0.5);
    Sleep(duration);
#else
    timespec time_spec;
    time_spec.tv_sec = static_cast<time_t>(seconds);
    time_spec.tv_nsec = static_cast<long>((seconds - static_cast<double>(time_spec.tv_sec)) * 1e9);
    nanosleep(&time_spec, nullptr);
#endif
}

} // namespace zen
