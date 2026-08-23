/* =========================================================
** builtin_math.cpp — math module for Zen
**
** Usage:
**   import math
**   math.sin(x), math.sqrt(x), math.pi, ...
**
**   from math import sin, cos, pi
** ========================================================= */

#include "module.h"
#include "vm.h"
#include "memory.h"
#include <cmath>
#include <cstdlib>

namespace zen
{

#define MATH_1(name, fn)                                                      \
    static int nat_math_##name(VM *vm, Value *args, int nargs)                \
    {                                                                         \
        if (nargs != 1)                                                       \
        {                                                                     \
            vm->runtime_error(#name "() expected 1 argument, got %d", nargs); \
            return 0;                                                         \
        }                                                                     \
        args[0] = val_float(fn(to_number(args[0])));                          \
        return 1;                                                             \
    }

#define MATH_2(name, fn)                                                       \
    static int nat_math_##name(VM *vm, Value *args, int nargs)                 \
    {                                                                          \
        if (nargs != 2)                                                        \
        {                                                                      \
            vm->runtime_error(#name "() expected 2 arguments, got %d", nargs); \
            return 0;                                                          \
        }                                                                      \
        args[0] = val_float(fn(to_number(args[0]),                             \
                               to_number(args[1])));                           \
        return 1;                                                              \
    }

    MATH_1(sin, ::sin)
    MATH_1(cos, ::cos)
    MATH_1(tan, ::tan)
    MATH_1(asin, ::asin)
    MATH_1(acos, ::acos)
    MATH_1(atan, ::atan)
    MATH_2(atan2, ::atan2)
    MATH_1(sqrt, ::sqrt)
    MATH_1(cbrt, ::cbrt)
    MATH_1(exp, ::exp)
    MATH_1(log, ::log)
    MATH_1(log2, ::log2)
    MATH_1(log10, ::log10)
    MATH_1(ceil, ::ceil)
    MATH_1(floor, ::floor)
    MATH_2(pow, ::pow)
    MATH_1(abs, ::fabs)

    static int nat_math_round(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        args[0] = val_float(::round(to_number(args[0])));
        return 1;
    }

    static int nat_math_min(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        double a = to_number(args[0]);
        double b = to_number(args[1]);
        args[0] = val_float(a < b ? a : b);
        return 1;
    }

    static int nat_math_max(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        double a = to_number(args[0]);
        double b = to_number(args[1]);
        args[0] = val_float(a > b ? a : b);
        return 1;
    }

    static int nat_math_clamp(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        double v = to_number(args[0]);
        double lo = to_number(args[1]);
        double hi = to_number(args[2]);
        if (v < lo)
            v = lo;
        if (v > hi)
            v = hi;
        args[0] = val_float(v);
        return 1;
    }

    static int nat_math_lerp(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        double a = to_number(args[0]);
        double b = to_number(args[1]);
        double t = to_number(args[2]);
        args[0] = val_float(a + (b - a) * t);
        return 1;
    }

    static int nat_math_hermite(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        double value1 = to_number(args[0]);
        double tangent1 = to_number(args[1]);
        double value2 = to_number(args[2]);
        double tangent2 = to_number(args[3]);
        double amount = to_number(args[4]);

        if (amount == 0.0)
        {
            args[0] = val_float(value1);
            return 1;
        }
        if (amount == 1.0)
        {
            args[0] = val_float(value2);
            return 1;
        }

        double s2 = amount * amount;
        double s3 = s2 * amount;
        double result = (2.0 * value1 - 2.0 * value2 + tangent2 + tangent1) * s3 + (3.0 * value2 - 3.0 * value1 - 2.0 * tangent1 - tangent2) * s2 + tangent1 * amount + value1;
        args[0] = val_float(result);
        return 1;
    }

    static int nat_math_smooth_step(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        double value1 = to_number(args[0]);
        double value2 = to_number(args[1]);
        double amount = to_number(args[2]);
        if (amount < 0.0)
            amount = 0.0;
        if (amount > 1.0)
            amount = 1.0;

        double s2 = amount * amount;
        double s3 = s2 * amount;
        args[0] = val_float((2.0 * value1 - 2.0 * value2) * s3 + (3.0 * value2 - 3.0 * value1) * s2 + value1);
        return 1;
    }

    static int nat_math_ping_pong(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        double t = to_number(args[0]);
        double length = to_number(args[1]);
        if (length <= 0.0)
        {
            args[0] = val_float(0.0);
            return 1;
        }

        double wrapped = std::fmod(t, length * 2.0);
        if (wrapped < 0.0)
            wrapped += length * 2.0;
        args[0] = val_float(length - std::fabs(wrapped - length));
        return 1;
    }

    static double shortest_angle_delta(double from_deg, double to_deg)
    {
        double delta = to_deg - from_deg;
        while (delta > 180.0)
            delta -= 360.0;
        while (delta < -180.0)
            delta += 360.0;
        return delta;
    }

    static int nat_math_angle_delta(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        args[0] = val_float(shortest_angle_delta(to_number(args[0]), to_number(args[1])));
        return 1;
    }

    static int nat_math_lerp_angle(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        double current = to_number(args[0]);
        double target = to_number(args[1]);
        double t = to_number(args[2]);
        args[0] = val_float(current + shortest_angle_delta(current, target) * t);
        return 1;
    }

    /* random()        → float in [0, 1)
       random(max)     → float in [0, max)
       random(min,max) → float in [min, max) */
    static int nat_math_random(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        double r = (double)::rand() / ((double)RAND_MAX + 1.0);
        if (nargs == 0) {
            args[0] = val_float(r);
        } else if (nargs == 1) {
            double mx = to_number(args[0]);
            args[0] = val_float(r * mx);
        } else {
            double mn = to_number(args[0]);
            double mx = to_number(args[1]);
            args[0] = val_float(mn + r * (mx - mn));
        }
        return 1;
    }

    /* hypot(a, b) */
    static int nat_math_hypot(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        double a = to_number(args[0]);
        double b = to_number(args[1]);
        args[0] = val_float(::sqrt(a * a + b * b));
        return 1;
    }

    /* isnan(x) */
    static int nat_math_isnan(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        args[0] = val_bool(std::isnan(to_number(args[0])));
        return 1;
    }

    /* isinf(x) */
    static int nat_math_isinf(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        args[0] = val_bool(std::isinf(to_number(args[0])));
        return 1;
    }

    static int nat_math_radians(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        args[0] = val_float(to_number(args[0]) * 3.14159265358979323846 / 180.0);
        return 1;
    }

    static int nat_math_degrees(VM *vm, Value *args, int nargs)
    {
        (void)vm;
        (void)nargs;
        args[0] = val_float(to_number(args[0]) * 180.0 / 3.14159265358979323846);
        return 1;
    }

    /* ----- function table ----- */

    static const NativeReg math_functions[] = {
        {"sin", nat_math_sin, 1},
        {"cos", nat_math_cos, 1},
        {"tan", nat_math_tan, 1},
        {"asin", nat_math_asin, 1},
        {"acos", nat_math_acos, 1},
        {"atan", nat_math_atan, 1},
        {"atan2", nat_math_atan2, 2},
        {"sqrt", nat_math_sqrt, 1},
        {"cbrt", nat_math_cbrt, 1},
        {"exp", nat_math_exp, 1},
        {"log", nat_math_log, 1},
        {"log2", nat_math_log2, 1},
        {"log10", nat_math_log10, 1},
        {"ceil", nat_math_ceil, 1},
        {"floor", nat_math_floor, 1},
        {"round", nat_math_round, 1},
        {"pow", nat_math_pow, 2},
        {"abs", nat_math_abs, 1},
        {"min", nat_math_min, 2},
        {"max", nat_math_max, 2},
        {"clamp", nat_math_clamp, 3},
        {"lerp", nat_math_lerp, 3},
        {"hermite", nat_math_hermite, 5},
        {"smooth_step", nat_math_smooth_step, 3},
        {"ping_pong", nat_math_ping_pong, 2},
        {"angle_delta", nat_math_angle_delta, 2},
        {"lerp_angle", nat_math_lerp_angle, 3},
        {"random", nat_math_random, -1},
        {"radians", nat_math_radians, 1},
        {"degrees", nat_math_degrees, 1},
        {"hypot", nat_math_hypot, 2},
        {"isnan", nat_math_isnan, 1},
        {"isinf", nat_math_isinf, 1},
    };

    /* ----- constants ----- */

    /* Build constants at static init time (C++ zero-init + runtime assign) */
    static NativeConst math_constants[5];
    static bool math_constants_inited = false;

    static void ensure_math_constants()
    {
        if (math_constants_inited)
            return;
        math_constants[0] = {"pi", val_float(3.14159265358979323846)};
        math_constants[1] = {"e", val_float(2.71828182845904523536)};
        math_constants[2] = {"tau", val_float(6.28318530717958647692)};
        math_constants[3] = {"inf", val_float(1.0 / 0.0)};
        math_constants[4] = {"nan", val_float(0.0 / 0.0)};
        math_constants_inited = true;
    }

    static void math_lib_init(VM *vm)
    {
        (void)vm;
        ensure_math_constants();
    }

    const NativeLib zen_lib_math = {
        "math",
        math_functions,
        31, /* num_functions */
        math_constants,
        5, /* num_constants */
        math_lib_init,
    };

} /* namespace zen */
