#include "vm.h"
#include "compiler.h"
#include "debug.h"
#include "name_tables.h"
#include "zenconf.h"
#include <cassert>
#include <cmath>
#include <ctime>

namespace zen
{

    static inline void copy_native_results(Value *dst, Value *src, int nret, int nresults)
    {
        int wanted = nresults <= 0 ? 0 : nresults;
        int copy_count = nret > 0 ? (nret < wanted ? nret : wanted) : 0;
        for (int j = 0; j < copy_count; j++)
            dst[j] = src[j];
        for (int j = copy_count; j < wanted; j++)
            dst[j] = val_nil();
    }

    /* Call a native function with GC paused — natives may do multi-step
       allocations (new_array + push, new_map + set, etc.) that must not
       be interrupted by a collection. */
    static inline int call_native(VM *vm, NativeFn fn, Value *args, int nargs)
    {
        gc_pause(&vm->get_gc());
        int ret = fn(vm, args, nargs);
        gc_resume(&vm->get_gc());
        return ret;
    }

    /* Clear unused registers in a new call frame so GC never sees stale Values.
       Call AFTER setting stack_top = base + num_regs. */
    static inline void clear_new_regs(Value *base, int first_used, int num_regs)
    {
        int count = num_regs - first_used;
        if (count > 0)
            memset(&base[first_used], 0, count * sizeof(Value));
    }

    static inline const char *val_type_str(Value v)
    {
        switch (v.type)
        {
        case VAL_NIL:
            return "nil";
        case VAL_BOOL:
            return "bool";
        case VAL_INT:
            return "int";
        case VAL_FLOAT:
            return "float";
        case VAL_SMALL_STRING:
            return "string";
        case VAL_OBJ:
            switch (v.as.obj->type)
            {
            case OBJ_STRING:
                return "string";
            case OBJ_FUNC:
                return "function";
            case OBJ_NATIVE:
                return "native function";
            case OBJ_CLOSURE:
                return "function";
            case OBJ_ARRAY:
                return "array";
            case OBJ_MAP:
                return "map";
            case OBJ_CLASS:
                return "class";
            case OBJ_INSTANCE:
                return "instance";
            case OBJ_STRUCT_DEF:
                return "struct_def";
            case OBJ_STRUCT:
                return "struct";
            case OBJ_BUFFER:
                return "buffer";
            case OBJ_NATIVE_STRUCT_DEF:
                return "native_struct_def";
            case OBJ_NATIVE_STRUCT:
                return "native_struct";
            default:
                return "object";
            }
        default:
            return "unknown";
        }
    }

    static inline int int_to_cstr(int64_t n, char *buf)
    {
        bool neg = n < 0;
        uint64_t u = neg ? -(uint64_t)n : (uint64_t)n;
        char tmp[21];
        int i = 20;
        tmp[i] = '\0';
        do
        {
            tmp[--i] = '0' + (char)(u % 10);
            u /= 10;
        } while (u);
        if (neg)
            tmp[--i] = '-';
        int len = 20 - i;
        memcpy(buf, tmp + i, (size_t)len + 1);
        return len;
    }

    /* Python-style value printing (for print() statement) */
    static void print_value_py(Value v, bool repr)
    {
        if (is_nil(v))
        {
            zen_writes("None");
        }
        else if (is_bool(v))
        {
            zen_writes(v.as.boolean ? "True" : "False");
        }
        else if (is_int(v))
        {
            char buf[21];
            int len = int_to_cstr(v.as.integer, buf);
            zen_write(buf, (size_t)len);
        }
        else if (is_float(v))
        {
            char buf[32];
            int len = snprintf(buf, sizeof(buf), "%g", v.as.number);
            zen_write(buf, (size_t)len);
        }
        else if (is_string(v))
        {
            ObjString *s = as_string(v);
            if (repr)
            {
                zen_writes("'");
                zen_write(s->chars, (size_t)s->length);
                zen_writes("'");
            }
            else
            {
                zen_write(s->chars, (size_t)s->length);
            }
        }
        else if (is_array(v))
        {
            ObjArray *a = as_array(v);
            int32_t n = arr_count(a);
            zen_writes("[");
            for (int32_t i = 0; i < n; i++)
            {
                if (i > 0)
                    zen_writes(", ");
                print_value_py(a->data[i], true);
            }
            zen_writes("]");
        }
        else if (is_map(v))
        {
            ObjMap *m = as_map(v);
            zen_writes("{");
            bool first = true;
            for (int32_t i = 0; i < m->capacity; i++)
            {
                if (m->nodes[i].hash == 0xFFFFFFFFu)
                    continue;
                if (!first)
                    zen_writes(", ");
                first = false;
                print_value_py(m->nodes[i].key, true);
                zen_writes(": ");
                print_value_py(m->nodes[i].value, true);
            }
            zen_writes("}");
        }
        else if (is_set(v))
        {
            ObjSet *s = as_set(v);
            zen_writes("{");
            bool first = true;
            for (int32_t i = 0; i < s->capacity; i++)
            {
                if (s->nodes[i].hash == 0xFFFFFFFFu)
                    continue;
                if (!first)
                    zen_writes(", ");
                first = false;
                print_value_py(s->nodes[i].key, true);
            }
            zen_writes("}");
        }
        else if (is_instance(v))
        {
            ObjInstance *inst = as_instance(v);
            char buf[128];
            snprintf(buf, sizeof(buf), "<%s instance>", inst->klass->name->chars);
            zen_writes(buf);
        }
        else
        {
            zen_writes("<object>");
        }
    }

    static void dump_value_rec(Value v, int depth)
    {
        static const int MAX_DEPTH = 8;
        auto do_indent = [](int d)
        {
            for (int i = 0; i < d * 2; i++)
                putchar(' ');
        };

        if (is_nil(v))
        {
            printf("nil");
        }
        else if (is_bool(v))
        {
            printf(v.as.boolean ? "true" : "false");
        }
        else if (is_int(v))
        {
            printf("%lld", (long long)v.as.integer);
        }
        else if (is_float(v))
        {
            printf("%g", v.as.number);
        }
        else if (is_string(v))
        {
            printf("\"%s\"", as_cstring(v));
        }
        else if (is_array(v))
        {
            ObjArray *a = as_array(v);
            int32_t n = arr_count(a);
            if (n == 0)
            {
                printf("[]");
                return;
            }
            if (depth >= MAX_DEPTH)
            {
                printf("<array[%d]>", n);
                return;
            }
            printf("[\n");
            for (int32_t i = 0; i < n; i++)
            {
                do_indent(depth + 1);
                dump_value_rec(a->data[i], depth + 1);
                printf(",\n");
            }
            do_indent(depth);
            printf("]");
        }
        else if (is_map(v))
        {
            ObjMap *m = as_map(v);
            if (m->count == 0)
            {
                printf("{}");
                return;
            }
            if (depth >= MAX_DEPTH)
            {
                printf("<map[%d]>", m->count);
                return;
            }
            printf("{\n");
            for (int32_t i = 0; i < m->capacity; i++)
            {
                if (m->nodes[i].hash == 0xFFFFFFFFu)
                    continue;
                do_indent(depth + 1);
                dump_value_rec(m->nodes[i].key, depth + 1);
                printf(": ");
                dump_value_rec(m->nodes[i].value, depth + 1);
                printf(",\n");
            }
            do_indent(depth);
            printf("}");
        }
        else if (is_set(v))
        {
            ObjSet *s = as_set(v);
            if (s->count == 0)
            {
                printf("#{}");
                return;
            }
            if (depth >= MAX_DEPTH)
            {
                printf("<set[%d]>", s->count);
                return;
            }
            printf("#{\n");
            for (int32_t i = 0; i < s->capacity; i++)
            {
                if (s->nodes[i].hash == 0xFFFFFFFFu)
                    continue;
                do_indent(depth + 1);
                dump_value_rec(s->nodes[i].key, depth + 1);
                printf(",\n");
            }
            do_indent(depth);
            printf("}");
        }
        else if (is_obj(v))
        {
            Obj *obj = v.as.obj;
            switch (obj->type)
            {
            case OBJ_FUNC:
            case OBJ_CLOSURE:
                printf("<function>");
                break;
            case OBJ_NATIVE:
                printf("<native %s>", ((ObjNative *)obj)->name->chars);
                break;
            case OBJ_INSTANCE:
                printf("<instance %s>", ((ObjInstance *)obj)->klass->name->chars);
                break;
            case OBJ_STRUCT:
                printf("<struct %s>", ((ObjStruct *)obj)->def->name->chars);
                break;
            case OBJ_NATIVE_STRUCT:
                printf("<native_struct %s>", ((ObjNativeStruct *)obj)->def->name->chars);
                break;
            case OBJ_CLASS:
                printf("<class %s>", ((ObjClass *)obj)->name->chars);
                break;
            default:
                printf("<object>");
                break;
            }
        }
        else
        {
            printf("?");
        }
    }

    static const char *value_debug_type(Value v)
    {
        switch (v.type)
        {
        case VAL_NIL:
            return "None";
        case VAL_BOOL:
            return "bool";
        case VAL_INT:
            return "int";
        case VAL_FLOAT:
            return "float";
        case VAL_PTR:
            return "ptr";
        case VAL_SMALL_STRING:
            return "string";
        case VAL_OBJ:
            if (!v.as.obj)
                return "obj(null)";
            switch (v.as.obj->type)
            {
            case OBJ_STRING:
                return "string";
            case OBJ_FUNC:
                return "function";
            case OBJ_NATIVE:
                return "native";
            case OBJ_UPVALUE:
                return "upvalue";
            case OBJ_CLOSURE:
                return "closure";
            case OBJ_FIBER:
                return "fiber";

            case OBJ_ARRAY:
                return "array";
            case OBJ_MAP:
                return "map";
            case OBJ_SET:
                return "set";
            case OBJ_BUFFER:
                return "buffer";
            case OBJ_CLASS:
                return "class";
            case OBJ_INSTANCE:
                return "instance";
            case OBJ_STRUCT_DEF:
                return "struct_def";
            case OBJ_STRUCT:
                return "struct";
            case OBJ_NATIVE_STRUCT_DEF:
                return "native_struct_def";
            case OBJ_NATIVE_STRUCT:
                return "native_struct";
            case OBJ_RANGE:
                return "range";
            }
            return "obj(unknown)";
        }
        return "unknown";
    }

    static bool instance_has_method_slot(Value receiver, int slot)
    {
        if (!is_instance(receiver) || slot < 0)
            return false;

        ObjClass *klass = as_instance(receiver)->klass;
        return slot < VM::SLOT_OPERATOR_COUNT && !is_nil(klass->operator_slots[slot]);
    }

    static bool try_binary_operator(VM *vm, Value lhs, Value rhs,
                                    int slot, int reflected_slot,
                                    Value *out)
    {
        if (instance_has_method_slot(lhs, slot))
        {
            Value args[1] = {rhs};
            *out = vm->invoke_operator(lhs, slot, args, 1);
            return true;
        }

        if (reflected_slot >= 0 && instance_has_method_slot(rhs, reflected_slot))
        {
            Value args[1] = {lhs};
            *out = vm->invoke_operator(rhs, reflected_slot, args, 1);
            return true;
        }

        return false;
    }

    static bool try_unary_operator(VM *vm, Value operand, int slot, Value *out)
    {
        if (!instance_has_method_slot(operand, slot))
            return false;

        *out = vm->invoke_operator(operand, slot, nullptr, 0);
        return true;
    }

    static bool try_string_operator(VM *vm, Value receiver, Value *out)
    {
        if (!instance_has_method_slot(receiver, VM::SLOT_STR))
            return false;

        *out = vm->invoke_operator(receiver, VM::SLOT_STR, nullptr, 0);
        return true;
    }

    static Value default_to_string(GC *gc, Value v)
    {
        if (is_string(v))
            return v;

        char buf[64];
        int len = 0;
        if (is_nil(v))
            len = snprintf(buf, sizeof(buf), "None");
        else if (is_bool(v))
            len = snprintf(buf, sizeof(buf), "%s", v.as.boolean ? "True" : "False");
        else if (is_int(v))
            len = int_to_cstr(v.as.integer, buf);
        else if (is_float(v))
            len = snprintf(buf, sizeof(buf), "%g", v.as.number);
        else
            len = snprintf(buf, sizeof(buf), "<object>");

        return val_obj((Obj *)new_string(gc, buf, len));
    }

    void VM::execute(ObjFiber *fiber)
    {
        /* Cache hot state em locals */
        CallFrame *frame = &fiber->frames[fiber->frame_count - 1];
        Instruction *ip = frame->ip;
        Value *R = frame->base;
        Value *K = frame->func->constants;
        ObjUpvalue **UV = frame->closure ? frame->closure->upvalues : nullptr;

/* Macro para reload após CALL/RETURN (frame mudou) */
#define LOAD_STATE()                                \
    frame = &fiber->frames[fiber->frame_count - 1]; \
    ip = frame->ip;                                 \
    R = frame->base;                                \
    K = frame->func->constants;                     \
    UV = frame->closure ? frame->closure->upvalues : nullptr

#define SAVE_IP() frame->ip = ip

/* Save IP before runtime errors so stack traces report correct lines */
#define RT_ERROR(...)               \
    do                              \
    {                               \
        SAVE_IP();                  \
        runtime_error(__VA_ARGS__); \
        return;                     \
    } while (0)

/* Check that the fiber's data stack has room for a new frame */
#define CHECK_STACK_SPACE(fiber_, base_, num_regs_)                       \
    do                                                                    \
    {                                                                     \
        if ((base_) + (num_regs_) > (fiber_)->stack + (fiber_)->stack_capacity) \
        {                                                                 \
            RT_ERROR("stack overflow (data)");                            \
        }                                                                 \
    } while (0)

/* Aritmética helpers — use int64_t wrapping via unsigned cast to avoid UB */
#define NUM_BINOP(op)                                                         \
    do                                                                        \
    {                                                                         \
        Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];                             \
        if (vb.type == VAL_INT && vc.type == VAL_INT)                         \
            R[ZEN_A(i)] = val_int((int64_t)((uint64_t)vb.as.integer           \
                                                op(uint64_t) vc.as.integer)); \
        else                                                                  \
            R[ZEN_A(i)] = val_float(to_number(vb) op to_number(vc));          \
    } while (0)

        /* =================================================================
        **  DISPATCH SETUP
        ** ================================================================= */

#ifdef ZEN_COMPUTED_GOTO

        /* --- Computed goto (GCC/Clang) --- */
        static const void *const dispatch_table[] = {
            &&lbl_OP_LOADNIL,
            &&lbl_OP_LOADBOOL,
            &&lbl_OP_LOADK,
            &&lbl_OP_LOADI,
            &&lbl_OP_MOVE,
            &&lbl_OP_GETGLOBAL,
            &&lbl_OP_SETGLOBAL,
            &&lbl_OP_ADD,
            &&lbl_OP_SUB,
            &&lbl_OP_MUL,
            &&lbl_OP_DIV,
            &&lbl_OP_IDIV,
            &&lbl_OP_MOD,
            &&lbl_OP_POW,
            &&lbl_OP_NEG,
            &&lbl_OP_ADD_OBJ,
            &&lbl_OP_SUB_OBJ,
            &&lbl_OP_MUL_OBJ,
            &&lbl_OP_DIV_OBJ,
            &&lbl_OP_MOD_OBJ,
            &&lbl_OP_NEG_OBJ,
            &&lbl_OP_EQ_OBJ,
            &&lbl_OP_LT_OBJ,
            &&lbl_OP_LE_OBJ,
            &&lbl_OP_ADDI,
            &&lbl_OP_SUBI,
            &&lbl_OP_BAND,
            &&lbl_OP_BOR,
            &&lbl_OP_BXOR,
            &&lbl_OP_BNOT,
            &&lbl_OP_SHL,
            &&lbl_OP_SHR,
            &&lbl_OP_EQ,
            &&lbl_OP_LT,
            &&lbl_OP_LE,
            &&lbl_OP_NOT,
            &&lbl_OP_CONTAINS,
            &&lbl_OP_IS,
            &&lbl_OP_JMP,
            &&lbl_OP_JMPIF,
            &&lbl_OP_JMPIFNOT,
            &&lbl_OP_CALL,
            &&lbl_OP_CALLGLOBAL,
            &&lbl_OP_RETURN,
            &&lbl_OP_CLOSURE,
            &&lbl_OP_GETUPVAL,
            &&lbl_OP_SETUPVAL,
            &&lbl_OP_CLOSE,
            &&lbl_OP_NEWFIBER,
            &&lbl_OP_RESUME,
            &&lbl_OP_YIELD,
            &&lbl_OP_AWAIT,
            &&lbl_OP_FOR_ITER,
            &&lbl_OP_NEWARRAY,
            &&lbl_OP_NEWMAP,
            &&lbl_OP_NEWSET,
            &&lbl_OP_NEWBUFFER,
            &&lbl_OP_APPEND,
            &&lbl_OP_SETADD,
            &&lbl_OP_GETFIELD,
            &&lbl_OP_SETFIELD,
            &&lbl_OP_GETFIELD_IDX,
            &&lbl_OP_SETFIELD_IDX,
            &&lbl_OP_GETINDEX,
            &&lbl_OP_SETINDEX,
            &&lbl_OP_DELINDEX,
            &&lbl_OP_GETSLICE,
            &&lbl_OP_INVOKE,
            &&lbl_OP_INVOKE_VT,
            &&lbl_OP_SUPER_INVOKE,
            &&lbl_OP_NEWCLASS,
            &&lbl_OP_NEWINSTANCE,
            &&lbl_OP_GETMETHOD,
            &&lbl_OP_CLASSFIELD,
            &&lbl_OP_CONCAT,
            &&lbl_OP_STRADD,
            &&lbl_OP_TOSTRING,
            &&lbl_OP_TOSTRING_OBJ,
            &&lbl_OP_LEN,
            &&lbl_OP_PRINT,
            &&lbl_OP_LTJMPIFNOT,
            &&lbl_OP_LEJMPIFNOT,
            &&lbl_OP_FORPREP,
            &&lbl_OP_FORLOOP,
            &&lbl_OP_GETFIELD_MUL,
            &&lbl_OP_GETFIELD_SUB,
            &&lbl_OP_ITER_ELEM,
            &&lbl_OP_EVAL,
            &&lbl_OP_ASSERT,
            &&lbl_OP_HALT,
            &&lbl_OP_IMPORT,
            &&lbl_OP_CLASSFIELDDEF,
        };

#define DISPATCH() goto *dispatch_table[ZEN_OP(*ip)]
#define CASE(op) lbl_##op:
#define NEXT()      \
    do              \
    {               \
        ++ip;       \
        DISPATCH(); \
    } while (0)

