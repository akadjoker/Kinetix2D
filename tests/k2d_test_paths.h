#pragma once

#include <ct/string.hpp>
#include <platform.h>

#if defined(_WIN32)
#include <sys/utime.h>
#else
#include <utime.h>
#endif

namespace k2d_tests
{

inline ct::String tempPath(const char *fileName)
{
    char directory[4096];
    if (!fs_get_temp_path(directory, sizeof directory))
        return ct::String(fileName);
    char path[4096];
    return path_join(path, sizeof path, directory, fileName) ? ct::String(path) : ct::String(fileName);
}

inline ct::String directoryOf(const char *path)
{
    char directory[4096];
    path_directory(path, directory, sizeof directory);
    return ct::String(directory);
}

inline void advanceModificationTime(const char *path, int seconds)
{
    const long long now = file_mod_time(path);
    utimbuf times;
    times.actime = times.modtime = (now < 0 ? 0 : now) + seconds;
    utime(path, &times);
}

} // namespace k2d_tests
