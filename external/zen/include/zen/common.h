#ifndef ZEN_COMMON_H
#define ZEN_COMMON_H

/*
** common.h — Base types and VM configuration.
*/

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#ifdef _MSC_VER
#ifndef __builtin_expect
#define __builtin_expect(expr, value) (expr)
#endif
#ifndef __builtin_unreachable
#define __builtin_unreachable() __assume(0)
#endif
#ifndef __builtin_prefetch
#define __builtin_prefetch(address, read_write, locality) ((void)0)
#endif
#endif

namespace zen
{

    /* VM limits */
    constexpr int kMaxRegs = 250;                   /* registers per call frame */
    constexpr int kMaxFrames = 2000;                /* max call depth */
    constexpr int kMaxFiberDepth = 64;              /* max nested fiber resumes (C stack) */
    constexpr int kMaxConstants = 65536;            /* constant pool per function */
    constexpr int kOperatorSlotCount = 15;          /* fixed object operator slots */
    /* Globals are stored in a dynamic array (grows on demand).
    ** kInitGlobalCapacity = how big the table is allocated up front.
    ** kMaxGlobalsHard   = absolute ceiling (OP_GETGLOBAL/SETGLOBAL encode
    **                     the index as a 16-bit Bx field). */
    constexpr int kInitGlobalCapacity = 1024;
    constexpr int kMaxGlobalsHard = 65536;
    /* Backwards-compat alias: previously this was the hard cap. Kept so
    ** external code that referenced it still compiles. New code should use
    ** kMaxGlobalsHard for the ceiling, kInitGlobalCapacity for sizing. */
    constexpr int MAX_GLOBALS = kInitGlobalCapacity;
    constexpr size_t kGCInitThreshold = 1024 * 256; /* first GC at 256 KB */
    constexpr float kGCGrowFactor = 2.0f;

    /* Forward declarations */
    class VM;
    struct Obj;
    struct ObjString;
    struct ObjFunc;
    struct ObjUpvalue;
    struct ObjClosure;
    struct ObjFiber;
    struct ObjArray;
    struct ObjMap;
    struct ObjSet;
    struct ObjClass;
    struct ObjInstance;
    struct NativeStructDef;
    struct ObjNativeStruct;

} /* namespace zen */

#endif /* ZEN_COMMON_H */
