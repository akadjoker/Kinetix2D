/* =========================================================
** builtin_struct.cpp — struct module for Zen
**
** Python-compatible binary packing/unpacking over typed arrays.
**
** Usage:
**   import struct
**   data = struct.pack('iif', 42, 100, 3.14)   # → Uint8Array
**   vals = struct.unpack('iif', data)           # → list
**   n    = struct.calcsize('iif')               # → 12
**
** Format characters:
**   b/B = int8/uint8     h/H = int16/uint16
**   i/I = int32/uint32   q/Q = int64/uint64
**   f   = float32        d   = float64
**   x   = pad byte (pack: writes 0, unpack: skips)
**   < > = little/big endian (default: native/little)
** ========================================================= */

#include "module.h"
#include "vm.h"
#include "memory.h"
#include <cstring>
#include <cstdint>

namespace zen
{
    /* ---- endian helpers ---- */

    static inline bool is_big_endian()
    {
        uint16_t x = 1;
        return *((uint8_t *)&x) == 0;
    }

    static inline void swap2(uint8_t *p)
    {
        uint8_t t = p[0]; p[0] = p[1]; p[1] = t;
    }

    static inline void swap4(uint8_t *p)
    {
        uint8_t t;
        t = p[0]; p[0] = p[3]; p[3] = t;
        t = p[1]; p[1] = p[2]; p[2] = t;
    }

    static inline void swap8(uint8_t *p)
    {
        uint8_t t;
        t = p[0]; p[0] = p[7]; p[7] = t;
        t = p[1]; p[1] = p[6]; p[6] = t;
        t = p[2]; p[2] = p[5]; p[5] = t;
        t = p[3]; p[3] = p[4]; p[4] = t;
    }

    /* ---- format string parser ---- */

    struct FmtEntry
    {
        char code;
        int size;
    };

    static const int kMaxFmtEntries = 128;

    /* Parse format string into entries.  Returns count, or -1 on error.
       *need_swap is set if byte-swapping is required.  */
    static int parse_format(const char *fmt, FmtEntry *out, bool *need_swap)
    {
        bool native_big = is_big_endian();
        bool target_big = native_big; /* default = native */
        const char *p = fmt;

        /* Optional byte-order prefix */
        if (*p == '<') { target_big = false; ++p; }
        else if (*p == '>' || *p == '!') { target_big = true; ++p; }
        else if (*p == '=' || *p == '@') { ++p; } /* native */

        *need_swap = (target_big != native_big);

        int count = 0;
        while (*p)
        {
            /* optional repeat count */
            int repeat = 0;
            while (*p >= '0' && *p <= '9')
            {
                repeat = repeat * 10 + (*p - '0');
                ++p;
            }
            if (repeat == 0) repeat = 1;

            char c = *p++;
            if (!c) break;

            int elem_size;
            switch (c)
            {
            case 'b': case 'B': case 'x': elem_size = 1; break;
            case 'h': case 'H':           elem_size = 2; break;
            case 'i': case 'I': case 'f': elem_size = 4; break;
            case 'q': case 'Q': case 'd': elem_size = 8; break;
            default: return -1; /* unknown format char */
            }

            for (int r = 0; r < repeat; r++)
            {
                if (count >= kMaxFmtEntries) return -1;
                out[count].code = c;
                out[count].size = elem_size;
                count++;
            }
        }
        return count;
    }

    static int fmt_total_size(const FmtEntry *entries, int count)
    {
        int total = 0;
        for (int i = 0; i < count; i++)
            total += entries[i].size;
        return total;
    }

    /* Count value entries (excludes 'x' pad bytes) */
    static int fmt_value_count(const FmtEntry *entries, int count)
    {
        int n = 0;
        for (int i = 0; i < count; i++)
            if (entries[i].code != 'x') n++;
        return n;
    }

