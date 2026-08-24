#ifndef ZEN_BYTECODE_H
#define ZEN_BYTECODE_H

#include "common.h"
#include <cstddef>
#include <cstdint>

namespace zen
{

    static constexpr uint8_t ZEN_BYTECODE_MAGIC[5] = {'Z', 'E', 'N', 'B', 'C'};
    static constexpr uint16_t ZEN_BYTECODE_VERSION_MAJOR = 2;
    /* minor 2: class field defaults (a class body's "name = literal"). */
    static constexpr uint16_t ZEN_BYTECODE_VERSION_MINOR = 2;

    struct BytecodeStats
    {
        uint32_t functions = 0;
        uint32_t classes = 0;
        uint32_t closures = 0;
        uint32_t strings = 0;
        uint32_t constants = 0;
        uint32_t instructions = 0;
        uint32_t globals = 0;
        uint32_t selectors = 0;
        size_t bytes = 0;
    };

    bool is_bytecode_buffer(const uint8_t *data, size_t size);

    bool dump_bytecode_file(ObjFunc *func, const char *path, bool strip_debug = false,
                            char *err = nullptr, int err_len = 0);

    bool dump_bytecode_file(VM *vm, ObjFunc *func, const char *path, bool strip_debug = false,
                            char *err = nullptr, int err_len = 0);

    bool dump_bytecode_file(VM *vm, ObjFunc *func, const char *path, bool strip_debug,
                            BytecodeStats *stats, char *err = nullptr, int err_len = 0);

    ObjFunc *load_bytecode_buffer(VM *vm, const uint8_t *data, size_t size,
                                  char *err = nullptr, int err_len = 0);

} /* namespace zen */

#endif /* ZEN_BYTECODE_H */
