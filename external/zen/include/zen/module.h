#ifndef ZEN_MODULE_H
#define ZEN_MODULE_H

#include "value.h"

namespace zen
{
    class VM; /* forward */

    typedef int (*NativeFn)(VM *vm, Value *args, int nargs);

    struct NativeReg
    {
        const char *name;
        NativeFn fn;
        int arity; /* -1 = variadic */
    };

    struct NativeConst
    {
        const char *name;
        Value value;
    };

    /* Called once when a lib is opened — use to register native structs/classes.
    ** Called before functions/constants are put in globals, so types are available
    ** when init_fn returns. VM is fully alive at this point. */
    typedef void (*NativeLibInitFn)(VM *vm);

    struct NativeLib
    {
        const char *name;            /* "math", "os" */
        const NativeReg *functions;  /* null-terminated array */
        int num_functions;
        const NativeConst *constants; /* null-terminated array (can be nullptr) */
        int num_constants;
        NativeLibInitFn init_fn;     /* optional — register structs/classes here */
    };

    /* Builtin library openers */
    extern const NativeLib zen_lib_base;  /* always open — no import needed */
    extern const NativeLib zen_lib_math;  /* import math */
    extern const NativeLib zen_lib_time;  /* import time */
    extern const NativeLib zen_lib_struct; /* import struct */
    extern const NativeLib zen_lib_numpy;  /* import numpy */
    extern const NativeLib zen_lib_io;     /* import io */
    extern const NativeLib zen_lib_os;     /* import os */
    extern const NativeLib zen_lib_path;   /* import path */
    extern const NativeLib zen_lib_json;   /* import json */
    extern const NativeLib zen_lib_net;    /* import net */
    extern const NativeLib zen_lib_http;   /* import http */

} /* namespace zen */

#endif /* ZEN_MODULE_H */
