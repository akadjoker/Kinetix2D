#include "zen/zen_host_output.h"

#include <cstdio>
#include <cstring>

namespace
{
    ZenHostWriteFn gWriter = nullptr;
    void *gWriterUser = nullptr;
}

extern "C" void zen_host_set_writer(ZenHostWriteFn fn, void *user)
{
    gWriter = fn;
    gWriterUser = user;
}

extern "C" void zen_host_write(const char *text, size_t length)
{
    if (gWriter)
        gWriter(text, length, 0, gWriterUser);
    else
        std::fwrite(text, 1, length, stdout);
}

extern "C" void zen_host_writes(const char *text)
{
    zen_host_write(text, std::strlen(text));
}

extern "C" void zen_host_writeln(void)
{
    zen_host_write("\n", 1);
    if (!gWriter)
        std::fflush(stdout);
}

extern "C" void zen_host_writeerr(const char *text, size_t length)
{
    if (gWriter)
        gWriter(text, length, 1, gWriterUser);
    else
        std::fwrite(text, 1, length, stderr);
}