        DISPATCH();

#else

/* --- Switch (portável) --- */
#define DISPATCH() continue
#define CASE(op) case op:
#define NEXT()    \
    do            \
    {             \
        ++ip;     \
        continue; \
    } while (0)

        for (;;)
        {

#define SWITCH_TOP       \
    switch (ZEN_OP(*ip)) \
    {
            SWITCH_TOP

#endif

        /* =================================================================
        **  OPCODES
        ** ================================================================= */

        CASE(OP_LOADNIL)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_nil();
            NEXT();
        }

        CASE(OP_LOADBOOL)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_bool(ZEN_B(i) != 0);
            if (ZEN_C(i))
                ++ip; /* skip next if C */
            NEXT();
        }

        CASE(OP_LOADK)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = K[ZEN_BX(i)];
            NEXT();
        }

        CASE(OP_LOADI)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_int(ZEN_SBX(i));
            NEXT();
        }

        CASE(OP_MOVE)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = R[ZEN_B(i)];
            NEXT();
        }

        CASE(OP_GETGLOBAL)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = globals_[ZEN_BX(i)];
            NEXT();
        }

        CASE(OP_SETGLOBAL)
        {
            uint32_t i = *ip;
            Value v = R[ZEN_A(i)];
            if (__builtin_expect(is_string(v), 0))
                v.as.obj->flags |= OBJ_FLAG_SHARED;
            globals_[ZEN_BX(i)] = v;
            NEXT();
        }

        /* --- Aritmética --- */
        CASE(OP_ADD)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (__builtin_expect(!is_obj(vb) && !is_obj(vc), 1))
            {
                NUM_BINOP(+);
            }
            else if (is_string(vb) && is_string(vc))
            {
                ObjString *sb = as_string(vb);
                /* In-place only when A == B (augmented assignment: s += x)
                   AND no intra-frame aliases exist.
                   When A != B the source register must be preserved. */
                if (ZEN_A(i) == ZEN_B(i)) {
                    bool aliased = sb->obj.interned || (sb->obj.flags & OBJ_FLAG_SHARED);
                    if (!aliased) {
                        int b_reg = ZEN_B(i);
                        int nregs = frame->func->num_regs;
                        for (int r = 0; r < nregs; r++) {
                            if (r != b_reg &&
                                is_obj(R[r]) && R[r].as.obj == (Obj *)sb) {
                                aliased = true;
                                break;
                            }
                        }
                    }
                    if (aliased)
                        R[ZEN_A(i)] = val_obj((Obj *)new_string_concat(&gc_, sb, as_string(vc)));
                    else {
#ifndef NDEBUG
                        /* DEBUG: exhaustive alias scan across ALL frames, globals,
                           upvalues, and caller fibers.  Catches any missing shared
                           marking.  Zero cost in release builds. */
                        {
                            Obj *target = (Obj *)sb;
                            auto chk = [&](Value v, const char *where, int idx) {
                                if (is_obj(v) && v.as.obj == target) {
                                    fprintf(stderr,
                                        "STRING SAFETY ASSERT: undetected alias!\n"
                                        "  string='%.*s' found in %s[%d]\n"
                                        "  current frame=%d reg=%d\n",
                                        sb->length, sb->chars, where, idx,
                                        fiber->frame_count - 1, ZEN_A(i));
                                    assert(false && "string_append_inplace: undetected alias");
                                }
                            };
                            /* All frames of current fiber */
                            for (int fi = 0; fi < fiber->frame_count; fi++) {
                                CallFrame &cf = fiber->frames[fi];
                                int nr = cf.func->num_regs;
                                for (int ri = 0; ri < nr; ri++) {
                                    Value *slot = &cf.base[ri];
                                    if (slot == &R[ZEN_A(i)]) continue;
                                    chk(*slot, "frame_reg", fi * 1000 + ri);
                                }
                            }
                            /* Globals */
                            for (int gi = 0; gi < num_globals_; gi++)
                                chk(globals_[gi], "global", gi);
                            /* Open upvalues */
                            for (ObjUpvalue *uv = fiber->open_upvalues; uv; uv = uv->next) {
                                if (uv->location != &R[ZEN_A(i)])
                                    chk(*uv->location, "upvalue", 0);
                            }
                            /* Caller fiber chain */
                            for (ObjFiber *cf = fiber->caller; cf; cf = cf->caller) {
                                for (int fi = 0; fi < cf->frame_count; fi++) {
                                    CallFrame &cfr = cf->frames[fi];
                                    int nr = cfr.func->num_regs;
                                    for (int ri = 0; ri < nr; ri++)
                                        chk(cfr.base[ri], "caller_fiber_reg", fi * 1000 + ri);
                                }
                                chk(cf->transfer_value, "caller_transfer", 0);
                            }
                        }
#endif
                        R[ZEN_A(i)] = val_obj((Obj *)string_append_inplace(&gc_, sb, as_string(vc)));
                    }
                } else {
                    R[ZEN_A(i)] = val_obj((Obj *)new_string_concat(&gc_, sb, as_string(vc)));
                }
            }
            else if (is_instance(vb) || is_instance(vc))
            {
                Value result;
                SAVE_IP();
                if (try_binary_operator(this, vb, vc, SLOT_ADD, SLOT_RADD, &result))
                {
                    if (had_error_) return;
                    LOAD_STATE();
                    R[ZEN_A(i)] = result;
                }
                else
                {
                    LOAD_STATE();
                    Value sv = vb, sc = vc;
                    if (!is_string(sv))
                    {
                        Value str_result;
                        if (try_string_operator(this, sv, &str_result))
                            sv = str_result;
                        else
                            sv = default_to_string(&gc_, sv);
                        if (had_error_) return;
                    }
                    if (!is_string(sc))
                    {
                        Value str_result;
                        if (try_string_operator(this, sc, &str_result))
                            sc = str_result;
                        else
                            sc = default_to_string(&gc_, sc);
                        if (had_error_) return;
                    }
                    LOAD_STATE();
                    R[ZEN_A(i)] = val_obj((Obj *)new_string_concat(&gc_, as_string(sv), as_string(sc)));
                }
            }
            else if (is_string(vb) || is_string(vc))
            {
                char buf[64], buf2[64];
                const char *sa;
                int la;
                const char *sb;
                int lb;
                if (is_string(vb))
                {
                    sa = safe_string_chars(vb);
                    la = safe_string_len(vb);
                }
                else if (is_int(vb))
                {
                    la = int_to_cstr(vb.as.integer, buf);
                    sa = buf;
                }
                else if (is_float(vb))
                {
                    la = snprintf(buf, sizeof(buf), "%g", vb.as.number);
                    sa = buf;
                }
                else if (is_bool(vb))
                {
                    sa = vb.as.boolean ? "true" : "false";
                    la = vb.as.boolean ? 4 : 5;
                }
                else
                {
                    sa = "nil";
                    la = 3;
                }

                if (is_string(vc))
                {
                    sb = safe_string_chars(vc);
                    lb = safe_string_len(vc);
                }
                else if (is_int(vc))
                {
                    lb = int_to_cstr(vc.as.integer, buf2);
                    sb = buf2;
                }
                else if (is_float(vc))
                {
                    lb = snprintf(buf2, sizeof(buf2), "%g", vc.as.number);
                    sb = buf2;
                }
                else if (is_bool(vc))
                {
                    sb = vc.as.boolean ? "true" : "false";
                    lb = vc.as.boolean ? 4 : 5;
                }
                else
                {
                    sb = "nil";
                    lb = 3;
                }

                /* Fix: new_string_uninit — zero buffer intermédio */
                ObjString *result = new_string_uninit(&gc_, la + lb);
                memcpy(result->chars, sa, la);
                memcpy(result->chars + la, sb, lb);
                result->obj.hash = hash_string(result->chars, la + lb);
                R[ZEN_A(i)] = val_obj((Obj *)result);
            }
            else
            {
                NUM_BINOP(+);
            }
            NEXT();
        }
        CASE(OP_SUB)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (__builtin_expect(!is_obj(vb) && !is_obj(vc), 1))
            {
                NUM_BINOP(-);
            }
            else if (is_instance(vb) || is_instance(vc))
            {
                Value result;
                SAVE_IP();
                if (try_binary_operator(this, vb, vc, SLOT_SUB, SLOT_RSUB, &result))
                {
                    if (had_error_)
                        return;
                    LOAD_STATE();
                    R[ZEN_A(i)] = result;
                }
                else
                {
                    LOAD_STATE();
                    NUM_BINOP(-);
                }
            }
            else
            {
                NUM_BINOP(-);
            }
            NEXT();
        }
        CASE(OP_MUL)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (__builtin_expect(!is_obj(vb) && !is_obj(vc), 1))
            {
                NUM_BINOP(*);
            }
            else if ((is_string(vb) && is_int(vc)) || (is_int(vb) && is_string(vc)))
            {
                /* String repetition: "abc" * 3 or 3 * "abc" */
                ObjString *s = is_string(vb) ? as_string(vb) : as_string(vc);
                int64_t n = is_int(vb) ? vb.as.integer : vc.as.integer;
                if (n <= 0)
                {
                    R[ZEN_A(i)] = val_obj((Obj *)new_string(&gc_, "", 0));
                }
                else
                {
                    int64_t total64 = (int64_t)s->length * n;
                    if (total64 > 0x7FFFFFFF || total64 < 0)
                    {
                        RT_ERROR("string repetition too large");
                    }
                    int total = (int)total64;
                    ObjString *result = new_string_uninit(&gc_, total);
                    char *dst = result->chars;
                    for (int64_t ri = 0; ri < n; ri++, dst += s->length)
                        memcpy(dst, s->chars, s->length);
                    result->obj.hash = hash_string(result->chars, total);
                    R[ZEN_A(i)] = val_obj((Obj *)result);
                }
            }
            else if (is_instance(vb) || is_instance(vc))
            {
                Value result;
                SAVE_IP();
                if (try_binary_operator(this, vb, vc, SLOT_MUL, SLOT_RMUL, &result))
                {
                    if (had_error_)
                        return;
                    LOAD_STATE();
                    R[ZEN_A(i)] = result;
                }
                else
                {
                    LOAD_STATE();
                    NUM_BINOP(*);
                }
            }
            else
            {
                NUM_BINOP(*);
            }
            NEXT();
        }
        CASE(OP_DIV)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (__builtin_expect(!is_obj(vb) && !is_obj(vc), 1))
            {
                double divisor = to_number(vc);
                if (divisor == 0.0)
                {
                    RT_ERROR("division by zero");
                }
                R[ZEN_A(i)] = val_float(to_number(vb) / divisor);
            }
            else if (is_instance(vb) || is_instance(vc))
            {
                Value result;
                SAVE_IP();
                if (try_binary_operator(this, vb, vc, SLOT_DIV, SLOT_RDIV, &result))
                {
                    if (had_error_)
                        return;
                    LOAD_STATE();
                    R[ZEN_A(i)] = result;
                }
                else
                {
                    LOAD_STATE();
                    double divisor = to_number(vc);
                    if (divisor == 0.0)
                    {
                        RT_ERROR("division by zero");
                    }
                    R[ZEN_A(i)] = val_float(to_number(vb) / divisor);
                }
            }
            else
            {
                double divisor = to_number(vc);
                if (divisor == 0.0)
                {
                    RT_ERROR("division by zero");
                }
                R[ZEN_A(i)] = val_float(to_number(vb) / divisor);
            }
            NEXT();
        }
        CASE(OP_IDIV)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (__builtin_expect(!is_obj(vb) && !is_obj(vc), 1))
            {
                if (vb.type == VAL_INT && vc.type == VAL_INT)
                {
                    int64_t b = vb.as.integer, c = vc.as.integer;
                    if (c == 0)
                    {
                        RT_ERROR("floor division by zero");
                    }
                    int64_t q = b / c;
                    if ((b ^ c) < 0 && q * c != b)
                        q--; /* floor toward -inf */
                    R[ZEN_A(i)] = val_int((int32_t)q);
                }
                else
                {
                    double a = to_number(vb), b = to_number(vc);
                    if (b == 0.0)
                    {
                        RT_ERROR("floor division by zero");
                    }
                    R[ZEN_A(i)] = val_float(std::floor(a / b));
                }
            }
            else
            {
                double a = to_number(vb), b = to_number(vc);
                if (b == 0.0)
                {
                    RT_ERROR("floor division by zero");
                }
                R[ZEN_A(i)] = val_float(std::floor(a / b));
            }
            NEXT();
        }
        CASE(OP_MOD)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (__builtin_expect(!is_obj(vb) && !is_obj(vc), 1))
            {
                if (vb.type == VAL_INT && vc.type == VAL_INT)
                {
                    int32_t divisor = vc.as.integer;
                    if (divisor == 0)
                        RT_ERROR("modulo by zero");
                    else
                    {

                        int64_t r = vb.as.integer % (int64_t)divisor;
                        R[ZEN_A(i)] = val_int(r != 0 && (r ^ (int64_t)divisor) < 0 ? r + divisor : r);
                    }
                }
                else
                {
                    double a = to_number(vb), b = to_number(vc);
                    if (b == 0.0)
                        RT_ERROR("modulo by zero");
                    else
                        R[ZEN_A(i)] = val_float(a - (int64_t)(a / b) * b);
                }
            }
            else if (is_instance(vb) || is_instance(vc))
            {
                Value result;
                SAVE_IP();
                if (try_binary_operator(this, vb, vc, SLOT_MOD, SLOT_RMOD, &result))
                {
                    if (had_error_)
                        return;
                    LOAD_STATE();
                    R[ZEN_A(i)] = result;
                }
                else
                {
                    LOAD_STATE();
                    double a = to_number(vb), b = to_number(vc);
                    if (b == 0.0)
                        RT_ERROR("modulo by zero");
                    else
                        R[ZEN_A(i)] = val_float(a - (int64_t)(a / b) * b);
                }
            }
            else
            {
                double a = to_number(vb), b = to_number(vc);
                if (b == 0.0)
                    RT_ERROR("modulo by zero");
                else
                    R[ZEN_A(i)] = val_float(a - (int64_t)(a / b) * b);
            }
            NEXT();
        }
        CASE(OP_POW)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            /* Integer power: if both are int and exponent >= 0, compute int result */
            if (vb.type == VAL_INT && vc.type == VAL_INT && vc.as.integer >= 0)
            {
                int64_t base = vb.as.integer;
                int64_t exp = vc.as.integer;
                int64_t result = 1;
                bool overflow = false;
                for (int64_t e = 0; e < exp; e++)
                {
                    /* Check for overflow before multiply */
                    if (base != 0 && (result > INT64_MAX / (base > 0 ? base : -base)))
                    {
                        overflow = true;
                        break;
                    }
                    result *= base;
                }
                if (!overflow)
                {
                    R[ZEN_A(i)] = val_int(result);
                    NEXT();
                }
            }
            R[ZEN_A(i)] = val_float(std::pow(to_number(vb), to_number(vc)));
            NEXT();
        }
        CASE(OP_NEG)
        {
            uint32_t i = *ip;
            Value v = R[ZEN_B(i)];
            if (__builtin_expect(!is_obj(v), 1))
            {
                if (v.type == VAL_INT)
                    R[ZEN_A(i)] = val_int((int64_t)(-(uint64_t)v.as.integer));
                else
                    R[ZEN_A(i)] = val_float(-to_number(v));
            }
            else if (is_instance(v))
            {
                Value result;
                SAVE_IP();
                if (try_unary_operator(this, v, SLOT_NEG, &result))
                {
                    if (had_error_)
                        return;
                    LOAD_STATE();
                    R[ZEN_A(i)] = result;
                }
                else
                {
                    LOAD_STATE();
                    R[ZEN_A(i)] = val_float(-to_number(v));
                }
            }
            else
            {
                R[ZEN_A(i)] = val_float(-to_number(v));
            }
            NEXT();
        }

        CASE(OP_ADD_OBJ)
        {
            uint32_t i = *ip;
            uint8_t dst = ZEN_A(i);
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            Value result;
            SAVE_IP();
            if (try_binary_operator(this, vb, vc, SLOT_ADD, SLOT_RADD, &result))
            {
                if (had_error_)
                    return;
                LOAD_STATE();
                R[dst] = result;
                NEXT();
            }
            /* Fallback: string concat with __str__ coercion for instances */
            if (is_string(vb) || is_string(vc) || is_instance(vb) || is_instance(vc))
            {
                /* Coerce both sides to string, calling __str__ on instances */
                Value sv = vb, sc = vc;
                if (!is_string(sv))
                {
                    Value str_result;
                    if (try_string_operator(this, sv, &str_result))
                        sv = str_result;
                    else
                        sv = default_to_string(&gc_, sv);
                    if (had_error_)
                        return;
                }
                if (!is_string(sc))
                {
                    Value str_result;
                    if (try_string_operator(this, sc, &str_result))
                        sc = str_result;
                    else
                        sc = default_to_string(&gc_, sc);
                    if (had_error_)
                        return;
                }
                LOAD_STATE();
                R[dst] = val_obj((Obj *)new_string_concat(&gc_, as_string(sv), as_string(sc)));
            }
            else
            {
                LOAD_STATE();
                NUM_BINOP(+);
            }
            NEXT();
        }
        CASE(OP_SUB_OBJ)
        {
            uint32_t i = *ip;
            uint8_t dst = ZEN_A(i);
            Value result;
            SAVE_IP();
            if (try_binary_operator(this, R[ZEN_B(i)], R[ZEN_C(i)], SLOT_SUB, SLOT_RSUB, &result))
            {
                if (had_error_)
                    return;
                LOAD_STATE();
                R[dst] = result;
                NEXT();
            }
            LOAD_STATE();
            NUM_BINOP(-);
            NEXT();
        }
        CASE(OP_MUL_OBJ)
        {
            uint32_t i = *ip;
            uint8_t dst = ZEN_A(i);
            Value result;
            SAVE_IP();
            if (try_binary_operator(this, R[ZEN_B(i)], R[ZEN_C(i)], SLOT_MUL, SLOT_RMUL, &result))
            {
                if (had_error_)
                    return;
                LOAD_STATE();
                R[dst] = result;
                NEXT();
            }
            LOAD_STATE();
            NUM_BINOP(*);
            NEXT();
        }
        CASE(OP_DIV_OBJ)
        {
            uint32_t i = *ip;
            uint8_t dst = ZEN_A(i);
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            Value result;
            SAVE_IP();
            if (try_binary_operator(this, vb, vc, SLOT_DIV, SLOT_RDIV, &result))
            {
                if (had_error_)
                    return;
                LOAD_STATE();
                R[dst] = result;
                NEXT();
            }
            LOAD_STATE();
            double divisor = to_number(vc);
            if (divisor == 0.0)
            {
                RT_ERROR("division by zero");
            }
            R[dst] = val_float(to_number(vb) / divisor);
            NEXT();
        }
        CASE(OP_MOD_OBJ)
        {
            uint32_t i = *ip;
            uint8_t dst = ZEN_A(i);
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            Value result;
            SAVE_IP();
            if (try_binary_operator(this, vb, vc, SLOT_MOD, SLOT_RMOD, &result))
            {
                if (had_error_)
                    return;
                LOAD_STATE();
                R[dst] = result;
                NEXT();
            }
            LOAD_STATE();
            if (vb.type == VAL_INT && vc.type == VAL_INT)
            {
                int32_t divisor = vc.as.integer;
                if (divisor == 0)
                    RT_ERROR("modulo by zero");
                else
                    R[dst] = val_int(vb.as.integer % divisor);
            }
            else
            {
                double a = to_number(vb), b = to_number(vc);
                if (b == 0.0)
                    RT_ERROR("modulo by zero");
                else
                    R[dst] = val_float(a - (int64_t)(a / b) * b);
            }
            NEXT();
        }
        CASE(OP_NEG_OBJ)
        {
            uint32_t i = *ip;
            uint8_t dst = ZEN_A(i);
            Value result;
            SAVE_IP();
            if (!try_unary_operator(this, R[ZEN_B(i)], SLOT_NEG, &result))
            {
                LOAD_STATE();
                RT_ERROR("object does not implement unary operator -");
            }
            if (had_error_)
                return;
            LOAD_STATE();
            R[dst] = result;
            NEXT();
        }
        CASE(OP_EQ_OBJ)
        {
            uint32_t i = *ip;
            uint8_t dst = ZEN_A(i);
            Value result;
            SAVE_IP();
            if (!try_binary_operator(this, R[ZEN_B(i)], R[ZEN_C(i)], SLOT_EQ, -1, &result))
            {
                LOAD_STATE();
                R[dst] = val_bool(values_deep_equal(R[ZEN_B(i)], R[ZEN_C(i)]));
                NEXT();
            }
            if (had_error_)
                return;
            LOAD_STATE();
            R[dst] = val_bool(is_truthy_full(result));
            NEXT();
        }
        CASE(OP_LT_OBJ)
        {
            uint32_t i = *ip;
            uint8_t dst = ZEN_A(i);
            Value result;
            SAVE_IP();
            if (!try_binary_operator(this, R[ZEN_B(i)], R[ZEN_C(i)], SLOT_LT, -1, &result))
            {
                LOAD_STATE();
                RT_ERROR("object does not implement operator <");
            }
            if (had_error_)
                return;
            LOAD_STATE();
            R[dst] = val_bool(is_truthy_full(result));
            NEXT();
        }
        CASE(OP_LE_OBJ)
        {
            uint32_t i = *ip;
            uint8_t dst = ZEN_A(i);
            Value result;
            SAVE_IP();
            if (!try_binary_operator(this, R[ZEN_B(i)], R[ZEN_C(i)], SLOT_LE, -1, &result))
            {
                LOAD_STATE();
                RT_ERROR("object does not implement operator <=");
            }
            if (had_error_)
                return;
            LOAD_STATE();
            R[dst] = val_bool(is_truthy_full(result));
            NEXT();
        }

        /* --- Superinstructions (immediate) --- */
        CASE(OP_ADDI)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)];
            int8_t imm = (int8_t)ZEN_C(i);
            if (vb.type == VAL_INT)
                R[ZEN_A(i)] = val_int((int64_t)((uint64_t)vb.as.integer + (int64_t)imm));
            else
                R[ZEN_A(i)] = val_float(to_number(vb) + imm);
            NEXT();
        }
        CASE(OP_SUBI)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)];
            int8_t imm = (int8_t)ZEN_C(i);
            if (vb.type == VAL_INT)
                R[ZEN_A(i)] = val_int((int64_t)((uint64_t)vb.as.integer - (int64_t)imm));
            else
                R[ZEN_A(i)] = val_float(to_number(vb) - imm);
            NEXT();
        }

        /* --- Bitwise --- */
        CASE(OP_BAND)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_int(to_integer(R[ZEN_B(i)]) & to_integer(R[ZEN_C(i)]));
            NEXT();
        }
        CASE(OP_BOR)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_int(to_integer(R[ZEN_B(i)]) | to_integer(R[ZEN_C(i)]));
            NEXT();
        }
        CASE(OP_BXOR)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_int(to_integer(R[ZEN_B(i)]) ^ to_integer(R[ZEN_C(i)]));
            NEXT();
        }
        CASE(OP_BNOT)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_int(~to_integer(R[ZEN_B(i)]));
            NEXT();
        }
        CASE(OP_SHL)
        {
            uint32_t i = *ip;
            int64_t val = to_integer(R[ZEN_B(i)]);
            int64_t raw = to_integer(R[ZEN_C(i)]);
            if (raw < 0 || raw >= 64)
            {
                RT_ERROR("shift amount out of range (%lld)", (long long)raw);
            }
            R[ZEN_A(i)] = val_int((int64_t)((uint64_t)val << (int)raw));
            NEXT();
        }
        CASE(OP_SHR)
        {
            uint32_t i = *ip;
            int64_t val = to_integer(R[ZEN_B(i)]);
            int64_t raw = to_integer(R[ZEN_C(i)]);
            if (raw < 0 || raw >= 64)
            {
                RT_ERROR("shift amount out of range (%lld)", (long long)raw);
            }
            int shift = (int)raw;
            /* Arithmetic right shift (sign-preserving) — portable implementation */
            R[ZEN_A(i)] = val_int((int64_t)(((uint64_t)val >> shift) | (val < 0 ? ~(~(uint64_t)0 >> shift) : 0)));
            NEXT();
        }

        /* --- Comparação --- */
        CASE(OP_EQ)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (__builtin_expect(is_instance(vb) || is_instance(vc), 0))
            {
                Value result;
                SAVE_IP();
                if (try_binary_operator(this, vb, vc, SLOT_EQ, SLOT_EQ, &result))
                {
                    if (had_error_)
                        return;
                    LOAD_STATE();
                    R[ZEN_A(i)] = val_bool(is_truthy_full(result));
                }
                else
                {
                    LOAD_STATE();
                    R[ZEN_A(i)] = val_bool(values_deep_equal(vb, vc));
                }
            }
            else
            {
                R[ZEN_A(i)] = val_bool(values_deep_equal(vb, vc));
            }
            NEXT();
        }
        CASE(OP_LT)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (__builtin_expect(is_instance(vb) || is_instance(vc), 0))
            {
                Value result;
                SAVE_IP();
                if (try_binary_operator(this, vb, vc, SLOT_LT, SLOT_LT, &result))
                {
                    if (had_error_)
                        return;
                    LOAD_STATE();
                    R[ZEN_A(i)] = val_bool(is_truthy_full(result));
                }
                else
                {
                    LOAD_STATE();
                    R[ZEN_A(i)] = val_bool(to_number(vb) < to_number(vc));
                }
            }
            else if (is_string(vb) && is_string(vc))
                R[ZEN_A(i)] = val_bool(strcmp(safe_string_chars(vb), safe_string_chars(vc)) < 0);
            else if (vb.type == VAL_INT && vc.type == VAL_INT)
                R[ZEN_A(i)] = val_bool(vb.as.integer < vc.as.integer);
            else
                R[ZEN_A(i)] = val_bool(to_number(vb) < to_number(vc));
            NEXT();
        }
        CASE(OP_LE)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            if (__builtin_expect(is_instance(vb) || is_instance(vc), 0))
            {
                Value result;
                SAVE_IP();
                if (try_binary_operator(this, vb, vc, SLOT_LE, SLOT_LE, &result))
                {
                    if (had_error_)
                        return;
                    LOAD_STATE();
                    R[ZEN_A(i)] = val_bool(is_truthy_full(result));
                }
                else
                {
                    LOAD_STATE();
                    R[ZEN_A(i)] = val_bool(to_number(vb) <= to_number(vc));
                }
            }
            else if (is_string(vb) && is_string(vc))
                R[ZEN_A(i)] = val_bool(strcmp(safe_string_chars(vb), safe_string_chars(vc)) <= 0);
            else if (vb.type == VAL_INT && vc.type == VAL_INT)
                R[ZEN_A(i)] = val_bool(vb.as.integer <= vc.as.integer);
            else
                R[ZEN_A(i)] = val_bool(to_number(vb) <= to_number(vc));
            NEXT();
        }
        CASE(OP_NOT)
        {
            uint32_t i = *ip;
            R[ZEN_A(i)] = val_bool(!is_truthy_full(R[ZEN_B(i)]));
            NEXT();
        }

        /* R[A] = (R[B] in R[C]) — containment check */
        CASE(OP_CONTAINS)
        {
            uint32_t i = *ip;
            ++ip;
            Value needle = R[ZEN_B(i)];
            Value haystack = R[ZEN_C(i)];
            bool found = false;

            if (is_array(haystack))
            {
                found = array_find(as_array(haystack), needle) >= 0;
            }
            else if (is_set(haystack))
            {
                found = set_contains(as_set(haystack), needle);
            }
            else if (is_map(haystack))
            {
                bool ok;
                map_get(as_map(haystack), needle, &ok);
                found = ok;
            }
            else if (is_string(haystack))
            {
                if (!is_string(needle))
                {
                    RT_ERROR("'in <string>' requires string as left operand");
                }
                ObjString *h = as_string(haystack);
                ObjString *n = as_string(needle);
                if (n->length == 0)
                    found = true;
                else if (n->length <= h->length)
                    found = strstr(h->chars, n->chars) != nullptr;
            }
            else
            {
                RT_ERROR("argument of type '%s' is not iterable", val_type_str(haystack));
            }

            R[ZEN_A(i)] = val_bool(found);
            DISPATCH();
        }

        /* --- Identity check (pointer comparison) --- */
        CASE(OP_IS)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            
            /* Identity check: true only if same type AND same bits */
            bool is_identical = false;
            
            if (vb.type == vc.type)
            {
                switch (vb.type)
                {
                case VAL_NIL:
                    /* Two nils are always identical */
                    is_identical = true;
                    break;
                case VAL_BOOL:
                    /* Two bools with same value are identical */
                    is_identical = vb.as.boolean == vc.as.boolean;
                    break;
                case VAL_INT:
                    /* Two ints with same value are identical */
                    is_identical = vb.as.integer == vc.as.integer;
                    break;
                case VAL_FLOAT:
                    /* Two floats with same value are identical (strict equality) */
                    is_identical = vb.as.number == vc.as.number;
                    break;
                case VAL_OBJ:
                    /* Objects: true only if same pointer (reference equality) */
                    is_identical = (vb.as.obj == vc.as.obj);
                    break;
                default:
                    is_identical = false;
                    break;
                }
            }
            
            R[ZEN_A(i)] = val_bool(is_identical);
            NEXT();
        }

        /* --- Jumps --- */
        CASE(OP_JMP)
        {
            uint32_t i = *ip;
            ip += ZEN_SBX(i);
            NEXT();
        }
        CASE(OP_JMPIF)
        {
            uint32_t i = *ip;
            if (is_truthy_full(R[ZEN_A(i)]))
                ip += ZEN_SBX(i);
            NEXT();
        }
        CASE(OP_JMPIFNOT)
        {
            uint32_t i = *ip;
            if (!is_truthy_full(R[ZEN_A(i)]))
                ip += ZEN_SBX(i);
            NEXT();
        }

        /* --- Funções --- */
        CASE(OP_CALL)
        {
            uint32_t i = *ip;
            int a = ZEN_A(i);
            int nargs = ZEN_B(i);
            int nresults = ZEN_C(i);
            ++ip;
            SAVE_IP();

            /* Spread expansion: bit 7 of nargs means last arg is an array to unpack */
            if (nargs & 0x80)
            {
                int fixed = (nargs & 0x7F) - 1; /* fixed args before the spread */
                Value spread_val = R[a + 1 + fixed];
                if (!is_array(spread_val))
                {
                    RT_ERROR("argument unpacking requires a list");
                }
                ObjArray *arr = as_array(spread_val);
                int arr_len = arr_count(arr);
                /* Shift spread elements into place */
                for (int si = 0; si < arr_len; si++)
                    R[a + 1 + fixed + si] = arr->data[si];
                nargs = fixed + arr_len;
            }

            Value callee = R[a];
            /* Mark string args as shared — protects against in-place
               mutation in the callee corrupting the caller's copies. */
            for (int ai = 0; ai < nargs; ai++) {
                Value av = R[a + 1 + ai];
                if (__builtin_expect(is_string(av), 0))
                    av.as.obj->flags |= OBJ_FLAG_SHARED;
            }
            if (is_closure(callee))
            {
                /* Script closure — hot path */
                ObjClosure *cl = as_closure(callee);
                ObjFunc *fn = cl->func;

                /* Generator function: create a suspended fiber instead of calling */
                if (fn->is_generator)
                {
                    int gen_stack = fn->num_regs < 256 ? 256 : fn->num_regs;
                    ObjFiber *gen = new_fiber(cl, gen_stack);
                    /* Copy arguments into the fiber's first frame base */
                    CallFrame *gf = &gen->frames[0];
                    for (int ai = 0; ai < nargs; ai++)
                        gf->base[ai] = R[a + 1 + ai];
                    gen->state = FIBER_SUSPENDED;
                    R[a] = val_obj((Obj *)gen);
                    DISPATCH();
                }

                if (fiber->frame_count >= kMaxFrames)
                {
                    RT_ERROR("stack overflow");
                }
                CHECK_STACK_SPACE(fiber, &R[a + 1], fn->num_regs);
                CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
                new_frame->closure = cl;
                new_frame->func = fn;
                new_frame->ip = fn->code;
                new_frame->base = &R[a + 1];
                new_frame->ret_reg = a;
                new_frame->ret_count = nresults;

                /* Vararg packing: def f(*args) -> arity<0 */
                if (fn->arity < 0)
                {
                    int min_args = (-fn->arity) - 1;
                    if (nargs < min_args)
                        RT_ERROR("expected at least %d args but got %d", min_args, nargs);
                    int extra = nargs - min_args;
                    gc_pause(&gc_);
                    ObjArray *arr = new_array(&gc_);
                    if (extra > 0)
                        array_push_n(&gc_, arr, new_frame->base + min_args, extra);
                    new_frame->base[min_args] = val_obj((Obj *)arr);
                    gc_resume(&gc_);
                }
                else
                {
                    /* Apply defaults for missing trailing args */
                    int required = fn->arity - fn->default_count;
                    if (nargs < required)
                        RT_ERROR("expected at least %d args but got %d", required, nargs);
                    if (nargs > fn->arity)
                        RT_ERROR("expected at most %d args but got %d", fn->arity, nargs);
                    /* Extend stack first so writes are within bounds */
                    fiber->stack_top = new_frame->base + fn->num_regs;
                    for (int di = nargs; di < fn->arity; di++)
                        new_frame->base[di] = fn->defaults[di - required];
                }

                fiber->stack_top = new_frame->base + fn->num_regs;
                {
                    int used = fn->arity < 0 ? ((-fn->arity - 1) + 1) : fn->arity;
                    clear_new_regs(new_frame->base, used, fn->num_regs);
                }
                LOAD_STATE();
                DISPATCH();
            }
            if (is_native(callee))
            {
                ObjNative *nat = as_native(callee);
                int nret = call_native(this, nat->fn, &R[a + 1], nargs);
                if (had_error_)
                    return;
                copy_native_results(&R[a], &R[a + 1], nret, nresults);
                DISPATCH();
            }

            if (is_class(callee))
            {
                /* Class call → create instance + call init */
                ObjClass *klass = as_class(callee);

                /* Check if script is allowed to instantiate this class */
                if (!klass->constructable)
                {
                    RT_ERROR("class '%s' cannot be instantiated from script", klass->name->chars);
                }

                ObjInstance *inst = new_instance(&gc_, klass);
                R[a] = val_obj((Obj *)inst);

                /* Call native constructor if the class (or parent) has one */
                ObjClass *ctor_src = klass;
                while (ctor_src && !ctor_src->native_ctor)
                    ctor_src = ctor_src->parent;
                if (ctor_src && ctor_src->native_ctor)
                {
                    inst->native_data = ctor_src->native_ctor(this, nargs, &R[a + 1]);
                }

                /* Look for __init__ method */
                ObjString *s_init = intern_string(&gc_, "__init__", 8, hash_string("__init__", 8));

                bool found;
                Value init_method = map_get(klass->methods, val_obj((Obj *)s_init), &found);
                if (!found && klass->parent)
                    init_method = map_get(klass->parent->methods, val_obj((Obj *)s_init), &found);

                if (found && is_closure(init_method))
                {
                    ObjClosure *cl = as_closure(init_method);
                    ObjFunc *fn = cl->func;
                    /* Check arity (init's arity = user params, self is implicit) */
                    if (fn->arity < 0)
                    {
                        int min_args = (-fn->arity) - 1;
                        if (nargs < min_args)
                            RT_ERROR("init() expects at least %d args but got %d", min_args, nargs);
                    }
                    else if (fn->default_count > 0)
                    {
                        int required = fn->arity - fn->default_count;
                        if (nargs < required)
                            RT_ERROR("init() expects at least %d args but got %d", required, nargs);
                        if (nargs > fn->arity)
                            RT_ERROR("init() expects at most %d args but got %d", fn->arity, nargs);
                    }
                    else if (nargs != fn->arity)
                    {
                        RT_ERROR("init() expects %d args but got %d", fn->arity, nargs);
                    }
                    /* Set up frame: R[a] = instance (self), args at R[a+1..] */
                    if (fiber->frame_count >= kMaxFrames)
                    {
                        RT_ERROR("stack overflow");
                    }
                    CHECK_STACK_SPACE(fiber, &R[a], fn->num_regs);
                    CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
                    new_frame->closure = cl;
                    new_frame->func = fn;
                    new_frame->ip = fn->code;
                    new_frame->base = &R[a]; /* self at base[0], args at base[1..] */
                    new_frame->ret_reg = a;
                    new_frame->ret_count = nresults;
                    fiber->stack_top = new_frame->base + fn->num_regs;

                    /* Fill in default values for missing args */
                    if (fn->arity >= 0 && fn->default_count > 0 && nargs < fn->arity)
                    {
                        int required = fn->arity - fn->default_count;
                        for (int di = nargs; di < fn->arity; di++)
                            new_frame->base[1 + di] = fn->defaults[di - required];
                    }

                    /* Clear unused regs: self(1) + arity args */
                    {
                        int used = 1 + (fn->arity >= 0 ? fn->arity : nargs);
                        clear_new_regs(new_frame->base, used, fn->num_regs);
                    }

                    LOAD_STATE();
                    DISPATCH();
                }
                else if (found && is_native(init_method))
                {
                    /* Native __init__ — call it directly with self at R[a] */
                    ObjNative *nat = as_native(init_method);
                    call_native(this, nat->fn, &R[a + 1], nargs);
                    if (had_error_)
                        return;
                    /* Result is still R[a] = the instance */
                    DISPATCH();
                }
                else if (nargs > 0 && !ctor_src)
                {
                    /* Error only if no init AND no native_ctor handled the args */
                    RT_ERROR("class '%s' has no init() but received %d args", klass->name->chars, nargs);
                }
                /* No init and no args (or native_ctor consumed them) — just return the instance */
                DISPATCH();
            }
            if (is_struct_def(callee))
            {
                ObjStructDef *def = as_struct_def(callee);
                if (nargs != def->num_fields)
                    RT_ERROR("struct '%s' expects %d args but got %d",
                             def->name->chars, def->num_fields, nargs);
                ObjStruct *s = (ObjStruct *)zen_alloc_now(&gc_, sizeof(ObjStruct));
                s->obj.type = OBJ_STRUCT;
                s->obj.color = GC_WHITE;
                s->obj.interned = 0;
                s->obj.flags = 0;
                s->obj.hash = 0;
                s->obj.gc_next = gc_.objects;
                gc_.objects = (Obj *)s;
                s->def = def;
                s->fields = (Value *)zen_alloc_now(&gc_, sizeof(Value) * def->num_fields);
                for (int fi = 0; fi < def->num_fields; fi++)
                    s->fields[fi] = R[a + 1 + fi];
                R[a] = val_obj((Obj *)s);
                DISPATCH();
            }
            if (is_native_struct_def(callee))
            {
                NativeStructDef *def = as_native_struct_def(callee);
                ObjNativeStruct *ns = (ObjNativeStruct *)zen_alloc_now(&gc_, sizeof(ObjNativeStruct));
                ns->obj.type = OBJ_NATIVE_STRUCT;
                ns->obj.color = GC_WHITE;
                ns->obj.hash = 0;
                ns->obj.interned = 0;
                ns->obj.flags = 0;
                ns->obj.gc_next = gc_.objects;
                gc_.objects = (Obj *)ns;
                ns->def = def;
                ns->data = zen_alloc_now(&gc_, def->struct_size);
                memset(ns->data, 0, def->struct_size);
                if (def->ctor)
                    def->ctor(this, ns->data, nargs, &R[a + 1]);
                R[a] = val_obj((Obj *)ns);
                DISPATCH();
            }
            RT_ERROR("attempt to call non-function (got %s)", val_type_str(R[a]));
        }

        CASE(OP_CALLGLOBAL)
        {
            uint32_t i = *ip;
            int a = ZEN_A(i);
            int nargs = ZEN_B(i);
            int nresults = ZEN_C(i);
            ++ip;
            int gidx = ZEN_BX(*ip); /* word 2: global index */
            ++ip;
            SAVE_IP();

            /* Spread expansion */
            if (nargs & 0x80)
            {
                int fixed = (nargs & 0x7F) - 1;
                Value spread_val = R[a + 1 + fixed];
                if (!is_array(spread_val))
                {
                    RT_ERROR("argument unpacking requires a list");
                }
                ObjArray *arr = as_array(spread_val);
                int arr_len = arr_count(arr);
                for (int si = 0; si < arr_len; si++)
                    R[a + 1 + fixed + si] = arr->data[si];
                nargs = fixed + arr_len;
            }

            Value callee = globals_[gidx];

            /* Mark string args as shared — matches OP_CALL behaviour */
            for (int ai = 0; ai < nargs; ai++) {
                Value av = R[a + 1 + ai];
                if (__builtin_expect(is_string(av), 0))
                    av.as.obj->flags |= OBJ_FLAG_SHARED;
            }

            if (is_closure(callee))
            {
                ObjClosure *cl = as_closure(callee);
                ObjFunc *fn = cl->func;
                if (fiber->frame_count >= kMaxFrames)
                {
                    RT_ERROR("stack overflow");
                }
                CHECK_STACK_SPACE(fiber, &R[a + 1], fn->num_regs);
                CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
                new_frame->closure = cl;
                new_frame->func = fn;
                new_frame->ip = fn->code;
                new_frame->base = &R[a + 1];
                new_frame->ret_reg = a;
                new_frame->ret_count = nresults;
                fiber->stack_top = new_frame->base + fn->num_regs;
                clear_new_regs(new_frame->base, nargs, fn->num_regs);
                LOAD_STATE();
                DISPATCH();
            }
            if (is_native(callee))
            {
                ObjNative *nat = as_native(callee);
                int nret = call_native(this, nat->fn, &R[a + 1], nargs);
                if (had_error_)
                    return;
                copy_native_results(&R[a], &R[a + 1], nret, nresults);
                DISPATCH();
            }
            RT_ERROR("attempt to call non-function (got %s)", val_type_str(callee));
        }

        CASE(OP_RETURN)
        {
            uint32_t i = *ip;
            int a = ZEN_A(i);
            int nresults = ZEN_B(i);

            /* Close upvalues do frame actual — skip se não há nenhum aberto */
            if (fiber->open_upvalues && fiber->open_upvalues->location >= frame->base)
                close_upvalues(fiber, frame->base);

            /* Copiar resultados para o caller */
            int ret_reg = frame->ret_reg;
            int ret_count = frame->ret_count;

            fiber->frame_count--;
            if (fiber->frame_count == 0)
            {
                /* Retorno do top-level */
                fiber->state = FIBER_DONE;
                if (fiber->caller)
                {
                    Value rv = nresults > 0 ? R[a] : val_nil();
                    if (__builtin_expect(is_string(rv), 0))
                        rv.as.obj->flags |= OBJ_FLAG_SHARED;
                    fiber->caller->transfer_value = rv;
                    fiber->caller->state = FIBER_RUNNING;
                    current_fiber_ = fiber->caller;
                }
                return;
            }

            /* Copiar resultados */
            CallFrame *caller_frame = &fiber->frames[fiber->frame_count - 1];
            Value *caller_base = caller_frame->base;
            int copy_count = ret_count < 0 ? nresults : ret_count;
            if (copy_count > nresults)
                copy_count = nresults;

            /* Fast path: single return (most common), then void, then multi */
            if (__builtin_expect(copy_count == 1, 1))
            {
                caller_base[ret_reg] = R[a];
            }
            else if (__builtin_expect(copy_count <= 0, 0))
            {
                /* void return — nothing to copy */
            }
            else
            {
                for (int j = 0; j < copy_count; j++)
                    caller_base[ret_reg + j] = R[a + j];
            }
            /* Nil-fill remaining */
            for (int j = copy_count; j < ret_count && ret_count > 0; j++)
            {
                caller_base[ret_reg + j] = val_nil();
            }

            /* Clear stale callee registers that overlap with the caller's
               GC scan range.  Avoids dangling pointers from callee temporaries
               being scanned by the GC after the call returns.
               Skips result slots (which may coincide with frame->base for
               __init__ where base == &R[ret_reg]).  Typically 3-8 slots
               (~50-128 bytes) — comparable to clear_new_regs at call time. */
            {
                int rslots = copy_count > ret_count ? copy_count : ret_count;
                if (rslots < 0) rslots = 0;
                Value *past_results = caller_base + ret_reg + rslots;
                Value *clear_start = (frame->base > past_results) ? frame->base : past_results;
                Value *clear_end   = frame->base + frame->func->num_regs;
                Value *scan_limit  = caller_base + caller_frame->func->num_regs;
                if (clear_end > scan_limit) clear_end = scan_limit;
                int n = (int)(clear_end - clear_start);
                if (n > 0) memset(clear_start, 0, n * sizeof(Value));
            }

            fiber->stack_top = caller_base + caller_frame->func->num_regs;
            if (external_call_stop_depth_ >= 0 && fiber->frame_count <= external_call_stop_depth_)
            {
                return;
            }
            LOAD_STATE();
            DISPATCH();
        }

        /* --- Closures / Upvalues --- */
        CASE(OP_CLOSURE)
        {
            uint32_t i = *ip;
            int a = ZEN_A(i);
            ObjFunc *fn = as_func(K[ZEN_BX(i)]);

            int nuv = fn->upvalue_count;
            ObjClosure *cl = (ObjClosure *)zen_alloc_now(&gc_, sizeof(ObjClosure));
            cl->obj.type = OBJ_CLOSURE;
            cl->obj.color = GC_BLACK;
            cl->obj.interned = 0;
            cl->obj.hash = 0;
            cl->obj.gc_next = gc_.objects;
            gc_.objects = (Obj *)cl;
            cl->func = fn;
            cl->upvalues = nullptr;
            cl->upvalue_count = 0;
            R[a] = val_obj((Obj *)cl);
            if (nuv > 0)
            {
                cl->upvalues = (ObjUpvalue **)zen_alloc_now(&gc_, nuv * sizeof(ObjUpvalue *));
                for (int j = 0; j < nuv; j++)
                    cl->upvalues[j] = nullptr;
                cl->upvalue_count = nuv;
                for (int j = 0; j < nuv; j++)
                {
                    UpvalDesc &desc = fn->upval_descs[j];
                    if (desc.is_local)
                    {
                        /* Capture from enclosing frame's registers */
                        cl->upvalues[j] = capture_upvalue(fiber, &R[desc.index]);
                    }
                    else
                    {
                        /* Copy from enclosing closure's upvalue */
                        cl->upvalues[j] = UV[desc.index];
                    }
                }
            }
            else
            {
                cl->upvalue_count = 0;
            }
            NEXT();
        }

        CASE(OP_GETUPVAL)
        {
            uint32_t i = *ip;
            Value uv = *UV[ZEN_B(i)]->location;
            /* Mark shared: the string lives in both the parent frame's
               register and this frame's register — intra-frame scan
               at OP_ADD cannot detect cross-frame aliases. */
            if (__builtin_expect(is_string(uv), 0))
                uv.as.obj->flags |= OBJ_FLAG_SHARED;
            R[ZEN_A(i)] = uv;
            NEXT();
        }

        CASE(OP_SETUPVAL)
        {
            uint32_t i = *ip;
            { Value v = R[ZEN_A(i)]; if (is_string(v) && is_obj(v)) v.as.obj->flags |= OBJ_FLAG_SHARED; }
            *UV[ZEN_B(i)]->location = R[ZEN_A(i)];
            NEXT();
        }

        CASE(OP_CLOSE)
        {
            uint32_t i = *ip;
            close_upvalues(fiber, &R[ZEN_A(i)]);
            NEXT();
        }

        /* --- Fibers --- */
        CASE(OP_NEWFIBER)
        {
            uint32_t i = *ip;
            if (!is_closure(R[ZEN_B(i)]))
            {
                RT_ERROR("spawn expects a function");
            }
            ObjClosure *cl = as_closure(R[ZEN_B(i)]);
            ObjFiber *f = new_fiber(cl, 256);
            R[ZEN_A(i)] = val_obj((Obj *)f);
            NEXT();
        }

        CASE(OP_RESUME)
        {
            uint32_t i = *ip;
            if (!is_fiber(R[ZEN_B(i)]))
            {
                RT_ERROR("resume expects a fiber");
            }
            ObjFiber *target = as_fiber(R[ZEN_B(i)]);
            Value send_val = R[ZEN_C(i)];
            ++ip;
            SAVE_IP();

            if (target->state == FIBER_DONE)
            {
                R[ZEN_A(i)] = val_nil();
                DISPATCH();
            }
            if (target->state == FIBER_RUNNING)
            {
                RT_ERROR("cannot resume running fiber");
            }

            if (__builtin_expect(is_string(send_val), 0))
                send_val.as.obj->flags |= OBJ_FLAG_SHARED;
            target->transfer_value = send_val;
            target->caller = fiber;
            target->state = FIBER_RUNNING;
            fiber->state = FIBER_SUSPENDED;
            current_fiber_ = target;

            /* If resuming a suspended fiber (not first time), write
               transfer_value into the yield_dest register */
            if (target->yield_dest >= 0)
            {
                CallFrame &tf = target->frames[target->frame_count - 1];
                Value *target_regs = tf.base;
                target_regs[target->yield_dest] = send_val;
                target->yield_dest = -1;
            }

            if (fiber_depth_ >= kMaxFiberDepth)
            {
                RT_ERROR("fiber resume depth exceeded");
            }
            ++fiber_depth_;
            execute(target);
            --fiber_depth_;

            /* Voltámos — target fez yield ou terminou */
            fiber->state = FIBER_RUNNING;
            current_fiber_ = fiber;
            R[ZEN_A(i)] = target->transfer_value;
            LOAD_STATE();
            DISPATCH();
        }

        CASE(OP_YIELD)
        {
            uint32_t i = *ip;
            ++ip;
            SAVE_IP();

            if (!fiber->caller)
            {
                RT_ERROR("yield outside of fiber");
            }

            Value yv = R[ZEN_B(i)];
            if (__builtin_expect(is_string(yv), 0))
                yv.as.obj->flags |= OBJ_FLAG_SHARED;
            fiber->transfer_value = yv;
            fiber->yield_dest = ZEN_A(i);
            fiber->state = FIBER_SUSPENDED;
            /* Retorna ao execute() do caller (que está em OP_RESUME) */
            return;
        }

        /* OP_AWAIT A, B — R[A] = await R[B]
        ** If R[B] is a fiber: resume it repeatedly until FIBER_DONE,
        ** then R[A] = last transfer_value (the return value).
        ** If R[B] is not a fiber: R[A] = R[B] (pass-through). */
        CASE(OP_AWAIT)
        {
            uint32_t i = *ip;
            ++ip;
            SAVE_IP();

            int a = ZEN_A(i);
            int b = ZEN_B(i);
            Value val = R[b];

            if (!is_fiber(val))
            {
                /* Not a fiber — pass through */
                R[a] = val;
                DISPATCH();
            }

            ObjFiber *target = as_fiber(val);

            /* Resume loop until done */
            while (target->state != FIBER_DONE)
            {
                if (target->state == FIBER_ERROR)
                {
                    RT_ERROR("await: fiber error: %s",
                             target->error ? target->error->chars : "unknown");
                }
                if (target->state == FIBER_RUNNING)
                {
                    RT_ERROR("await: cannot await running fiber");
                }

                target->transfer_value = val_nil();
                target->caller = fiber;
                target->state = FIBER_RUNNING;
                fiber->state = FIBER_SUSPENDED;
                current_fiber_ = target;

                if (target->yield_dest >= 0)
                {
                    CallFrame &tf = target->frames[target->frame_count - 1];
                    tf.base[target->yield_dest] = val_nil();
                    target->yield_dest = -1;
                }

                if (fiber_depth_ >= kMaxFiberDepth)
                {
                    RT_ERROR("await: fiber depth exceeded");
                }
                ++fiber_depth_;
                execute(target);
                --fiber_depth_;

                fiber->state = FIBER_RUNNING;
                current_fiber_ = fiber;

                if (had_error_)
                    return;
            }

            /* The return value was stored in fiber->transfer_value by the
            ** target's final RETURN (via fiber->caller->transfer_value). */
            R[a] = fiber->transfer_value;
            DISPATCH();
        }

        /* OP_FOR_ITER A, B (2-word: word2 = signed jump offset)
        ** R[B] = iterable (array or fiber)
        ** R[B+1] = index counter (for arrays, initialized to 0 before loop)
        ** R[A] = next value
        ** If iteration done → pc += word2 (exit loop) */
        CASE(OP_FOR_ITER)
        {
            uint32_t i = *ip;
            int32_t offset = (int32_t)ip[1]; /* word2 = signed jump */
            ip += 2;

            int a = ZEN_A(i);
            int b = ZEN_B(i);
            Value iterable = R[b];

            if (is_fiber(iterable))
            {
                ObjFiber *target = as_fiber(iterable);
                if (target->state == FIBER_DONE)
                {
                    ip += offset;
                    DISPATCH();
                }
                if (target->state == FIBER_RUNNING)
                {
                    RT_ERROR("cannot iterate running fiber");
                }

                /* Resume the fiber */
                target->transfer_value = val_nil();
                target->caller = fiber;
                target->state = FIBER_RUNNING;
                fiber->state = FIBER_SUSPENDED;
                current_fiber_ = target;

                if (target->yield_dest >= 0)
                {
                    CallFrame &tf = target->frames[target->frame_count - 1];
                    Value *target_regs = tf.base;
                    target_regs[target->yield_dest] = val_nil();
                    target->yield_dest = -1;
                }

                ++fiber_depth_;
                SAVE_IP();
                execute(target);
                --fiber_depth_;

                fiber->state = FIBER_RUNNING;
                current_fiber_ = fiber;

                if (had_error_ || target->state == FIBER_ERROR)
                {
                    return;
                }

                LOAD_STATE();

                if (target->state == FIBER_DONE)
                {
                    ip += offset;
                    DISPATCH();
                }
                R[a] = target->transfer_value;
            }
            else if (is_array(iterable))
            {
                ObjArray *arr = as_array(iterable);
                int32_t idx = R[b + 1].as.integer;
                if (idx >= arr_count(arr))
                {
                    ip += offset;
                    DISPATCH();
                }
                R[a] = arr->data[idx];
                R[b + 1] = val_int(idx + 1);
            }
            else if (is_range(iterable))
            {
                ObjRange *rng = as_range(iterable);
                int64_t idx = R[b + 1].as.integer;
                int64_t cur = rng->start + idx * rng->step;
                if (rng->step > 0 ? cur >= rng->stop : cur <= rng->stop)
                {
                    ip += offset;
                    DISPATCH();
                }
                R[a] = val_int(cur);
                R[b + 1] = val_int(idx + 1);
            }
            else if (is_string(iterable))
            {
                /* String iteration: yield each character as a 1-char string */
                const char *chars = safe_string_chars(iterable);
                int len = safe_string_len(iterable);
                int32_t idx = R[b + 1].as.integer;
                
                if (idx >= len)
                {
                    ip += offset;
                    DISPATCH();
                }
                
                /* Create a single-character string */
                ObjString *char_str = new_string(&gc_, chars + idx, 1);
                R[a] = val_obj((Obj *)char_str);
                R[b + 1] = val_int(idx + 1);
            }
            else if (is_map(iterable))
            {
                /* Map/dict iteration not yet implemented */
                RT_ERROR("'for' on dicts requires calling .items(), .keys(), or .values()");
            }
            else
            {
                RT_ERROR("'for' requires an iterable (array, string, or generator)");
            }
            DISPATCH();
        }

        /* --- Collections --- */
        CASE(OP_NEWARRAY)
        {
            uint32_t i = *ip;
            ObjArray *arr = new_array(&gc_);
            R[ZEN_A(i)] = val_obj((Obj *)arr);
            NEXT();
        }

        CASE(OP_NEWMAP)
        {
            uint32_t i = *ip;
            ObjMap *map = new_map(&gc_);
            R[ZEN_A(i)] = val_obj((Obj *)map);
            NEXT();
        }

        CASE(OP_NEWSET)
        {
            uint32_t i = *ip;
            ObjSet *set = new_set(&gc_);
            R[ZEN_A(i)] = val_obj((Obj *)set);
            NEXT();
        }

        CASE(OP_NEWBUFFER)
        {
            uint32_t i = *ip;
            int a = ZEN_A(i);
            int b = ZEN_B(i);
            BufferType btype = (BufferType)ZEN_C(i);
            Value arg = R[b];
            if (is_int(arg))
            {
                int32_t count = arg.as.integer;
                if (count < 0)
                {
                    RT_ERROR("buffer size must be non-negative");
                }
                ObjBuffer *buf = new_buffer(&gc_, btype, count);
                R[a] = val_obj((Obj *)buf);
            }
            else if (is_array(arg))
            {
                ObjArray *src = as_array(arg);
                int32_t count = arr_count(src);
                ObjBuffer *buf = new_buffer(&gc_, btype, count);
                for (int32_t idx = 0; idx < count; idx++)
                {
                    double v = 0;
                    if (is_int(src->data[idx]))
                        v = (double)src->data[idx].as.integer;
                    else if (is_float(src->data[idx]))
                        v = src->data[idx].as.number;
                    buffer_set(buf, idx, v);
                }
                R[a] = val_obj((Obj *)buf);
            }
            else
            {
                RT_ERROR("buffer constructor expects int or array");
            }
            NEXT();
        }

        CASE(OP_APPEND)
        {
            uint32_t i = *ip;
            ObjArray *arr = as_array(R[ZEN_A(i)]);
            Value v = R[ZEN_B(i)];
            if (__builtin_expect(is_string(v), 0))
                v.as.obj->flags |= OBJ_FLAG_SHARED;
            array_push(&gc_, arr, v);
            NEXT();
        }

        CASE(OP_SETADD)
        {
            uint32_t i = *ip;
            ObjSet *set = as_set(R[ZEN_A(i)]);
            Value v = R[ZEN_B(i)];
            if (__builtin_expect(is_string(v), 0))
                v.as.obj->flags |= OBJ_FLAG_SHARED;
            set_add(&gc_, set, v);
            NEXT();
        }

        /* --- Field/Index access --- */
        CASE(OP_GETFIELD)
        {
            /* R[A] = R[B].field_by_name(constants[C]) — pointer compare fallback */
            uint32_t i = *ip;
            Value receiver = R[ZEN_B(i)];
            ObjString *name = as_string(K[ZEN_C(i)]);

            if (is_struct(receiver))
            {
                ObjStruct *s = as_struct(receiver);
                ObjStructDef *def = s->def;
                for (int32_t fi = 0; fi < def->num_fields; fi++)
                {
                    if (def->field_names[fi] == name) /* pointer compare (interned) */
                    {
                        R[ZEN_A(i)] = s->fields[fi];
                        goto getfield_done;
                    }
                }
                RT_ERROR("struct '%s' has no field '%s'", def->name->chars, name->chars);
            }
            if (is_native_struct(receiver))
            {
                ObjNativeStruct *ns = as_native_struct(receiver);
                NativeStructDef *def = ns->def;
                for (int16_t fi = 0; fi < def->num_fields; fi++)
                {
                    if (def->fields[fi].name == name)
                    {
                        uint8_t *base = (uint8_t *)ns->data + def->fields[fi].offset;
                        switch (def->fields[fi].type)
                        {
                        case FIELD_BYTE:    R[ZEN_A(i)] = val_int(*(uint8_t *)base); break;
                        case FIELD_INT:     R[ZEN_A(i)] = val_int(*(int32_t *)base); break;
                        case FIELD_UINT:    R[ZEN_A(i)] = val_int((int64_t)*(uint32_t *)base); break;
                        case FIELD_FLOAT:   R[ZEN_A(i)] = val_float((double)*(float *)base); break;
                        case FIELD_DOUBLE:  R[ZEN_A(i)] = val_float(*(double *)base); break;
                        case FIELD_BOOL:    R[ZEN_A(i)] = val_bool(*(bool *)base); break;
                        case FIELD_POINTER: R[ZEN_A(i)] = val_ptr(*(void **)base); break;
                        }
                        goto getfield_done;
                    }
                }
                RT_ERROR("native struct '%s' has no field '%s'", def->name->chars, name->chars);
            }
            if (is_instance(receiver))
            {
                ObjInstance *inst = as_instance(receiver);
                ObjClass *klass = inst->klass;
                for (int32_t fi = 0; fi < klass->num_fields; fi++)
                {
                    if (klass->field_names[fi] == name) /* pointer compare (interned) */
                    {
                        R[ZEN_A(i)] = (fi < inst->num_fields) ? inst->fields[fi] : val_nil();
                        goto getfield_done;
                    }
                }
                /* Field not found */
                RT_ERROR("instance of '%s' has no field '%s'", klass->name->chars, name->chars);
            }
            if (is_map(receiver))
            {
                /* Module / dict field access: R[A] = map[name] */
                bool found;
                Value v = map_get(as_map(receiver), val_obj((Obj *)name), &found);
                if (!found)
                    RT_ERROR("module has no attribute '%s'", name->chars);
                R[ZEN_A(i)] = v;
                goto getfield_done;
            }
            RT_ERROR("cannot access field '%s' on this type", name->chars);
        getfield_done:
            NEXT();
        }
        CASE(OP_SETFIELD)
        {
            /* R[A].field_by_name(constants[B]) = R[C] — pointer compare fallback */
            uint32_t i = *ip;
            Value receiver = R[ZEN_A(i)];
            ObjString *name = as_string(K[ZEN_B(i)]);
            Value val = R[ZEN_C(i)];
            if (is_string(val) && is_obj(val)) val.as.obj->flags |= OBJ_FLAG_SHARED;

            if (is_class(receiver))
            {
                /* Setting a method on a class object */
                ObjClass *klass = as_class(receiver);
                map_set(&gc_, klass->methods, val_obj((Obj *)name), val);
                /* Fill operator_slots for dunder methods (__add__, __eq__, etc.) */
                {
                    int op_slot = operator_slot_for_name(name->chars, name->length);
                    if (op_slot >= 0)
                        klass->operator_slots[op_slot] = val;
                }

                /* Also register in vtable — intern selector to ensure slot exists */
                int sel = intern_selector(name->chars, name->length);
                if (sel >= 0)
                {
                    if (sel >= klass->vtable_size)
                    {
                        int old_size = klass->vtable_size;
                        int new_size = sel + 1;
                        gc_pause(&gc_);
                        klass->vtable = (Value *)zen_realloc(
                            &gc_, klass->vtable,
                            sizeof(Value) * old_size,
                            sizeof(Value) * new_size);
                        gc_resume(&gc_);
                        for (int si = old_size; si < new_size; si++)
                            klass->vtable[si] = val_nil();
                        klass->vtable_size = new_size;
                    }
                    klass->vtable[sel] = val;
                }
                goto setfield_done;
            }

            if (is_struct(receiver))
            {
                ObjStruct *s = as_struct(receiver);
                ObjStructDef *def = s->def;
                for (int32_t fi = 0; fi < def->num_fields; fi++)
                {
                    if (def->field_names[fi] == name)
                    {
                        s->fields[fi] = val;
                        goto setfield_done;
                    }
                }
                RT_ERROR("struct '%s' has no field '%s'", def->name->chars, name->chars);
            }

            if (is_native_struct(receiver))
            {
                ObjNativeStruct *ns = as_native_struct(receiver);
                NativeStructDef *ndef = ns->def;
                for (int16_t fi = 0; fi < ndef->num_fields; fi++)
                {
                    if (ndef->fields[fi].name == name)
                    {
                        if (ndef->fields[fi].read_only)
                            RT_ERROR("field '%s' of native struct '%s' is read-only", name->chars, ndef->name->chars);
                        uint8_t *base = (uint8_t *)ns->data + ndef->fields[fi].offset;
                        switch (ndef->fields[fi].type)
                        {
                        case FIELD_BYTE:    *(uint8_t *)base  = (uint8_t)val.as.integer; break;
                        case FIELD_INT:     *(int32_t *)base  = (int32_t)val.as.integer; break;
                        case FIELD_UINT:    *(uint32_t *)base = (uint32_t)val.as.integer; break;
                        case FIELD_FLOAT:   *(float *)base    = (val.type == VAL_FLOAT) ? (float)val.as.number : (float)val.as.integer; break;
                        case FIELD_DOUBLE:  *(double *)base   = (val.type == VAL_FLOAT) ? val.as.number : (double)val.as.integer; break;
                        case FIELD_BOOL:    *(bool *)base     = val.as.boolean; break;
                        case FIELD_POINTER: *(void **)base    = val.as.pointer; break;
                        }
                        goto setfield_done;
                    }
                }
                RT_ERROR("native struct '%s' has no field '%s'", ndef->name->chars, name->chars);
            }

            if (is_instance(receiver))
            {
                ObjInstance *inst = as_instance(receiver);
                ObjClass *klass = inst->klass;
                /* Look for existing field in this instance */
                for (int32_t fi = 0; fi < inst->num_fields; fi++)
                {
                    if (klass->field_names[fi] == name) /* pointer compare (interned) */
                    {
                        inst->fields[fi] = val;
                        goto setfield_done;
                    }
                }
                /* Field exists in class template but not yet in this instance? Grow. */
                /* Or brand new field — register in class + grow instance. */
                {
                    /* Check if class already knows this field name */
                    int32_t class_idx = -1;
                    for (int32_t fi = 0; fi < klass->num_fields; fi++)
                    {
                        if (klass->field_names[fi] == name)
                        {
                            class_idx = fi;
                            break;
                        }
                    }
                    if (class_idx < 0)
                    {
                        /* New field — register in class */
                        class_idx = klass->num_fields;
                        klass->num_fields = class_idx + 1;
                        gc_pause(&gc_);
                        klass->field_names = (ObjString **)zen_realloc(
                            &gc_, klass->field_names,
                            sizeof(ObjString *) * class_idx,
                            sizeof(ObjString *) * (class_idx + 1));
                        gc_resume(&gc_);
                        klass->field_names[class_idx] = name;
                    }
                    /* Grow instance fields — all use arena via zen_realloc. */
                    int32_t old_n = inst->num_fields;
                    int32_t new_n = class_idx + 1;
                    gc_pause(&gc_);
                    inst->fields = (Value *)zen_realloc(
                        &gc_, inst->fields,
                        sizeof(Value) * old_n,
                        sizeof(Value) * new_n);
                    gc_resume(&gc_);
                    for (int32_t fi = old_n; fi < new_n; fi++)
                        inst->fields[fi] = val_nil();
                    inst->fields[class_idx] = val;
                    inst->num_fields = new_n;
                    goto setfield_done;
                }
            }
            RT_ERROR("cannot set field '%s' on this type", name->chars);
        setfield_done:
            NEXT();
        }
        CASE(OP_GETFIELD_IDX)
        {
            /* R[A] = R[B].fields[C] — O(1) direct index */
            uint32_t i = *ip;
            Value obj = R[ZEN_B(i)];
            const int field_idx = ZEN_C(i);
            if (is_instance(obj))
            {
                ObjInstance *inst = as_instance(obj);
                if (field_idx < 0 || field_idx >= inst->num_fields)
                {
                    RT_ERROR("GETFIELD_IDX out of bounds: receiver=R%d class=%s field_index=%d field_count=%d", ZEN_B(i), inst->klass->name->chars, field_idx, inst->num_fields);
                }
                R[ZEN_A(i)] = inst->fields[field_idx];
            }
            else if (is_struct(obj))
            {
                ObjStruct *s = as_struct(obj);
                if (field_idx < 0 || field_idx >= s->def->num_fields)
                    RT_ERROR("struct field index out of bounds");
                R[ZEN_A(i)] = s->fields[field_idx];
            }
            else
            {
                RT_ERROR("GETFIELD_IDX expected instance/struct: dst=R%d receiver=R%d receiver_type=%s field_index=%d", ZEN_A(i), ZEN_B(i), value_debug_type(obj), field_idx);
            }
            NEXT();
        }
        CASE(OP_SETFIELD_IDX)
        {
            /* R[A].fields[B] = R[C] — O(1) direct index; grows instance if needed */
            uint32_t i = *ip;
            Value recv = R[ZEN_A(i)];
            const int field_idx = ZEN_B(i);
            { Value v = R[ZEN_C(i)]; if (is_string(v) && is_obj(v)) v.as.obj->flags |= OBJ_FLAG_SHARED; }
            if (is_instance(recv))
            {
                ObjInstance *inst = as_instance(recv);
                if (__builtin_expect(field_idx < inst->num_fields, 1))
                {
                    inst->fields[field_idx] = R[ZEN_C(i)];
                }
                else
                {
                    int old_n = inst->num_fields;
                    int new_n = field_idx + 1;
                    /* All instances use arena — persistent ones are simply
                    ** not in the GC object list so never swept. */
                    gc_pause(&gc_);
                    inst->fields = (Value *)zen_realloc(
                        &gc_, inst->fields,
                        sizeof(Value) * old_n,
                        sizeof(Value) * new_n);
                    gc_resume(&gc_);
                    for (int fi = old_n; fi < new_n; fi++)
                        inst->fields[fi] = val_nil();
                    inst->num_fields = new_n;
                    inst->fields[field_idx] = R[ZEN_C(i)];
                }
            }
            else if (is_struct(recv))
            {
                ObjStruct *s = as_struct(recv);
                if (field_idx < 0 || field_idx >= s->def->num_fields)
                    RT_ERROR("struct field index out of bounds");
                s->fields[field_idx] = R[ZEN_C(i)];
            }
            else
            {
                RT_ERROR("SETFIELD_IDX expected instance/struct");
            }
            NEXT();
        }
        CASE(OP_GETINDEX)
        {
            uint32_t i = *ip;
            Value container = R[ZEN_B(i)];
            Value key = R[ZEN_C(i)];
            if (is_array(container))
            {
                if (!is_int(key))
                {
                    RT_ERROR("array index must be integer");
                }
                int32_t idx = (int32_t)key.as.integer;
                ObjArray *arr = as_array(container);
                if (idx < 0)
                    idx += arr_count(arr);
                if ((uint32_t)idx >= (uint32_t)arr_count(arr))
                {
                    RT_ERROR("array index out of bounds");
                }
                R[ZEN_A(i)] = arr->data[idx];
            }
            else if (is_map(container))
            {
                bool found;
                R[ZEN_A(i)] = map_get(as_map(container), key, &found);
                if (!found)
                    R[ZEN_A(i)] = val_nil();
            }
            else if (is_buffer(container))
            {
                if (!is_int(key))
                {
                    RT_ERROR("buffer index must be integer");
                }
                ObjBuffer *buf = as_buffer(container);
                int32_t idx = (int32_t)key.as.integer;
                if (idx < 0)
                    idx += buf->count;
                if ((uint32_t)idx >= (uint32_t)buf->count)
                {
                    RT_ERROR("buffer index out of bounds");
                }
                double v = buffer_get(buf, idx);
                /* Float types return float, integer types return int64 */
                if (buf->btype >= BUF_FLOAT32)
                    R[ZEN_A(i)] = val_float(v);
                else
                    R[ZEN_A(i)] = val_int((int64_t)v);
            }
            else if (is_string(container))
            {
                if (!is_int(key))
                {
                    RT_ERROR("string index must be integer");
                }
                ObjString *s = as_string(container);
                int32_t idx = (int32_t)key.as.integer;
                if (idx < 0)
                    idx += s->length;
                if ((uint32_t)idx >= (uint32_t)s->length)
                {
                    RT_ERROR("string index out of bounds");
                }
                R[ZEN_A(i)] = val_obj((Obj *)new_string(&gc_, &s->chars[idx], 1));
            }
            else if (is_range(container))
            {
                if (!is_int(key))
                {
                    RT_ERROR("range index must be integer");
                }
                ObjRange *r = as_range(container);
                R[ZEN_A(i)] = val_int(r->start + key.as.integer * r->step);
            }
            else
            {
                RT_ERROR("cannot index value");
            }
            NEXT();
        }
        CASE(OP_ITER_ELEM)
        {
            uint32_t i = *ip;
            Value container = R[ZEN_B(i)];
            Value vidx = R[ZEN_C(i)];
            int32_t idx = (int32_t)vidx.as.integer;
            if (is_array(container))
            {
                R[ZEN_A(i)] = array_get(as_array(container), idx);
            }
            else if (is_buffer(container))
            {
                ObjBuffer *buf = as_buffer(container);
                if ((uint32_t)idx >= (uint32_t)buf->count)
                {
                    RT_ERROR("buffer index out of bounds");
                }
                double v = buffer_get(buf, idx);
                if (buf->btype >= BUF_FLOAT32)
                    R[ZEN_A(i)] = val_float(v);
                else
                    R[ZEN_A(i)] = val_int((int64_t)v);
            }
            else if (is_string(container))
            {
                ObjString *s = as_string(container);
                if ((uint32_t)idx >= (uint32_t)s->length)
                {
                    RT_ERROR("string index out of bounds");
                }
                R[ZEN_A(i)] = val_obj((Obj *)new_string(&gc_, &s->chars[idx], 1));
            }
            else if (is_map(container))
            {
                ObjMap *map = as_map(container);
                int32_t ord = 0;
                bool found = false;
                for (int32_t n = 0; n < map->capacity; n++)
                {
                    if (map->nodes[n].hash != 0xFFFFFFFFu)
                    {
                        if (ord == idx)
                        {
                            R[ZEN_A(i)] = map->nodes[n].key;
                            found = true;
                            break;
                        }
                        ord++;
                    }
                }
                if (!found)
                {
                    RT_ERROR("map iteration index out of bounds");
                }
            }
            else if (is_set(container))
            {
                ObjSet *set = as_set(container);
                int32_t ord = 0;
                bool found = false;
                for (int32_t n = 0; n < set->capacity; n++)
                {
                    if (set->nodes[n].hash != 0xFFFFFFFFu)
                    {
                        if (ord == idx)
                        {
                            R[ZEN_A(i)] = set->nodes[n].key;
                            found = true;
                            break;
                        }
                        ord++;
                    }
                }
                if (!found)
                {
                    RT_ERROR("set iteration index out of bounds");
                }
            }
            else
            {
                RT_ERROR("cannot iterate value");
            }
            NEXT();
        }
        CASE(OP_SETINDEX)
        {
            uint32_t i = *ip;
            Value container = R[ZEN_A(i)];
            Value key = R[ZEN_B(i)];
            Value val = R[ZEN_C(i)];
            if (is_string(val) && is_obj(val)) val.as.obj->flags |= OBJ_FLAG_SHARED;
            if (is_array(container))
            {
                if (!is_int(key))
                {
                    RT_ERROR("array index must be integer");
                }
                int32_t idx = key.as.integer;
                ObjArray *arr = as_array(container);
                if (idx < 0)
                    idx += arr_count(arr);
                if ((uint32_t)idx < (uint32_t)arr_count(arr))
                {
                    arr->data[idx] = val;
                }
                else
                {
                    RT_ERROR("array index out of bounds");
                }
            }
            else if (is_map(container))
            {
                if (is_string(key) && is_obj(key)) key.as.obj->flags |= OBJ_FLAG_SHARED;
                map_set(&gc_, as_map(container), key, val);
            }
            else if (is_buffer(container))
            {
                if (!is_int(key))
                {
                    RT_ERROR("buffer index must be integer");
                }
                ObjBuffer *buf = as_buffer(container);
                int32_t idx = key.as.integer;
                if (idx < 0)
                    idx += buf->count;
                if ((uint32_t)idx >= (uint32_t)buf->count)
                {
                    RT_ERROR("buffer index out of bounds");
                }
                double v = 0;
                if (is_int(val))
                    v = (double)val.as.integer;
                else if (is_float(val))
                    v = val.as.number;
                else
                {
                    RT_ERROR("buffer only accepts numbers");
                }
                buffer_set(buf, idx, v);
            }
            else
            {
                RT_ERROR("cannot index value (container type: %s)", val_type_str(container));
            }
            NEXT();
        }

        CASE(OP_DELINDEX)
        {
            uint32_t i = *ip;
            Value container = R[ZEN_A(i)];
            Value key = R[ZEN_B(i)];
            if (is_array(container))
            {
                if (!is_int(key))
                {
                    RT_ERROR("array index must be integer");
                }
                ObjArray *arr = as_array(container);
                int32_t idx = (int32_t)key.as.integer;
                if (idx < 0)
                    idx += arr_count(arr);
                if ((uint32_t)idx >= (uint32_t)arr_count(arr))
                {
                    RT_ERROR("del: index out of bounds");
                }
                array_remove(arr, idx);
            }
            else if (is_map(container))
            {
                map_delete(as_map(container), key);
            }
            else
            {
                RT_ERROR("del: cannot delete from this type");
            }
            NEXT();
        }

        CASE(OP_GETSLICE)
        {
            /* R[A] = R[B][R[C] : R[C+1] : R[C+2]]
            ** nil in start/stop/step means omitted (Python None). */
            uint32_t i = *ip;
            int dst = ZEN_A(i);
            Value container = R[ZEN_B(i)];
            int base = ZEN_C(i);
            Value vstart = R[base];
            Value vstop = R[base + 1];
            Value vstep = R[base + 2];

            int32_t step = is_int(vstep) ? (int32_t)vstep.as.integer : 1;
            if (step == 0)
            {
                RT_ERROR("slice step cannot be zero");
            }

            if (is_array(container))
            {
                ObjArray *src = as_array(container);
                int32_t len = arr_count(src);
                int32_t start = is_int(vstart) ? (int32_t)vstart.as.integer : (step > 0 ? 0 : len - 1);
                int32_t stop = is_int(vstop) ? (int32_t)vstop.as.integer : (step > 0 ? len : -len - 1);
                /* Normalize negative indices */
                if (start < 0)
                    start += len;
                if (stop < 0)
                    stop += len;
                /* Clamp */
                if (step > 0)
                {
                    if (start < 0)
                        start = 0;
                    if (stop > len)
                        stop = len;
                }
                else
                {
                    if (start >= len)
                        start = len - 1;
                    if (stop < -1)
                        stop = -1;
                }
                ObjArray *result = new_array(&gc_);
                R[dst] = val_obj((Obj *)result); /* root before push triggers GC */
                if (step > 0)
                {
                    for (int32_t k = start; k < stop; k += step)
                        array_push(&gc_, as_array(R[dst]), src->data[k]);
                }
                else
                {
                    for (int32_t k = start; k > stop; k += step)
                        array_push(&gc_, as_array(R[dst]), src->data[k]);
                }
            }
            else if (is_string(container))
            {
                ObjString *src = as_string(container);
                int32_t len = src->length;
                int32_t start = is_int(vstart) ? (int32_t)vstart.as.integer : (step > 0 ? 0 : len - 1);
                int32_t stop = is_int(vstop) ? (int32_t)vstop.as.integer : (step > 0 ? len : -len - 1);
                if (start < 0)
                    start += len;
                if (stop < 0)
                    stop += len;
                if (step > 0)
                {
                    if (start < 0)
                        start = 0;
                    if (stop > len)
                        stop = len;
                }
                else
                {
                    if (start >= len)
                        start = len - 1;
                    if (stop < -1)
                        stop = -1;
                }
                /* Build result string */
                int32_t count = 0;
                if (step > 0)
                {
                    for (int32_t k = start; k < stop; k += step)
                        count++;
                }
                else
                {
                    for (int32_t k = start; k > stop; k += step)
                        count++;
                }
                char *buf = (char *)zen_alloc_now(&gc_, (size_t)(count + 1));
                int32_t wi = 0;
                if (step > 0)
                {
                    for (int32_t k = start; k < stop; k += step)
                        buf[wi++] = src->chars[k];
                }
                else
                {
                    for (int32_t k = start; k > stop; k += step)
                        buf[wi++] = src->chars[k];
                }
                buf[wi] = '\0';
                ObjString *res = create_string(&gc_, buf, count);
                zen_free(&gc_, buf, (size_t)(count + 1));
                R[dst] = val_obj((Obj *)res);
            }
            else
            {
                RT_ERROR("slice not supported on this type");
            }
            NEXT();
        }

        /* --- OP_INVOKE: method dispatch by receiver type --- */
        /* 2-word instruction: word1=[OP_INVOKE|A|B|C], word2=name_ki */
        /* A=base (receiver at R[A], args at R[A+1]..R[A+B]), result → R[A] */
        CASE(OP_INVOKE)
        {
            uint32_t i = *ip;
            uint8_t base = ZEN_A(i);
            uint8_t arg_count = ZEN_B(i);
            uint8_t nresults = ZEN_C(i);
            if (nresults == 0) nresults = 1;
            uint32_t word2 = *(++ip); /* packed: (selector_slot << 16) | name_ki */
            uint16_t sel_slot = (uint16_t)(word2 >> 16);
            uint16_t name_ki = (uint16_t)(word2 & 0xFFFF);
            Value receiver = R[base];
            ObjString *method = as_string(K[name_ki]);
            const char *mname = method->chars;
            Value *args = &R[base + 1];

            /* Mark string args as shared — matches OP_CALL behaviour */
            for (int ai = 0; ai < arg_count; ai++) {
                if (__builtin_expect(is_string(args[ai]), 0))
                    args[ai].as.obj->flags |= OBJ_FLAG_SHARED;
            }

            if (is_array(receiver))
            {
#include "invoke_array.inl"
            }
            else if (is_string(receiver))
            {
#include "invoke_string.inl"
            }
            else if (is_map(receiver))
            {
#include "invoke_map.inl"
            }
            else if (is_set(receiver))
            {
#include "invoke_set.inl"
            }
            else if (is_buffer(receiver))
            {
#include "invoke_buffer.inl"
            }
            else if (is_instance(receiver))
            {
                /* Vtable dispatch using compile-time selector slot */
                ObjInstance *inst = as_instance(receiver);
                ObjClass *klass = inst->klass;

                /* Walk class hierarchy for vtable lookup */
                Value mval = val_nil();
                ObjClass *search = klass;
                while (search != nullptr)
                {
                    if (sel_slot < search->vtable_size)
                        mval = search->vtable[sel_slot];
                    if (!is_nil(mval))
                        break;
                    search = search->parent;
                }

                if (is_nil(mval))
                {
                    RT_ERROR("'%s' has no method '%s'", klass->name->chars, mname);
                }

                if (is_closure(mval))
                {
                    ObjClosure *cl = as_closure(mval);
                    ObjFunc *fn = cl->func;
                    if (fn->arity < 0)
                    {
                        /* Vararg method */
                        int min_args = (-fn->arity) - 1;
                        if (arg_count < min_args)
                            RT_ERROR("%s.%s() expects at least %d args but got %d", klass->name->chars, mname, min_args, arg_count);
                    }
                    else if (fn->default_count > 0)
                    {
                        int required = fn->arity - fn->default_count;
                        if (arg_count < required)
                            RT_ERROR("%s.%s() expects at least %d args but got %d", klass->name->chars, mname, required, arg_count);
                        if (arg_count > fn->arity)
                            RT_ERROR("%s.%s() expects at most %d args but got %d", klass->name->chars, mname, fn->arity, arg_count);
                    }
                    else if (arg_count != fn->arity)
                    {
                        RT_ERROR("%s.%s() expects %d args but got %d", klass->name->chars, mname, fn->arity, arg_count);
                    }
                    if (fiber->frame_count >= kMaxFrames)
                    {
                        RT_ERROR("stack overflow");
                    }
                    CHECK_STACK_SPACE(fiber, &R[base], fn->num_regs);
                    ++ip;
                    SAVE_IP();
                    CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
                    new_frame->closure = cl;
                    new_frame->func = fn;
                    new_frame->ip = fn->code;
                    new_frame->base = &R[base]; /* base[0]=self, base[1..]=args */
                    new_frame->ret_reg = base;
                    new_frame->ret_count = nresults;
                    fiber->stack_top = new_frame->base + fn->num_regs;

                    /* Fill in default values for missing args */
                    if (fn->arity >= 0 && fn->default_count > 0 && arg_count < fn->arity)
                    {
                        int required = fn->arity - fn->default_count;
                        for (int di = arg_count; di < fn->arity; di++)
                            new_frame->base[1 + di] = fn->defaults[di - required];
                    }

                    /* Clear unused regs: self(1) + arity args */
                    {
                        int used = 1 + (fn->arity >= 0 ? fn->arity : arg_count);
                        clear_new_regs(new_frame->base, used, fn->num_regs);
                    }

                    LOAD_STATE();
                    DISPATCH();
                }
                else if (is_native(mval))
                {
                    ObjNative *nat = as_native(mval);
                    /* ClassBuilder convention: args[-1]=self, args[0..n-1]=arguments */
                    int nret = call_native(this, nat->fn, &R[base + 1], arg_count);
                    if (nret >= 0)
                        copy_native_results(&R[base], &R[base + 1], nret, nresults);
                    else
                        RT_ERROR("native method '%s' returned error", mname);
                }
                else
                {
                    RT_ERROR("'%s.%s' is not callable", klass->name->chars, mname);
                }
            }
            else
            {
                RT_ERROR("cannot invoke method '%s' on this type", mname);
            }
            NEXT();
        }

        CASE(OP_INVOKE_VT)
        {
            /* Single-word vtable dispatch: A=base, B=arg_count, C=slot_idx */
            uint32_t i = *ip;
            uint8_t base = ZEN_A(i);
            uint8_t arg_count = ZEN_B(i);
            uint8_t slot = ZEN_C(i);

            ObjInstance *inst = as_instance(R[base]);
            ObjClass *klass = inst->klass;
            Value mval = klass->vtable[slot];

            /* Mark string args as shared — matches OP_CALL behaviour */
            for (int ai = 0; ai < arg_count; ai++) {
                Value av = R[base + 1 + ai];
                if (__builtin_expect(is_string(av), 0))
                    av.as.obj->flags |= OBJ_FLAG_SHARED;
            }

            if (is_closure(mval))
            {
                ObjClosure *cl = as_closure(mval);
                ObjFunc *fn = cl->func;
                if (fiber->frame_count >= kMaxFrames)
                {
                    RT_ERROR("stack overflow");
                }
                CHECK_STACK_SPACE(fiber, &R[base], fn->num_regs);
                ++ip;
                SAVE_IP();
                CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
                new_frame->closure = cl;
                new_frame->func = fn;
                new_frame->ip = fn->code;
                new_frame->base = &R[base]; /* base[0]=self, base[1..]=args */
                new_frame->ret_reg = base;
                new_frame->ret_count = 1;
                fiber->stack_top = new_frame->base + fn->num_regs;
                clear_new_regs(new_frame->base, 1 + arg_count, fn->num_regs);
                LOAD_STATE();
                DISPATCH();
            }
            else if (is_native(mval))
            {
                ObjNative *nat = as_native(mval);
                /* ClassBuilder convention: args[-1]=self, args[0..n-1]=arguments */
                int nret = call_native(this, nat->fn, &R[base + 1], arg_count);
                if (nret > 0)
                    R[base] = R[base + 1];
                else if (nret == 0)
                    R[base] = val_nil();
                else
                    RT_ERROR("native method returned error");
            }
            else
            {
                RT_ERROR("vtable slot %d is nil (method not found)", slot);
            }
            NEXT();
        }

        CASE(OP_SUPER_INVOKE)
        {
            /* 3-word: word1=[OP|base|argc|0], word2=(sel<<16|name_ki), word3=parent_gidx */
            uint32_t i = *ip;
            uint8_t base = ZEN_A(i);
            uint8_t arg_count = ZEN_B(i);
            uint32_t word2 = ip[1];
            int sel_slot = (int)(word2 >> 16);
            int name_ki = (int)(word2 & 0xFFFF);
            uint32_t parent_gidx = ip[2];

            /* Resolve parent class from globals table */
            Value parent_val = globals_[parent_gidx];
            if (!is_class(parent_val))
            {
                RT_ERROR("super: parent is not a class");
            }
            ObjClass *parent = as_class(parent_val);

            /* Mark string args as shared — matches OP_CALL behaviour */
            for (int ai = 0; ai < arg_count; ai++) {
                Value av = R[base + 1 + ai];
                if (__builtin_expect(is_string(av), 0))
                    av.as.obj->flags |= OBJ_FLAG_SHARED;
            }

            /* Vtable lookup on static parent */
            Value mval = val_nil();
            if (sel_slot < parent->vtable_size)
                mval = parent->vtable[sel_slot];

            if (is_nil(mval))
            {
                const char *mname = as_string(frame->func->constants[name_ki])->chars;
                RT_ERROR("parent class '%s' has no method '%s'", parent->name->chars, mname);
            }

            if (is_closure(mval))
            {
                ObjClosure *cl = as_closure(mval);
                ObjFunc *fn = cl->func;
                if (fn->arity < 0)
                {
                    int min_args = (-fn->arity) - 1;
                    if (arg_count < min_args)
                    {
                        const char *mname = as_string(frame->func->constants[name_ki])->chars;
                        RT_ERROR("super.%s() expects at least %d args but got %d", mname, min_args, arg_count);
                    }
                }
                else if (fn->default_count > 0)
                {
                    int required = fn->arity - fn->default_count;
                    if (arg_count < required)
                    {
                        const char *mname = as_string(frame->func->constants[name_ki])->chars;
                        RT_ERROR("super.%s() expects at least %d args but got %d", mname, required, arg_count);
                    }
                    if (arg_count > fn->arity)
                    {
                        const char *mname = as_string(frame->func->constants[name_ki])->chars;
                        RT_ERROR("super.%s() expects at most %d args but got %d", mname, fn->arity, arg_count);
                    }
                }
                else if (arg_count != fn->arity)
                {
                    const char *mname = as_string(frame->func->constants[name_ki])->chars;
                    RT_ERROR("super.%s() expects %d args but got %d", mname, fn->arity, arg_count);
                }
                if (fiber->frame_count >= kMaxFrames)
                {
                    RT_ERROR("stack overflow");
                }
                CHECK_STACK_SPACE(fiber, &R[base], fn->num_regs);
                ip += 3; /* skip all 3 words */
                SAVE_IP();
                CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
                new_frame->closure = cl;
                new_frame->func = fn;
                new_frame->ip = fn->code;
                new_frame->base = &R[base]; /* base[0]=self, base[1..]=args */
                new_frame->ret_reg = base;
                new_frame->ret_count = 1;
                fiber->stack_top = new_frame->base + fn->num_regs;

                /* Fill in default values for missing args */
                if (fn->arity >= 0 && fn->default_count > 0 && arg_count < fn->arity)
                {
                    int required = fn->arity - fn->default_count;
                    for (int di = arg_count; di < fn->arity; di++)
                        new_frame->base[1 + di] = fn->defaults[di - required];
                }

                /* Clear unused regs: self(1) + arity args */
                {
                    int used = 1 + (fn->arity >= 0 ? fn->arity : arg_count);
                    clear_new_regs(new_frame->base, used, fn->num_regs);
                }

                LOAD_STATE();
                DISPATCH();
            }
            else if (is_native(mval))
            {
                ObjNative *nat = as_native(mval);
                /* ClassBuilder convention: args[-1]=self, args[0..n-1]=arguments */
                int nret = call_native(this, nat->fn, &R[base + 1], arg_count);
                if (nret > 0)
                    R[base] = R[base + 1];
                else if (nret == 0)
                    R[base] = val_nil();
                else
                {
                    const char *mn = as_string(frame->func->constants[name_ki])->chars;
                    RT_ERROR("native method '%s' returned error", mn);
                }
            }
            else
            {
                const char *mname = as_string(frame->func->constants[name_ki])->chars;
                RT_ERROR("super.%s is not callable", mname);
            }
            ip += 2; /* skip word2+word3 */
            NEXT();
        }

        /* --- Classes --- */
        CASE(OP_NEWCLASS)
        {
            /* R[A] = new class; name from constants[B]; parent from R[C] (255=no parent) */
            uint32_t i = *ip;
            ObjString *name = as_string(K[ZEN_B(i)]);
            uint8_t c = ZEN_C(i);
            ObjClass *parent = nullptr;
            if (c != 255)
            {
                if (!is_class(R[c]))
                {
                    RT_ERROR("superclass must be a class");
                }
                parent = as_class(R[c]);
            }
            ObjClass *klass = new_class(&gc_, name, parent);
            /* Into the register first: the allocation below can trigger a
            ** collection, and until the class is in a register nothing roots
            ** it - it would be swept and its memory handed to the next
            ** object allocated. */
            R[ZEN_A(i)] = val_obj((Obj *)klass);
            /* Flatten the parent's field defaults into the subclass, the way
            ** the vtable is flattened: the compiler gives a subclass the
            ** parent's field indices, so a straight copy lines up, and the
            ** subclass's own OP_CLASSFIELDDEF then overwrites what it
            ** redeclares and extends the array for what it adds. */
            if (parent && parent->field_defaults && parent->num_field_defaults > 0)
            {
                const int32_t n = parent->num_field_defaults;
                klass->field_defaults = (Value *)zen_alloc(&gc_, sizeof(Value) * (size_t)n);
                for (int32_t fi = 0; fi < n; fi++)
                    klass->field_defaults[fi] = parent->field_defaults[fi];
                klass->num_field_defaults = n;
            }
            NEXT();
        }
        CASE(OP_NEWINSTANCE)
        {
            /* R[A] = new instance of class R[B] */
            uint32_t i = *ip;
            if (!is_class(R[ZEN_B(i)]))
            {
                RT_ERROR("cannot instantiate non-class");
            }
            ObjClass *klass = as_class(R[ZEN_B(i)]);
            ObjInstance *inst = new_instance(&gc_, klass);
            R[ZEN_A(i)] = val_obj((Obj *)inst);
            NEXT();
        }
        CASE(OP_GETMETHOD)
        {
            /* R[A] = R[B].method(constants[C]) — bound method lookup */
            uint32_t i = *ip;
            Value receiver = R[ZEN_B(i)];
            ObjString *name = as_string(K[ZEN_C(i)]);
            if (is_instance(receiver))
            {
                ObjInstance *inst = as_instance(receiver);
                ObjClass *klass = inst->klass;
                bool found;
                Value method = map_get(klass->methods, val_obj((Obj *)name), &found);
                if (found)
                {
                    R[ZEN_A(i)] = method;
                    NEXT();
                }
            }
            else if (is_class(receiver))
            {
                ObjClass *klass = as_class(receiver);
                bool found;
                Value method = map_get(klass->methods, val_obj((Obj *)name), &found);
                if (found)
                {
                    R[ZEN_A(i)] = method;
                    NEXT();
                }
            }
            RT_ERROR("no method '%s' found", name->chars);
        }

        CASE(OP_CLASSFIELD)
        {
            /* as_class(R[A]).field_names[B] = as_string(K[C]) — pre-register field name */
            uint32_t i = *ip;
            ObjClass *klass = as_class(R[ZEN_A(i)]);
            int field_idx = ZEN_B(i);
            ObjString *fname = as_string(K[ZEN_C(i)]);
            if (field_idx >= klass->num_fields)
            {
                int old_n = klass->num_fields;
                int new_n = field_idx + 1;
                klass->field_names = (ObjString **)zen_realloc(
                    &gc_, klass->field_names,
                    sizeof(ObjString *) * old_n,
                    sizeof(ObjString *) * new_n);
                for (int fi = old_n; fi < new_n; fi++)
                    klass->field_names[fi] = nullptr;
                klass->num_fields = new_n;
            }
            klass->field_names[field_idx] = fname;
            NEXT();
        }

        CASE(OP_CLASSFIELDDEF)
        {
            /* as_class(R[A]).field_defaults[B] = K[C] — the value a class
            ** body assignment gave a field. Emitted right after the
            ** OP_CLASSFIELD run that registered the names, so the field
            ** index is already valid here. */
            uint32_t i = *ip;
            ObjClass *klass = as_class(R[ZEN_A(i)]);
            int field_idx = ZEN_B(i);
            if (field_idx >= klass->num_field_defaults)
            {
                int old_n = klass->num_field_defaults;
                int new_n = field_idx + 1;
                klass->field_defaults = (Value *)zen_realloc(
                    &gc_, klass->field_defaults,
                    sizeof(Value) * old_n,
                    sizeof(Value) * new_n);
                for (int fi = old_n; fi < new_n; fi++)
                    klass->field_defaults[fi] = val_nil();
                klass->num_field_defaults = new_n;
            }
            klass->field_defaults[field_idx] = K[ZEN_C(i)];
            NEXT();
        }

        /* --- Misc --- */
        CASE(OP_CONCAT)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];

            /* Fast path: both are strings already */
            if (is_string(vb) && is_string(vc))
            {
                R[ZEN_A(i)] = val_obj((Obj *)new_string_concat(&gc_, as_string(vb), as_string(vc)));
                NEXT();
            }

            /* Slow path: convert to strings then concat */
            char buf_b[64], buf_c[64];
            const char *sb;
            int lb;
            const char *sc;
            int lc;
            (void)buf_b;
            (void)buf_c;
            (void)sb;
            (void)lb;
            (void)sc;
            (void)lc;

            if (is_string(vb))
            {
                sb = safe_string_chars(vb);
                lb = safe_string_len(vb);
            }
            else if (is_int(vb))
            {
                lb = int_to_cstr(vb.as.integer, buf_b);
                sb = buf_b;
            }
            else if (is_float(vb))
            {
                lb = snprintf(buf_b, sizeof(buf_b), "%g", vb.as.number);
                sb = buf_b;
            }
            else if (is_bool(vb))
            {
                sb = vb.as.boolean ? "True" : "False";
                lb = vb.as.boolean ? 4 : 5;
            }
            else if (is_nil(vb))
            {
                sb = "None";
                lb = 4;
            }
            else
            {
                sb = "<obj>";
                lb = 5;
            }

            if (is_string(vc))
            {
                sc = safe_string_chars(vc);
                lc = safe_string_len(vc);
            }
            else if (is_int(vc))
            {
                lc = int_to_cstr(vc.as.integer, buf_c);
                sc = buf_c;
            }
            else if (is_float(vc))
            {
                lc = snprintf(buf_c, sizeof(buf_c), "%g", vc.as.number);
                sc = buf_c;
            }
            else if (is_bool(vc))
            {
                sc = vc.as.boolean ? "True" : "False";
                lc = vc.as.boolean ? 4 : 5;
            }
            else if (is_nil(vc))
            {
                sc = "None";
                lc = 4;
            }
            else
            {
                sc = "<obj>";
                lc = 5;
            }

            
 
            ObjString *result = new_string_concat(&gc_, (ObjString*)R[ZEN_B(i)].as.obj, (ObjString*)R[ZEN_C(i)].as.obj);
            R[ZEN_A(i)] = val_obj((Obj *)result);
            NEXT();
        }

        CASE(OP_STRADD)
        {
            uint32_t i = *ip;
            uint8_t a = ZEN_A(i);
            Value va = R[a], vb = R[ZEN_B(i)];
            if (is_string(va) && is_string(vb))
            {
                /* OP_STRADD is dead code (never emitted), but use safe path */
                ObjString *result = new_string_concat(&gc_, as_string(va), as_string(vb));
                R[a] = val_obj((Obj *)result);
            }
            else if (is_string(va) || is_string(vb))
            {
                /* At least one is string — coerce the other and concat */
                char buf[64], buf2[64];
                const char *sa;
                int la;
                const char *sb;
                int lb;
                if (is_string(va))
                {
                    sa = safe_string_chars(va);
                    la = safe_string_len(va);
                }
                else if (is_int(va))
                {
                    la = int_to_cstr(va.as.integer, buf);
                    sa = buf;
                }
                else if (is_float(va))
                {
                    la = snprintf(buf, sizeof(buf), "%g", va.as.number);
                    sa = buf;
                }
                else
                {
                    sa = "nil";
                    la = 3;
                }

                if (is_string(vb))
                {
                    sb = safe_string_chars(vb);
                    lb = safe_string_len(vb);
                }
                else if (is_int(vb))
                {
                    lb = int_to_cstr(vb.as.integer, buf2);
                    sb = buf2;
                }
                else if (is_float(vb))
                {
                    lb = snprintf(buf2, sizeof(buf2), "%g", vb.as.number);
                    sb = buf2;
                }
                else
                {
                    sb = "nil";
                    lb = 3;
                }

                ObjString *result = new_string_uninit(&gc_, la + lb);
                memcpy(result->chars, sa, la);
                memcpy(result->chars + la, sb, lb);
                result->obj.hash = hash_string(result->chars, la + lb);
                R[a] = val_obj((Obj *)result);
            }
            else
            {
                /* Both numeric — regular addition */
                if (is_int(va) && is_int(vb))
                    R[a] = val_int(va.as.integer + vb.as.integer);
                else
                {
                    double da = is_int(va) ? (double)va.as.integer : va.as.number;
                    double db = is_int(vb) ? (double)vb.as.integer : vb.as.number;
                    R[a] = val_float(da + db);
                }
            }
            NEXT();
        }

        CASE(OP_TOSTRING)
        {
            uint32_t i = *ip;
            uint8_t dst = ZEN_A(i);
            Value v = R[ZEN_B(i)];
            if (is_instance(v) && instance_has_method_slot(v, SLOT_STR))
            {
                Value result;
                SAVE_IP();
                if (!try_string_operator(this, v, &result))
                {
                    LOAD_STATE();
                    R[dst] = default_to_string(&gc_, v);
                    NEXT();
                }
                if (had_error_)
                    return;
                LOAD_STATE();
                if (!is_string(result))
                {
                    RT_ERROR("__str__ must return a string");
                }
                R[dst] = result;
            }
            else
            {
                R[dst] = default_to_string(&gc_, v);
            }
            NEXT();
        }

        CASE(OP_TOSTRING_OBJ)
        {
            uint32_t i = *ip;
            uint8_t dst = ZEN_A(i);
            Value result;
            SAVE_IP();
            if (!try_string_operator(this, R[ZEN_B(i)], &result))
            {
                LOAD_STATE();
                RT_ERROR("object does not implement __str__");
            }
            if (had_error_)
                return;
            LOAD_STATE();
            if (!is_string(result))
            {
                RT_ERROR("__str__ must return a string");
            }
            R[dst] = result;
            NEXT();
        }

        CASE(OP_LEN)
        {
            uint32_t i = *ip;
            Value v = R[ZEN_B(i)];
            if (is_string(v))
                R[ZEN_A(i)] = val_int(safe_string_len(v));
            else if (is_array(v))
                R[ZEN_A(i)] = val_int(arr_count(as_array(v)));
            else if (is_map(v))
                R[ZEN_A(i)] = val_int(as_map(v)->count);
            else if (is_set(v))
                R[ZEN_A(i)] = val_int(as_set(v)->count);
            else if (is_buffer(v))
                R[ZEN_A(i)] = val_int(as_buffer(v)->count);
            else if (is_range(v))
            {
                ObjRange *r = as_range(v);
                int64_t len;
                if (r->step > 0)
                    len = (r->stop > r->start) ? (r->stop - r->start + r->step - 1) / r->step : 0;
                else
                    len = (r->start > r->stop) ? (r->start - r->stop + (-r->step) - 1) / (-r->step) : 0;
                R[ZEN_A(i)] = val_int(len);
            }
            else
                R[ZEN_A(i)] = val_int(0);
            NEXT();
        }

        CASE(OP_PRINT)
        {
            uint32_t i = *ip;
            if (!ZEN_C(i))
            { /* C=0: normal print value */
                Value v = R[ZEN_A(i)];
                if (is_instance(v) && instance_has_method_slot(v, SLOT_STR))
                {
                    Value result;
                    SAVE_IP();
                    if (!try_string_operator(this, v, &result))
                    {
                        LOAD_STATE();
                        print_value_py(v, false);
                        goto print_end;
                    }
                    if (had_error_)
                        return;
                    LOAD_STATE();
                    if (!is_string(result))
                    {
                        RT_ERROR("__str__ must return a string");
                    }
                    ObjString *s = as_string(result);
                    zen_write(s->chars, (size_t)s->length);
                }
                else
                {
                    print_value_py(v, false);
                }
            }
        print_end:
            if (ZEN_B(i))
                zen_writeln();
            else
                zen_writes(" ");
            NEXT();
        }

        /* --- Fused comparison + jump superinstructions (2-word) --- */
        CASE(OP_LTJMPIFNOT)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            bool less;
            if (vb.type == VAL_INT && vc.type == VAL_INT)
                less = vb.as.integer < vc.as.integer;
            else
                less = to_number(vb) < to_number(vc);
            ++ip; /* advance to the sBx word */
            if (!less)
                ip += ZEN_SBX(*ip);
            NEXT();
        }

        CASE(OP_LEJMPIFNOT)
        {
            uint32_t i = *ip;
            Value vb = R[ZEN_B(i)], vc = R[ZEN_C(i)];
            bool le;
            if (vb.type == VAL_INT && vc.type == VAL_INT)
                le = vb.as.integer <= vc.as.integer;
            else
                le = to_number(vb) <= to_number(vc);
            ++ip; /* advance to the sBx word */
            if (!le)
                ip += ZEN_SBX(*ip);
            NEXT();
        }

        CASE(OP_FORPREP)
        {
            /* R[A]=counter, R[A+1]=limit, R[A+2]=step
               Subtract step so first FORLOOP increments to start value.
               Then jump to FORLOOP for initial test. */
            uint32_t i = *ip;
            int a = ZEN_A(i);
            R[a].as.integer -= R[a + 2].as.integer;
            ip += ZEN_SBX(i); /* jump to FORLOOP */
            NEXT();
        }

        CASE(OP_FORLOOP)
        {
            /* R[A] += R[A+2]; if still in range: pc += sBx (back to body) */
            uint32_t i = *ip;
            int a = ZEN_A(i);
            int32_t counter = R[a].as.integer + R[a + 2].as.integer;
            int32_t limit = R[a + 1].as.integer;
            R[a].as.integer = counter;
            if (counter < limit)
                ip += ZEN_SBX(i); /* loop back */
            NEXT();
        }

        /* --- Fused field+arith superinstructions (2-word) --- */
        CASE(OP_GETFIELD_MUL)
        {
            /* word1: GETFIELD_IDX  R[A] = R[B].fields[C]
               word2: MUL           R[A] = R[B] * R[C]    */
            uint32_t i1 = *ip;
            ObjInstance *inst = as_instance(R[ZEN_B(i1)]);
            R[ZEN_A(i1)] = inst->fields[ZEN_C(i1)];
            ++ip;
            uint32_t i2 = *ip;
            Value vb = R[ZEN_B(i2)], vc = R[ZEN_C(i2)];
            if (vb.type == VAL_INT && vc.type == VAL_INT)
                R[ZEN_A(i2)] = val_int((int64_t)((uint64_t)vb.as.integer * (uint64_t)vc.as.integer));
            else
                R[ZEN_A(i2)] = val_float(to_number(vb) * to_number(vc));
            NEXT();
        }

        CASE(OP_GETFIELD_SUB)
        {
            /* word1: GETFIELD_IDX  R[A] = R[B].fields[C]
               word2: SUB           R[A] = R[B] - R[C]    */
            uint32_t i1 = *ip;
            ObjInstance *inst = as_instance(R[ZEN_B(i1)]);
            R[ZEN_A(i1)] = inst->fields[ZEN_C(i1)];
            ++ip;
            uint32_t i2 = *ip;
            Value vb = R[ZEN_B(i2)], vc = R[ZEN_C(i2)];
            if (vb.type == VAL_INT && vc.type == VAL_INT)
                R[ZEN_A(i2)] = val_int(vb.as.integer - vc.as.integer);
            else
                R[ZEN_A(i2)] = val_float(to_number(vb) - to_number(vc));
            NEXT();
        }

        CASE(OP_EVAL)
        {
            uint32_t i = *ip;
            int a = ZEN_A(i);
            int b = ZEN_B(i);
            ++ip;
            SAVE_IP();

            Value src = R[b];
            if (!is_string(src))
            {
                RT_ERROR("eval() expects a string, got %s", val_type_str(src));
            }
            ObjString *code_str = as_string(src);

            /* Compile: try EVAL_MODE (single expression) first, silently.
            ** Fall back to full script mode (statements) if parse fails. */
            Compiler comp;
            ObjFunc *fn = comp.compile_eval(&gc_, this, code_str->chars, "<eval>", /*silent=*/true);
            if (!fn)
            {
                Compiler comp2;
                fn = comp2.compile(&gc_, this, code_str->chars, "<eval>");
            }
            if (!fn)
            {
                RT_ERROR("eval: compile error");
            }

            /* Root fn against GC by placing it in R[a] before any further allocation */
            R[a] = val_obj((Obj *)fn);

            /* Wrap fn in a no-upvalue closure */
            ObjClosure *cl = (ObjClosure *)zen_alloc_now(&gc_, sizeof(ObjClosure));
            cl->obj.type = OBJ_CLOSURE;
            cl->obj.color = GC_BLACK;
            cl->obj.interned = 0;
            cl->obj.hash = 0;
            cl->obj.gc_next = gc_.objects;
            gc_.objects = (Obj *)cl;
            cl->func = fn;
            cl->upvalues = nullptr;
            cl->upvalue_count = 0;

            /* Push call frame: 0 args, result → R[a] */
            if (fiber->frame_count >= kMaxFrames)
            {
                RT_ERROR("stack overflow");
            }
            CHECK_STACK_SPACE(fiber, &R[a + 1], fn->num_regs);
            CallFrame *new_frame = &fiber->frames[fiber->frame_count++];
            new_frame->closure = cl;
            new_frame->func = fn;
            new_frame->ip = fn->code;
            new_frame->base = &R[a + 1]; /* args start after result slot */
            new_frame->ret_reg = a;
            new_frame->ret_count = 1;
            fiber->stack_top = new_frame->base + fn->num_regs;
            clear_new_regs(new_frame->base, 0, fn->num_regs);
            LOAD_STATE();
            DISPATCH();
        }

        CASE(OP_ASSERT)
        {
            uint32_t i = *ip;
            if (!is_truthy_full(R[ZEN_A(i)]))
            {
                Value msg = R[ZEN_B(i)];
                if (is_string(msg))
                    RT_ERROR("AssertionError: %s", as_string(msg)->chars);
                else
                    RT_ERROR("AssertionError");
            }
            NEXT();
        }

        CASE(OP_HALT)
        {
            SAVE_IP();
            fiber->state = FIBER_DONE;
            return;
        }

        CASE(OP_IMPORT)
        {
            /* R[A] = module ObjMap for lib named constants[Bx]
            ** The map is cached as a global with the module name. */
            uint32_t i = *ip;
            ObjString *mod_name = as_string(K[ZEN_BX(i)]);
            int dest = ZEN_A(i);

            /* Check if already imported (global with same name exists as map) */
            int gidx = find_global(mod_name->chars);
            if (gidx >= 0 && is_map(globals_[gidx]))
            {
                R[dest] = globals_[gidx];
                NEXT();
            }

            /* Find lib in registry */
            const NativeLib *lib = find_lib(mod_name->chars);
            if (!lib)
            {
                /* Try loading as a .py script module */
                ObjMap *script_mod = import_script_module(mod_name->chars);
                if (script_mod)
                {
                    R[dest] = val_obj((Obj *)script_mod);
                    /* Cache as global */
                    if (gidx < 0)
                        gidx = def_global(mod_name->chars, R[dest]);
                    else
                        globals_[gidx] = R[dest];
                    NEXT();
                }
                RT_ERROR("module '%s' not found", mod_name->chars);
            }

            /* Build ObjMap: name_string → Value(native) */
            ObjMap *mod = new_map(&gc_);
            mod->is_module = true;
            /* Temporarily root it so GC doesn't collect it during construction */
            R[dest] = val_obj((Obj *)mod);

            /* Run init_fn first (may populate constants array) */
            if (lib->init_fn)
            {
                gc_pause(&gc_);
                lib->init_fn(this);
                gc_resume(&gc_);
            }

            gc_pause(&gc_);
            for (int fi = 0; fi < lib->num_functions; fi++)
            {
                const char *fname = lib->functions[fi].name;
                int flen = (int)strlen(fname);
                ObjString *key = intern_string(&gc_, fname, flen, hash_string(fname, flen));
                ObjNative *nat = new_native(&gc_, lib->functions[fi].fn,
                                            lib->functions[fi].arity, key);
                map_set(&gc_, as_map(R[dest]), val_obj((Obj *)key), val_obj((Obj *)nat));
            }
            for (int ci = 0; ci < lib->num_constants; ci++)
            {
                const char *cname = lib->constants[ci].name;
                int clen = (int)strlen(cname);
                ObjString *key = intern_string(&gc_, cname, clen, hash_string(cname, clen));
                map_set(&gc_, as_map(R[dest]), val_obj((Obj *)key), lib->constants[ci].value);
            }
            gc_resume(&gc_);

            /* Cache as global with module name */
            if (gidx < 0)
                gidx = def_global(mod_name->chars, R[dest]);
            else
                globals_[gidx] = R[dest];

            NEXT();
        }

        /* =================================================================
        **  DISPATCH CLEANUP
        ** ================================================================= */

#ifdef ZEN_COMPUTED_GOTO
        /* Computed goto não precisa de close */
#else
        } /* switch */
    } /* for */
#endif

#undef DISPATCH
#undef CASE
#undef NEXT
#undef LOAD_STATE
#undef SAVE_IP
#undef NUM_BINOP
    }

} /* namespace zen */
