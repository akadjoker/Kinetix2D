/* =========================================================
** builtin_numpy.cpp — numpy module for Zen
**
** Mini NumPy over typed arrays (Float64Array by default).
** Element-wise ops, reductions, constructors.
**
** Usage:
**   import numpy as np
**   a = np.zeros(10)
**   b = np.ones(10)
**   c = np.add(a, b)
**   s = np.sum(c)
** ========================================================= */

#include "module.h"
#include "vm.h"
#include "memory.h"
#include <cstring>
#include <cstdint>
#include <cmath>

namespace zen
{

    /* ---- helpers ---- */

    static inline bool check_buffer(VM *vm, Value v, const char *fn)
    {
        if (!is_buffer(v))
        {
            vm->runtime_error("%s: expected buffer argument", fn);
            return false;
        }
        return true;
    }

    static inline bool check_same_len(VM *vm, ObjBuffer *a, ObjBuffer *b, const char *fn)
    {
        if (a->count != b->count)
        {
            vm->runtime_error("%s: buffers must have same length (%d vs %d)",
                              fn, a->count, b->count);
            return false;
        }
        return true;
    }

    /* clone buffer type + size, return empty */
    static ObjBuffer *clone_like(GC *gc, ObjBuffer *src)
    {
        return new_buffer(gc, src->btype, src->count);
    }

    /* ============================================================
    ** Constructors
    ** ============================================================ */

