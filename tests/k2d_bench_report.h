#pragma once

#include <platform.h>

#include <cstdarg>
#include <cstdio>

namespace k2d_tests
{

// One line of a benchmark's results: stdout on the desktop, the log (logcat) on
// Android, where a NativeActivity has no terminal.
inline void report(const char *format, ...)
{
    char line[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof line, format, args);
    va_end(args);
#if defined(__ANDROID__)
    log_info("%s", line);
#else
    std::fputs(line, stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
#endif
}

} // namespace k2d_tests