    /* ---- pack a single value into buf at offset ---- */
    static bool pack_one(uint8_t *buf, int offset, const FmtEntry &e,
                         Value val, bool need_swap, VM *vm)
    {
        uint8_t *dst = buf + offset;

        if (e.code == 'x')
        {
            *dst = 0;
            return true;
        }

        switch (e.code)
        {
        case 'b':
        {
            if (!is_int(val)) { vm->runtime_error("struct.pack: expected int for 'b'"); return false; }
            int8_t v = (int8_t)val.as.integer;
            memcpy(dst, &v, 1);
            break;
        }
        case 'B':
        {
            if (!is_int(val)) { vm->runtime_error("struct.pack: expected int for 'B'"); return false; }
            uint8_t v = (uint8_t)val.as.integer;
            memcpy(dst, &v, 1);
            break;
        }
        case 'h':
        {
            if (!is_int(val)) { vm->runtime_error("struct.pack: expected int for 'h'"); return false; }
            int16_t v = (int16_t)val.as.integer;
            memcpy(dst, &v, 2);
            if (need_swap) swap2(dst);
            break;
        }
        case 'H':
        {
            if (!is_int(val)) { vm->runtime_error("struct.pack: expected int for 'H'"); return false; }
            uint16_t v = (uint16_t)val.as.integer;
            memcpy(dst, &v, 2);
            if (need_swap) swap2(dst);
            break;
        }
        case 'i':
        {
            if (!is_int(val)) { vm->runtime_error("struct.pack: expected int for 'i'"); return false; }
            int32_t v = (int32_t)val.as.integer;
            memcpy(dst, &v, 4);
            if (need_swap) swap4(dst);
            break;
        }
        case 'I':
        {
            if (!is_int(val)) { vm->runtime_error("struct.pack: expected int for 'I'"); return false; }
            uint32_t v = (uint32_t)val.as.integer;
            memcpy(dst, &v, 4);
            if (need_swap) swap4(dst);
            break;
        }
        case 'q':
        {
            if (!is_int(val)) { vm->runtime_error("struct.pack: expected int for 'q'"); return false; }
            int64_t v = (int64_t)val.as.integer;
            memcpy(dst, &v, 8);
            if (need_swap) swap8(dst);
            break;
        }
        case 'Q':
        {
            if (!is_int(val)) { vm->runtime_error("struct.pack: expected int for 'Q'"); return false; }
            uint64_t v = (uint64_t)(unsigned)val.as.integer;
            memcpy(dst, &v, 8);
            if (need_swap) swap8(dst);
            break;
        }
        case 'f':
        {
            double d = is_float(val) ? val.as.number : is_int(val) ? (double)val.as.integer : 0;
            if (!is_float(val) && !is_int(val)) { vm->runtime_error("struct.pack: expected number for 'f'"); return false; }
            float v = (float)d;
            memcpy(dst, &v, 4);
            if (need_swap) swap4(dst);
            break;
        }
        case 'd':
        {
            double d = is_float(val) ? val.as.number : is_int(val) ? (double)val.as.integer : 0;
            if (!is_float(val) && !is_int(val)) { vm->runtime_error("struct.pack: expected number for 'd'"); return false; }
            memcpy(dst, &d, 8);
            if (need_swap) swap8(dst);
            break;
        }
        default: return false;
        }
        return true;
    }