    /* zeros(n) → Float64Array of n zeros */
    static int nat_zeros(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_int(args[0]))
        {
            vm->runtime_error("zeros: expected int argument");
            return -1;
        }
        int32_t n = (int32_t)args[0].as.integer;
        if (n < 0) { vm->runtime_error("zeros: size must be non-negative"); return -1; }
        ObjBuffer *buf = new_buffer(&vm->get_gc(), BUF_FLOAT64, n);
        /* new_buffer already zeroes data */
        args[0] = val_obj((Obj *)buf);
        return 1;
    }

    /* ones(n) → Float64Array of n ones */
    static int nat_ones(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_int(args[0]))
        {
            vm->runtime_error("ones: expected int argument");
            return -1;
        }
        int32_t n = (int32_t)args[0].as.integer;
        if (n < 0) { vm->runtime_error("ones: size must be non-negative"); return -1; }
        ObjBuffer *buf = new_buffer(&vm->get_gc(), BUF_FLOAT64, n);
        buffer_fill(buf, 1.0);
        args[0] = val_obj((Obj *)buf);
        return 1;
    }

    /* full(n, val) → Float64Array of n copies of val */
    static int nat_full(VM *vm, Value *args, int nargs)
    {
        if (nargs != 2 || !is_int(args[0]))
        {
            vm->runtime_error("full: expected (int, number)");
            return -1;
        }
        int32_t n = (int32_t)args[0].as.integer;
        double v = 0;
        if (is_int(args[1])) v = (double)args[1].as.integer;
        else if (is_float(args[1])) v = args[1].as.number;
        else { vm->runtime_error("full: expected number for fill value"); return -1; }

        if (n < 0) { vm->runtime_error("full: size must be non-negative"); return -1; }
        ObjBuffer *buf = new_buffer(&vm->get_gc(), BUF_FLOAT64, n);
        buffer_fill(buf, v);
        args[0] = val_obj((Obj *)buf);
        return 1;
    }

    /* arange(stop) or arange(start, stop) or arange(start, stop, step) */
    static int nat_arange(VM *vm, Value *args, int nargs)
    {
        double start = 0, stop = 0, step = 1;
        if (nargs == 1)
        {
            if (is_int(args[0])) stop = (double)args[0].as.integer;
            else if (is_float(args[0])) stop = args[0].as.number;
            else { vm->runtime_error("arange: expected number"); return -1; }
        }
        else if (nargs == 2)
        {
            if (is_int(args[0])) start = (double)args[0].as.integer;
            else if (is_float(args[0])) start = args[0].as.number;
            else { vm->runtime_error("arange: expected number"); return -1; }

            if (is_int(args[1])) stop = (double)args[1].as.integer;
            else if (is_float(args[1])) stop = args[1].as.number;
            else { vm->runtime_error("arange: expected number"); return -1; }
        }
        else if (nargs == 3)
        {
            if (is_int(args[0])) start = (double)args[0].as.integer;
            else if (is_float(args[0])) start = args[0].as.number;
            else { vm->runtime_error("arange: expected number"); return -1; }

            if (is_int(args[1])) stop = (double)args[1].as.integer;
            else if (is_float(args[1])) stop = args[1].as.number;
            else { vm->runtime_error("arange: expected number"); return -1; }

            if (is_int(args[2])) step = (double)args[2].as.integer;
            else if (is_float(args[2])) step = args[2].as.number;
            else { vm->runtime_error("arange: expected number"); return -1; }
        }
        else
        {
            vm->runtime_error("arange: expected 1-3 arguments");
            return -1;
        }
        if (step == 0) { vm->runtime_error("arange: step cannot be zero"); return -1; }

        int32_t n = 0;
        if (step > 0 && start < stop)
            n = (int32_t)ceil((stop - start) / step);
        else if (step < 0 && start > stop)
            n = (int32_t)ceil((start - stop) / (-step));

        ObjBuffer *buf = new_buffer(&vm->get_gc(), BUF_FLOAT64, n);
        for (int32_t i = 0; i < n; i++)
            buffer_set(buf, i, start + i * step);

        args[0] = val_obj((Obj *)buf);
        return 1;
    }

    /* linspace(start, stop, n) → n evenly spaced values [start, stop] */
    static int nat_linspace(VM *vm, Value *args, int nargs)
    {
        if (nargs != 3)
        {
            vm->runtime_error("linspace: expected 3 arguments");
            return -1;
        }
        double start = 0, stop = 0;
        if (is_int(args[0])) start = (double)args[0].as.integer;
        else if (is_float(args[0])) start = args[0].as.number;
        else { vm->runtime_error("linspace: expected number"); return -1; }

        if (is_int(args[1])) stop = (double)args[1].as.integer;
        else if (is_float(args[1])) stop = args[1].as.number;
        else { vm->runtime_error("linspace: expected number"); return -1; }

        if (!is_int(args[2])) { vm->runtime_error("linspace: n must be int"); return -1; }
        int32_t n = (int32_t)args[2].as.integer;
        if (n < 0) { vm->runtime_error("linspace: n must be non-negative"); return -1; }

        ObjBuffer *buf = new_buffer(&vm->get_gc(), BUF_FLOAT64, n);
        if (n == 1)
        {
            buffer_set(buf, 0, start);
        }
        else if (n > 1)
        {
            double step = (stop - start) / (n - 1);
            for (int32_t i = 0; i < n; i++)
                buffer_set(buf, i, start + i * step);
        }
        args[0] = val_obj((Obj *)buf);
        return 1;
    }

    /* ============================================================
    ** Element-wise binary ops (buffer, buffer) → buffer
    ** Also supports scalar broadcast: (buffer, number) or (number, buffer)
    ** ============================================================ */

    typedef double (*BinOp)(double, double);

    static inline double op_add(double a, double b) { return a + b; }
    static inline double op_sub(double a, double b) { return a - b; }
    static inline double op_mul(double a, double b) { return a * b; }
    static inline double op_div(double a, double b) { return a / b; }
    static inline double op_mod(double a, double b) { return fmod(a, b); }
    static inline double op_pow(double a, double b) { return pow(a, b); }
    static inline double op_min2(double a, double b) { return a < b ? a : b; }
    static inline double op_max2(double a, double b) { return a > b ? a : b; }

    static inline double as_num(Value v)
    {
        if (is_int(v)) return (double)v.as.integer;
        return v.as.number;
    }

    static inline bool is_num(Value v)
    {
        return is_int(v) || is_float(v);
    }

    static int binop(VM *vm, Value *args, int nargs, BinOp fn, const char *name)
    {
        if (nargs != 2) { vm->runtime_error("%s: expected 2 arguments", name); return -1; }
        GC *gc = &vm->get_gc();

        bool a_buf = is_buffer(args[0]);
        bool b_buf = is_buffer(args[1]);
        bool a_num = is_num(args[0]);
        bool b_num = is_num(args[1]);

        if (a_buf && b_buf)
        {
            /* array op array */
            ObjBuffer *a = as_buffer(args[0]);
            ObjBuffer *b = as_buffer(args[1]);
            if (!check_same_len(vm, a, b, name)) return -1;
            ObjBuffer *out = clone_like(gc, a);
            for (int32_t i = 0; i < a->count; i++)
                buffer_set(out, i, fn(buffer_get(a, i), buffer_get(b, i)));
            args[0] = val_obj((Obj *)out);
            return 1;
        }
        else if (a_buf && b_num)
        {
            /* array op scalar */
            ObjBuffer *a = as_buffer(args[0]);
            double s = as_num(args[1]);
            ObjBuffer *out = clone_like(gc, a);
            for (int32_t i = 0; i < a->count; i++)
                buffer_set(out, i, fn(buffer_get(a, i), s));
            args[0] = val_obj((Obj *)out);
            return 1;
        }
        else if (a_num && b_buf)
        {
            /* scalar op array */
            ObjBuffer *b = as_buffer(args[1]);
            double s = as_num(args[0]);
            ObjBuffer *out = clone_like(gc, b);
            for (int32_t i = 0; i < b->count; i++)
                buffer_set(out, i, fn(s, buffer_get(b, i)));
            args[0] = val_obj((Obj *)out);
            return 1;
        }
        else
        {
            vm->runtime_error("%s: expected (buffer,buffer), (buffer,number), or (number,buffer)", name);
            return -1;
        }
    }

    static int nat_add(VM *vm, Value *a, int n)  { return binop(vm, a, n, op_add, "add"); }
    static int nat_sub(VM *vm, Value *a, int n)  { return binop(vm, a, n, op_sub, "subtract"); }
    static int nat_mul(VM *vm, Value *a, int n)  { return binop(vm, a, n, op_mul, "multiply"); }
    static int nat_div(VM *vm, Value *a, int n)  { return binop(vm, a, n, op_div, "divide"); }
    static int nat_mod(VM *vm, Value *a, int n)  { return binop(vm, a, n, op_mod, "mod"); }
    static int nat_power(VM *vm, Value *a, int n){ return binop(vm, a, n, op_pow, "power"); }
    static int nat_minimum(VM *vm, Value *a, int n){ return binop(vm, a, n, op_min2, "minimum"); }
    static int nat_maximum(VM *vm, Value *a, int n){ return binop(vm, a, n, op_max2, "maximum"); }

    /* ============================================================
    ** Element-wise unary ops
    ** ============================================================ */

    typedef double (*UnaryOp)(double);

    static inline double op_neg(double x) { return -x; }
    static inline double op_abs(double x) { return fabs(x); }
    static inline double op_sqrt(double x) { return sqrt(x); }
    static inline double op_sin(double x) { return sin(x); }
    static inline double op_cos(double x) { return cos(x); }
    static inline double op_exp(double x) { return exp(x); }
    static inline double op_log(double x) { return log(x); }
    static inline double op_floor(double x) { return floor(x); }
    static inline double op_ceil(double x)  { return ceil(x); }

    static int unaryop(VM *vm, Value *args, int nargs, UnaryOp fn, const char *name)
    {
        if (nargs != 1) { vm->runtime_error("%s: expected 1 argument", name); return -1; }
        if (!check_buffer(vm, args[0], name)) return -1;
        GC *gc = &vm->get_gc();
        ObjBuffer *a = as_buffer(args[0]);
        ObjBuffer *out = clone_like(gc, a);
        for (int32_t i = 0; i < a->count; i++)
            buffer_set(out, i, fn(buffer_get(a, i)));
        args[0] = val_obj((Obj *)out);
        return 1;
    }

    static int nat_negative(VM *vm, Value *a, int n) { return unaryop(vm, a, n, op_neg, "negative"); }
    static int nat_abs(VM *vm, Value *a, int n)      { return unaryop(vm, a, n, op_abs, "abs"); }
    static int nat_sqrt(VM *vm, Value *a, int n)     { return unaryop(vm, a, n, op_sqrt, "sqrt"); }
    static int nat_sin(VM *vm, Value *a, int n)      { return unaryop(vm, a, n, op_sin, "sin"); }
    static int nat_cos(VM *vm, Value *a, int n)      { return unaryop(vm, a, n, op_cos, "cos"); }
    static int nat_exp(VM *vm, Value *a, int n)      { return unaryop(vm, a, n, op_exp, "exp"); }
    static int nat_log(VM *vm, Value *a, int n)      { return unaryop(vm, a, n, op_log, "log"); }
    static int nat_floor(VM *vm, Value *a, int n)    { return unaryop(vm, a, n, op_floor, "floor"); }
    static int nat_ceil(VM *vm, Value *a, int n)     { return unaryop(vm, a, n, op_ceil, "ceil"); }

    /* ============================================================
    ** Reductions
    ** ============================================================ */

    /* sum(a) → float */
    static int nat_sum(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("sum: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "sum")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        double s = 0;
        for (int32_t i = 0; i < a->count; i++)
            s += buffer_get(a, i);
        args[0] = val_float(s);
        return 1;
    }

    /* prod(a) → float */
    static int nat_prod(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("prod: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "prod")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        double p = 1;
        for (int32_t i = 0; i < a->count; i++)
            p *= buffer_get(a, i);
        args[0] = val_float(p);
        return 1;
    }

    /* min(a) → float */
    static int nat_min(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("min: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "min")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        if (a->count == 0) { vm->runtime_error("min: empty buffer"); return -1; }
        double m = buffer_get(a, 0);
        for (int32_t i = 1; i < a->count; i++)
        {
            double v = buffer_get(a, i);
            if (v < m) m = v;
        }
        args[0] = val_float(m);
        return 1;
    }

    /* max(a) → float */
    static int nat_max(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("max: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "max")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        if (a->count == 0) { vm->runtime_error("max: empty buffer"); return -1; }
        double m = buffer_get(a, 0);
        for (int32_t i = 1; i < a->count; i++)
        {
            double v = buffer_get(a, i);
            if (v > m) m = v;
        }
        args[0] = val_float(m);
        return 1;
    }

    /* mean(a) → float */
    static int nat_mean(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("mean: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "mean")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        if (a->count == 0) { vm->runtime_error("mean: empty buffer"); return -1; }
        double s = 0;
        for (int32_t i = 0; i < a->count; i++)
            s += buffer_get(a, i);
        args[0] = val_float(s / a->count);
        return 1;
    }

    /* std(a) → float (population standard deviation) */
    static int nat_std(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("std: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "std")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        if (a->count == 0) { vm->runtime_error("std: empty buffer"); return -1; }
        double s = 0;
        for (int32_t i = 0; i < a->count; i++)
            s += buffer_get(a, i);
        double mean = s / a->count;
        double var = 0;
        for (int32_t i = 0; i < a->count; i++)
        {
            double d = buffer_get(a, i) - mean;
            var += d * d;
        }
        args[0] = val_float(sqrt(var / a->count));
        return 1;
    }

    /* dot(a, b) → float (inner product) */
    static int nat_dot(VM *vm, Value *args, int nargs)
    {
        if (nargs != 2) { vm->runtime_error("dot: expected 2 arguments"); return -1; }
        if (!check_buffer(vm, args[0], "dot") || !check_buffer(vm, args[1], "dot")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        ObjBuffer *b = as_buffer(args[1]);
        if (!check_same_len(vm, a, b, "dot")) return -1;
        double s = 0;
        for (int32_t i = 0; i < a->count; i++)
            s += buffer_get(a, i) * buffer_get(b, i);
        args[0] = val_float(s);
        return 1;
    }

    /* argmin(a) → int */
    static int nat_argmin(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("argmin: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "argmin")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        if (a->count == 0) { vm->runtime_error("argmin: empty buffer"); return -1; }
        int32_t idx = 0;
        double m = buffer_get(a, 0);
        for (int32_t i = 1; i < a->count; i++)
        {
            double v = buffer_get(a, i);
            if (v < m) { m = v; idx = i; }
        }
        args[0] = val_int(idx);
        return 1;
    }

    /* argmax(a) → int */
    static int nat_argmax(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("argmax: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "argmax")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        if (a->count == 0) { vm->runtime_error("argmax: empty buffer"); return -1; }
        int32_t idx = 0;
        double m = buffer_get(a, 0);
        for (int32_t i = 1; i < a->count; i++)
        {
            double v = buffer_get(a, i);
            if (v > m) { m = v; idx = i; }
        }
        args[0] = val_int(idx);
        return 1;
    }

    /* ============================================================
    ** Utility
    ** ============================================================ */

    /* zeros_like(a) → buffer of same type+size, filled with 0 */
    static int nat_zeros_like(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("zeros_like: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "zeros_like")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        ObjBuffer *out = new_buffer(&vm->get_gc(), a->btype, a->count);
        args[0] = val_obj((Obj *)out);
        return 1;
    }

    /* ones_like(a) → buffer of same type+size, filled with 1 */
    static int nat_ones_like(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("ones_like: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "ones_like")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        ObjBuffer *out = new_buffer(&vm->get_gc(), a->btype, a->count);
        buffer_fill(out, 1.0);
        args[0] = val_obj((Obj *)out);
        return 1;
    }

    /* array(list) → Float64Array from a list */
    static int nat_array(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_array(args[0]))
        {
            vm->runtime_error("array: expected list argument");
            return -1;
        }
        ObjArray *src = as_array(args[0]);
        int32_t count = arr_count(src);
        GC *gc = &vm->get_gc();
        ObjBuffer *buf = new_buffer(gc, BUF_FLOAT64, count);
        for (int32_t i = 0; i < count; i++)
        {
            double v = 0;
            if (is_int(src->data[i])) v = (double)src->data[i].as.integer;
            else if (is_float(src->data[i])) v = src->data[i].as.number;
            buffer_set(buf, i, v);
        }
        args[0] = val_obj((Obj *)buf);
        return 1;
    }

    /* tolist(a) → list from buffer */
    static int nat_tolist(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("tolist: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "tolist")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        GC *gc = &vm->get_gc();
        ObjArray *arr = new_array(gc);
        array_reserve(gc, arr, a->count);
        bool is_float_type = (a->btype == BUF_FLOAT32 || a->btype == BUF_FLOAT64);
        for (int32_t i = 0; i < a->count; i++)
        {
            double v = buffer_get(a, i);
            if (is_float_type)
                array_push(gc, arr, val_float(v));
            else
                array_push_int(gc, arr, (int32_t)v);
        }
        args[0] = val_obj((Obj *)arr);
        return 1;
    }

    /* clip(a, lo, hi) → element-wise clamp */
    static int nat_clip(VM *vm, Value *args, int nargs)
    {
        if (nargs != 3) { vm->runtime_error("clip: expected 3 arguments"); return -1; }
        if (!check_buffer(vm, args[0], "clip")) return -1;
        if (!is_num(args[1]) || !is_num(args[2]))
        {
            vm->runtime_error("clip: lo and hi must be numbers");
            return -1;
        }
        ObjBuffer *a = as_buffer(args[0]);
        double lo = as_num(args[1]);
        double hi = as_num(args[2]);
        GC *gc = &vm->get_gc();
        ObjBuffer *out = clone_like(gc, a);
        for (int32_t i = 0; i < a->count; i++)
        {
            double v = buffer_get(a, i);
            if (v < lo) v = lo;
            if (v > hi) v = hi;
            buffer_set(out, i, v);
        }
        args[0] = val_obj((Obj *)out);
        return 1;
    }

    /* cumsum(a) → cumulative sum */
    static int nat_cumsum(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("cumsum: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "cumsum")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        GC *gc = &vm->get_gc();
        ObjBuffer *out = clone_like(gc, a);
        double s = 0;
        for (int32_t i = 0; i < a->count; i++)
        {
            s += buffer_get(a, i);
            buffer_set(out, i, s);
        }
        args[0] = val_obj((Obj *)out);
        return 1;
    }

    /* diff(a) → differences (length n-1) */
    static int nat_diff(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1) { vm->runtime_error("diff: expected 1 argument"); return -1; }
        if (!check_buffer(vm, args[0], "diff")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        if (a->count < 2) { vm->runtime_error("diff: buffer must have at least 2 elements"); return -1; }
        GC *gc = &vm->get_gc();
        int32_t n = a->count - 1;
        ObjBuffer *out = new_buffer(gc, a->btype, n);
        for (int32_t i = 0; i < n; i++)
            buffer_set(out, i, buffer_get(a, i + 1) - buffer_get(a, i));
        args[0] = val_obj((Obj *)out);
        return 1;
    }

    /* where(cond, x, y) → element-wise: cond[i]!=0 ? x[i] : y[i] */
    static int nat_where(VM *vm, Value *args, int nargs)
    {
        if (nargs != 3) { vm->runtime_error("where: expected 3 arguments"); return -1; }
        if (!check_buffer(vm, args[0], "where")) return -1;
        if (!check_buffer(vm, args[1], "where")) return -1;
        if (!check_buffer(vm, args[2], "where")) return -1;
        ObjBuffer *cond = as_buffer(args[0]);
        ObjBuffer *x = as_buffer(args[1]);
        ObjBuffer *y = as_buffer(args[2]);
        if (!check_same_len(vm, cond, x, "where")) return -1;
        if (!check_same_len(vm, cond, y, "where")) return -1;
        GC *gc = &vm->get_gc();
        ObjBuffer *out = clone_like(gc, x);
        for (int32_t i = 0; i < cond->count; i++)
            buffer_set(out, i, buffer_get(cond, i) != 0 ? buffer_get(x, i) : buffer_get(y, i));
        args[0] = val_obj((Obj *)out);
        return 1;
    }

    /* concatenate(a, b) → new buffer with elements of a followed by b */
    static int nat_concatenate(VM *vm, Value *args, int nargs)
    {
        if (nargs != 2) { vm->runtime_error("concatenate: expected 2 arguments"); return -1; }
        if (!check_buffer(vm, args[0], "concatenate")) return -1;
        if (!check_buffer(vm, args[1], "concatenate")) return -1;
        ObjBuffer *a = as_buffer(args[0]);
        ObjBuffer *b = as_buffer(args[1]);
        GC *gc = &vm->get_gc();
        int32_t total = a->count + b->count;
        ObjBuffer *out = new_buffer(gc, a->btype, total);
        for (int32_t i = 0; i < a->count; i++)
            buffer_set(out, i, buffer_get(a, i));
        for (int32_t i = 0; i < b->count; i++)
            buffer_set(out, a->count + i, buffer_get(b, i));
        args[0] = val_obj((Obj *)out);
        return 1;
    }

    /* ============================================================
    ** Module registration
    ** ============================================================ */

    static const NativeReg numpy_functions[] = {
        /* constructors */
        {"zeros",       nat_zeros,       1},
        {"ones",        nat_ones,        1},
        {"full",        nat_full,        2},
        {"arange",      nat_arange,     -1},
        {"linspace",    nat_linspace,    3},
        {"array",       nat_array,       1},
        {"zeros_like",  nat_zeros_like,  1},
        {"ones_like",   nat_ones_like,   1},
        /* element-wise binary */
        {"add",         nat_add,        2},
        {"subtract",    nat_sub,        2},
        {"multiply",    nat_mul,        2},
        {"divide",      nat_div,        2},
        {"mod",         nat_mod,        2},
        {"power",       nat_power,      2},
        {"minimum",     nat_minimum,    2},
        {"maximum",     nat_maximum,    2},
        /* element-wise unary */
        {"negative",    nat_negative,   1},
        {"abs",         nat_abs,        1},
        {"sqrt",        nat_sqrt,       1},
        {"sin",         nat_sin,        1},
        {"cos",         nat_cos,        1},
        {"exp",         nat_exp,        1},
        {"log",         nat_log,        1},
        {"floor",       nat_floor,      1},
        {"ceil",        nat_ceil,       1},
        /* reductions */
        {"sum",         nat_sum,        1},
        {"prod",        nat_prod,       1},
        {"min",         nat_min,        1},
        {"max",         nat_max,        1},
        {"mean",        nat_mean,       1},
        {"std",         nat_std,        1},
        {"dot",         nat_dot,        2},
        {"argmin",      nat_argmin,     1},
        {"argmax",      nat_argmax,     1},
        /* utility */
        {"tolist",      nat_tolist,     1},
        {"clip",        nat_clip,       3},
        {"cumsum",      nat_cumsum,     1},
        {"diff",        nat_diff,       1},
        {"where",       nat_where,      3},
        {"concatenate", nat_concatenate, 2},
        {nullptr, nullptr, 0}
    };

    extern const NativeLib zen_lib_numpy = {
        "numpy",            /* import numpy */
        numpy_functions,
        40,                 /* num_functions */
        nullptr,            /* no constants */
        0,
        nullptr             /* no init_fn */
    };

} /* namespace zen */
