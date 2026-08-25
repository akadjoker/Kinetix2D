/* =========================================================
** builtin_time.cpp — time module for Zen
**
** import time
**   time.time()      → float seconds since epoch (wall clock)
**   time.monotonic() → float seconds monotonic (for benchmarks)
**   time.sleep(s)    → sleep s seconds (float ok)
** ========================================================= */

#include "module.h"
#include "vm.h"
#include "memory.h"
#include "platform_time.h"
#include <cmath>

namespace zen
{
/* time.time() → float: seconds since Unix epoch */
static int nat_time_time(VM* vm, Value* args, int nargs)
{
    (void)vm;
    (void)nargs;
    args[0] = val_float(platform_wall_seconds());
    return 1;
}

/* time.monotonic() → float: monotonic seconds (benchmark safe) */
static int nat_time_monotonic(VM* vm, Value* args, int nargs)
{
    (void)vm;
    (void)nargs;
    args[0] = val_float(platform_monotonic_seconds());
    return 1;
}

/* time.perf_counter() → alias for monotonic */
static int nat_time_perf_counter(VM* vm, Value* args, int nargs)
{
    return nat_time_monotonic(vm, args, nargs);
}

/* time.sleep(seconds) → nil */
static int nat_time_sleep(VM* vm, Value* args, int nargs)
{
    (void)vm;
    (void)nargs;
    double secs = to_number(args[0]);
    if (secs > 0.0)
    {
        platform_sleep_seconds(secs);
    }
    return 0;
}

static const NativeReg time_functions[] = {
    {"time", nat_time_time, 0},
    {"monotonic", nat_time_monotonic, 0},
    {"perf_counter", nat_time_perf_counter, 0},
    {"sleep", nat_time_sleep, 1},
};

const NativeLib zen_lib_time = {
    "time", time_functions, 4, nullptr, 0, nullptr,
};

} /* namespace zen */