    /* ---- unpack a single value from buf at offset ---- */
    static Value unpack_one(const uint8_t *buf, int offset,
                            const FmtEntry &e, bool need_swap)
    {
        uint8_t tmp[8];
        memcpy(tmp, buf + offset, (size_t)e.size);

        if (need_swap)
        {
            if (e.size == 2) swap2(tmp);
            else if (e.size == 4) swap4(tmp);
            else if (e.size == 8) swap8(tmp);
        }

        switch (e.code)
        {
        case 'b': { int8_t v;  memcpy(&v, tmp, 1); return val_int(v); }
        case 'B': { uint8_t v; memcpy(&v, tmp, 1); return val_int(v); }
        case 'h': { int16_t v; memcpy(&v, tmp, 2); return val_int(v); }
        case 'H': { uint16_t v; memcpy(&v, tmp, 2); return val_int(v); }
        case 'i': { int32_t v; memcpy(&v, tmp, 4); return val_int(v); }
        case 'I': { uint32_t v; memcpy(&v, tmp, 4); return val_int((int32_t)v); }
        case 'q': { int64_t v; memcpy(&v, tmp, 8); return val_int((int32_t)v); }
        case 'Q': { uint64_t v; memcpy(&v, tmp, 8); return val_int((int32_t)v); }
        case 'f': { float v;  memcpy(&v, tmp, 4); return val_float((double)v); }
        case 'd': { double v; memcpy(&v, tmp, 8); return val_float(v); }
        default: return val_nil();
        }
    }

    /* ---- native functions ---- */

    /* struct.pack(fmt, v1, v2, ...) → Uint8Array */
    static int nat_struct_pack(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("struct.pack: first argument must be format string");
            return -1;
        }

        FmtEntry entries[kMaxFmtEntries];
        bool need_swap = false;
        int count = parse_format(as_cstring(args[0]), entries, &need_swap);
        if (count < 0)
        {
            vm->runtime_error("struct.pack: invalid format string");
            return -1;
        }

        int val_count = fmt_value_count(entries, count);
        if (nargs - 1 != val_count)
        {
            vm->runtime_error("struct.pack: format requires %d values, got %d",
                              val_count, nargs - 1);
            return -1;
        }

        int total = fmt_total_size(entries, count);
        GC *gc = &vm->get_gc();
        ObjBuffer *buf = new_buffer(gc, BUF_UINT8, total);

        int offset = 0;
        int vi = 1; /* index into args (skip format string) */
        for (int i = 0; i < count; i++)
        {
            Value val = (entries[i].code == 'x') ? val_int(0) : args[vi++];
            if (!pack_one(buf->data, offset, entries[i], val, need_swap, vm))
                return -1;
            offset += entries[i].size;
        }

