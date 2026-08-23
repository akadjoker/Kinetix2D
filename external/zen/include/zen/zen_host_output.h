#ifndef ZEN_HOST_OUTPUT_H
#define ZEN_HOST_OUTPUT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*ZenHostWriteFn)(const char *text, size_t length, int isError, void *user);

    void zen_host_set_writer(ZenHostWriteFn fn, void *user);
    void zen_host_write(const char *text, size_t length);
    void zen_host_writes(const char *text);
    void zen_host_writeln(void);
    void zen_host_writeerr(const char *text, size_t length);

#ifdef __cplusplus
}
#endif

#define zen_write(s, l) zen_host_write((s), (l))
#define zen_writes(s) zen_host_writes(s)
#define zen_writeln() zen_host_writeln()
#define zen_writeerr(s, l) zen_host_writeerr((s), (l))

#endif
