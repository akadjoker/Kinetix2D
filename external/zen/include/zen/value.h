#ifndef ZEN_VALUE_H
#define ZEN_VALUE_H

#include "common.h"

namespace zen
{

    /*
    ** Value — tagged union, 16 bytes.
    **
    ** Tipos imediatos (vivem dentro do Value, sem GC):
    **   NIL, BOOL, INT, FLOAT
    **
    ** Tipos heap (apontam para GCObject, geridos pelo GC):
    **   OBJ → ObjString, ObjFunc, ObjArray, ObjMap, ObjClass, ObjInstance
    **
    */

    enum ValueType : uint8_t
    {
        VAL_NIL,
        VAL_BOOL,
        VAL_INT,
        VAL_FLOAT,
        VAL_OBJ, /* qualquer objecto no heap (string, func, array...) */
        VAL_PTR, /* raw pointer (não gerido pelo GC) */
        VAL_SMALL_STRING, /* EXPERIMENTAL: string <= 7 bytes, stored inline (no GC) */
    };

    struct Value
    {
        ValueType type;
        union
        {
            bool boolean;
            int64_t integer;
            double number;
            Obj *obj;
            void *pointer;
            /* EXPERIMENTAL SSO: small string optimization (1-7 bytes inline) */
            struct
            {
                uint8_t len;               /* 1-7 bytes */
                char chars[7];             /* string data */
                uint8_t _pad;              /* padding */
            } sso;
        } as;
    };

    /* Constructores inline — sem overhead */
    inline Value val_nil()
    {
        Value v;
        v.type = VAL_NIL;
        return v;
    }
    inline Value val_bool(bool b)
    {
        Value v;
        v.type = VAL_BOOL;
        v.as.boolean = b;
        return v;
    }
    inline Value val_int(int64_t i)
    {
        Value v;
        v.type = VAL_INT;
        v.as.integer = i;
        return v;
    }
    inline Value val_float(double d)
    {
        Value v;
        v.type = VAL_FLOAT;
        v.as.number = d;
        return v;
    }
    inline Value val_obj(Obj *o)
    {
        Value v;
        v.type = VAL_OBJ;
        v.as.obj = o;
        return v;
    }
    inline Value val_ptr(void *p)
    {
        Value v;
        v.type = VAL_PTR;
        v.as.pointer = p;
        return v;
    }

    /* EXPERIMENTAL SSO: small string creation (1-7 bytes inline, no GC) */
    inline Value val_small_string(const char *chars, int len)
    {
        Value v;
        v.type = VAL_SMALL_STRING;
        v.as.sso.len = len;
        for (int i = 0; i < len; i++)
            v.as.sso.chars[i] = chars[i];
        return v;
    }

    /* EXPERIMENTAL SSO: safely get small string data */
    inline bool is_small_string(const Value &v) { return v.type == VAL_SMALL_STRING; }
    inline int small_string_len(const Value &v)
    {
        return is_small_string(v) ? v.as.sso.len : 0;
    }
    inline const char *small_string_chars(const Value &v)
    {
        return is_small_string(v) ? v.as.sso.chars : nullptr;
    }

    /* Type checks */
    inline bool is_nil(Value v) { return v.type == VAL_NIL; }
    inline bool is_bool(Value v) { return v.type == VAL_BOOL; }
    inline bool is_int(Value v) { return v.type == VAL_INT; }
    inline bool is_float(Value v) { return v.type == VAL_FLOAT; }
    inline bool is_obj(Value v) { return v.type == VAL_OBJ; }
    inline bool is_ptr(Value v) { return v.type == VAL_PTR; }
    inline void *as_ptr(Value v) { return v.as.pointer; }

    bool objects_equal(Obj *a, Obj *b);

    /* Truthiness — nil e false são falsy, tudo o resto truthy */
    inline bool is_truthy(Value v)
    {
        switch (v.type)
        {
        case VAL_NIL:   return false;
        case VAL_BOOL:  return v.as.boolean;
        case VAL_INT:   return v.as.integer != 0;
        case VAL_FLOAT: return v.as.number != 0.0;
        case VAL_SMALL_STRING: return v.as.sso.len != 0;
        default:        return true; /* objects: refined in is_truthy_full() */
        }
    }

    /* Conversão numérica */
    inline double to_number(Value v)
    {
        if (v.type == VAL_INT)
            return (double)v.as.integer;
        if (v.type == VAL_FLOAT)
            return v.as.number;
        return 0.0;
    }

    /* Conversão para inteiro (bitwise ops) */
    inline int64_t to_integer(Value v)
    {
        if (v.type == VAL_INT)
            return v.as.integer;
        if (v.type == VAL_FLOAT)
            return (int64_t)v.as.number;
        if (v.type == VAL_BOOL)
            return v.as.boolean ? 1 : 0;
        return 0;
    }

    /* Comparação de igualdade — fast path for int (most common in tables) */
    inline bool values_equal(Value a, Value b)
    {
        if (a.type == b.type) {
            if (__builtin_expect(a.type == VAL_INT, 1))
                return a.as.integer == b.as.integer;
            switch (a.type)
            {
            case VAL_NIL:
                return true;
            case VAL_BOOL:
                return a.as.boolean == b.as.boolean;
            case VAL_INT:
                __builtin_unreachable();
            case VAL_FLOAT:
                return a.as.number == b.as.number;
            case VAL_OBJ:
                return objects_equal(a.as.obj, b.as.obj);
            case VAL_PTR:
                return a.as.pointer == b.as.pointer;
            case VAL_SMALL_STRING:
            {
                if (a.as.sso.len != b.as.sso.len)
                    return false;
                for (int i = 0; i < a.as.sso.len; i++)
                    if (a.as.sso.chars[i] != b.as.sso.chars[i])
                        return false;
                return true;
            }
            }
        }
        /* Mixed int/float: compare numerically (like Lua) */
        if (a.type == VAL_INT && b.type == VAL_FLOAT) {
            return (double)a.as.integer == b.as.number;
        }
        if (a.type == VAL_FLOAT && b.type == VAL_INT) {
            return a.as.number == (double)b.as.integer;
        }
        return false;
    }

    /* Deep equality for == operator (compares array/map contents recursively) */
    bool values_deep_equal(Value a, Value b);

} /* namespace zen */

#endif /* ZEN_VALUE_H */