        args[0] = val_obj((Obj *)buf);
        return 1;
    }

    /* struct.unpack(fmt, buffer) → list */
    static int nat_struct_unpack(VM *vm, Value *args, int nargs)
    {
        if (nargs != 2 || !is_string(args[0]) || !is_buffer(args[1]))
        {
            vm->runtime_error("struct.unpack: expects (format_string, buffer)");
            return -1;
        }

        FmtEntry entries[kMaxFmtEntries];
        bool need_swap = false;
        int count = parse_format(as_cstring(args[0]), entries, &need_swap);
        if (count < 0)
        {
            vm->runtime_error("struct.unpack: invalid format string");
            return -1;
        }

        int total = fmt_total_size(entries, count);
        ObjBuffer *buf = as_buffer(args[1]);
        if (buf->count < total)
        {
            vm->runtime_error("struct.unpack: buffer too small (%d bytes, need %d)",
                              buf->count, total);
            return -1;
        }

        GC *gc = &vm->get_gc();
        ObjArray *result = new_array(gc);

        int offset = 0;
        for (int i = 0; i < count; i++)
        {
            if (entries[i].code != 'x')
            {
                Value v = unpack_one(buf->data, offset, entries[i], need_swap);
                array_push(gc, result, v);
            }
            offset += entries[i].size;
        }

        args[0] = val_obj((Obj *)result);
        return 1;
    }

    /* struct.calcsize(fmt) → int */
    static int nat_struct_calcsize(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        {
            vm->runtime_error("struct.calcsize: expects format string");
            return -1;
        }

        FmtEntry entries[kMaxFmtEntries];
        bool need_swap = false;
        int count = parse_format(as_cstring(args[0]), entries, &need_swap);
        if (count < 0)
        {
            vm->runtime_error("struct.calcsize: invalid format string");
            return -1;
        }

        args[0] = val_int(fmt_total_size(entries, count));
        return 1;
    }

    /* struct.pack_into(fmt, buffer, offset, v1, v2, ...) → None */
    static int nat_struct_pack_into(VM *vm, Value *args, int nargs)
    {
        if (nargs < 3 || !is_string(args[0]) || !is_buffer(args[1]) || !is_int(args[2]))
        {
            vm->runtime_error("struct.pack_into: expects (format, buffer, offset, ...)");
            return -1;
        }

        FmtEntry entries[kMaxFmtEntries];
        bool need_swap = false;
        int count = parse_format(as_cstring(args[0]), entries, &need_swap);
        if (count < 0)
        {
            vm->runtime_error("struct.pack_into: invalid format string");
            return -1;
        }

        int val_count = fmt_value_count(entries, count);
        if (nargs - 3 != val_count)
        {
            vm->runtime_error("struct.pack_into: format requires %d values, got %d",
                              val_count, nargs - 3);
            return -1;
        }

        ObjBuffer *buf = as_buffer(args[1]);
        int base_offset = args[2].as.integer;
        int total = fmt_total_size(entries, count);

        if (base_offset < 0 || base_offset + total > buf->count)
        {
            vm->runtime_error("struct.pack_into: write exceeds buffer bounds");
            return -1;
        }

        int offset = base_offset;
        int vi = 3;
        for (int i = 0; i < count; i++)
        {
            Value val = (entries[i].code == 'x') ? val_int(0) : args[vi++];
            if (!pack_one(buf->data, offset, entries[i], val, need_swap, vm))
                return -1;
            offset += entries[i].size;
        }

        return 0; /* returns nil */
    }

    /* struct.unpack_from(fmt, buffer, offset?) → list */
    static int nat_struct_unpack_from(VM *vm, Value *args, int nargs)
    {
        if (nargs < 2 || !is_string(args[0]) || !is_buffer(args[1]))
        {
            vm->runtime_error("struct.unpack_from: expects (format, buffer [, offset])");
            return -1;
        }

        int base_offset = 0;
        if (nargs >= 3)
        {
            if (!is_int(args[2])) { vm->runtime_error("struct.unpack_from: offset must be int"); return -1; }
            base_offset = args[2].as.integer;
        }

        FmtEntry entries[kMaxFmtEntries];
        bool need_swap = false;
        int count = parse_format(as_cstring(args[0]), entries, &need_swap);
        if (count < 0)
        {
            vm->runtime_error("struct.unpack_from: invalid format string");
            return -1;
        }

        int total = fmt_total_size(entries, count);
        ObjBuffer *buf = as_buffer(args[1]);

        if (base_offset < 0 || base_offset + total > buf->count)
        {
            vm->runtime_error("struct.unpack_from: read exceeds buffer bounds");
            return -1;
        }

        GC *gc = &vm->get_gc();
        ObjArray *result = new_array(gc);

        int offset = base_offset;
        for (int i = 0; i < count; i++)
        {
            if (entries[i].code != 'x')
            {
                Value v = unpack_one(buf->data, offset, entries[i], need_swap);
                array_push(gc, result, v);
            }
            offset += entries[i].size;
        }

        args[0] = val_obj((Obj *)result);
        return 1;
    }

    /* ---- module definition ---- */

    static const NativeReg struct_functions[] = {
        {"pack",        nat_struct_pack,        -1},
        {"unpack",      nat_struct_unpack,       2},
        {"calcsize",    nat_struct_calcsize,     1},
        {"pack_into",   nat_struct_pack_into,   -1},
        {"unpack_from", nat_struct_unpack_from, -1},
    };

    const NativeLib zen_lib_struct = {
        "struct",
        struct_functions,
        5,       /* num_functions */
        nullptr, /* constants */
        0,       /* num_constants */
        nullptr, /* init_fn */
    };

} /* namespace zen */
