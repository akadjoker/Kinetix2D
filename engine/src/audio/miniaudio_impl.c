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

/* stb_vorbis uses short, generic macro names for its internal channel map.
 * They have done their job once the implementation above has been included,
 * but PLAYBACK_LEFT and PLAYBACK_RIGHT also occur in Windows SDK headers that
 * miniaudio includes below. Do not let the decoder macros leak into those
 * platform headers.
 */
#undef PLAYBACK_MONO
#undef PLAYBACK_LEFT
#undef PLAYBACK_RIGHT
#undef L
#undef C
#undef R

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
