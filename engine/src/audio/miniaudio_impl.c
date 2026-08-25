/* miniaudio + stb_vorbis implementation, compiled as C.
 *
 * Kept separate from bugl_audio.cpp (C++) on purpose: building the miniaudio
 * implementation as C gives all its functions C linkage. This is required on
 * web — emscripten's Web Audio bridge resolves EMSCRIPTEN_KEEPALIVE helpers by
 * their C name (e.g. _ma_device__on_notification_unlocked). A few of these are
 * not wrapped in extern "C" upstream, so compiling as C++ would mangle them and
 * break the audio backend at runtime. The C++ side (bugl_audio.cpp) includes
 * miniaudio.h for its (extern "C") declarations only.
 *
 * NOTE: do not add -std=c* flags to this file for the Emscripten build —
 * miniaudio explicitly does not support them.
 */
#include "stb_vorbis.c"
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
