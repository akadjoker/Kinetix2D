/* =========================================================
** zenconf.h — Platform-configurable macros for Zen
**
** Override these at compile time with -D to redirect I/O:
**   -Dzen_write=my_write_func
**   -Dzen_writeln=my_writeln_func
** ========================================================= */

#ifndef ZEN_CONF_H
#define ZEN_CONF_H

#include <cstdio>
#include <cstddef>

/* --- Output: write raw bytes --- */
#if !defined(zen_write)
  #if defined(__ANDROID__)
    #include <android/log.h>
    static inline void zen_write(const char *s, size_t l) {
        char buf[4096];
        size_t n = l < sizeof(buf) - 1 ? l : sizeof(buf) - 1;
        for (size_t i = 0; i < n; i++) buf[i] = s[i];
        buf[n] = '\0';
        __android_log_write(ANDROID_LOG_INFO, "zen", buf);
    }
  #elif defined(__EMSCRIPTEN__)
    #include <emscripten.h>
    static inline void zen_write(const char *s, size_t l) {
        /* emscripten printf goes to console.log — use it directly */
        fwrite((s), 1, (l), stdout);
    }
  #else
    #define zen_write(s, l)  fwrite((s), 1, (l), stdout)
  #endif
#endif

/* --- Output: write null-terminated string --- */
#if !defined(zen_writes)
  #if defined(__ANDROID__)
    #define zen_writes(s)  __android_log_write(ANDROID_LOG_INFO, "zen", (s))
  #elif defined(__EMSCRIPTEN__)
    #define zen_writes(s)  fputs((s), stdout)
  #else
    #define zen_writes(s)  fputs((s), stdout)
  #endif
#endif

/* --- Output: newline + flush --- */
#if !defined(zen_writeln)
  #if defined(__ANDROID__)
    #define zen_writeln()  ((void)0)
  #elif defined(__EMSCRIPTEN__)
    #define zen_writeln()  (fputc('\n', stdout))
  #else
    #define zen_writeln()  (fputc('\n', stdout), fflush(stdout))
  #endif
#endif

/* --- Error output (console.error on web, stderr on desktop) --- */
#if !defined(zen_writeerr)
  #if defined(__ANDROID__)
    static inline void zen_writeerr(const char *s, size_t l) {
        char buf[4096];
        size_t n = l < sizeof(buf) - 1 ? l : sizeof(buf) - 1;
        for (size_t i = 0; i < n; i++) buf[i] = s[i];
        buf[n] = '\0';
        __android_log_write(ANDROID_LOG_ERROR, "zen", buf);
    }
  #elif defined(__EMSCRIPTEN__)
    #include <emscripten.h>
    static inline void zen_writeerr(const char *s, size_t l) {
        /* console.error — appears red in browser DevTools */
        char buf[4096];
        size_t n = l < sizeof(buf) - 1 ? l : sizeof(buf) - 1;
        for (size_t i = 0; i < n; i++) buf[i] = s[i];
        buf[n] = '\0';
        EM_ASM({ console.error(UTF8ToString($0)); }, buf);
    }
  #else
    #define zen_writeerr(s, l)  fwrite((s), 1, (l), stderr)
  #endif
#endif

#endif /* ZEN_CONF_H */
