#include "memory.h"
#include "name_tables.h"
#include "vm.h"
#include <cmath>

namespace zen
{
    
    /* Forward declare for GC trigger */
    void gc_collect(VM *vm);

    /* =========================================================
    ** Allocator base — wrapper fino sobre malloc/realloc/free.
    ** Contabiliza bytes para trigger do GC.
    ** ========================================================= */

    void *zen_alloc(GC *gc, size_t size)
    {
#ifdef ZEN_DEBUG_STRESS_GC
        if (gc->pause_depth == 0 && gc->vm)
            gc_collect((VM *)gc->vm);
#else
        if (gc->pause_depth == 0 && gc->vm && gc->bytes_allocated > gc->next_gc)
            gc_collect((VM *)gc->vm);
#endif
        gc->bytes_allocated += size;
        return arena_alloc(&gc->arena, size);
    }
    void *zen_alloc_now(GC *gc, size_t size)
    {
        gc->bytes_allocated += size;
        return arena_alloc(&gc->arena, size);
    }

    void *zen_realloc(GC *gc, void *ptr, size_t old_size, size_t new_size)
    {
#ifdef ZEN_DEBUG_STRESS_GC
        if (gc->pause_depth == 0 && gc->vm)
            gc_collect((VM *)gc->vm);
#else
        if (gc->pause_depth == 0 && gc->vm && gc->bytes_allocated > gc->next_gc)
            gc_collect((VM *)gc->vm);
#endif
        gc->bytes_allocated += new_size - old_size;
        return arena_realloc(&gc->arena, ptr, old_size, new_size);
    }

    void zen_free(GC *gc, void *ptr, size_t size)
    {
        gc->bytes_allocated -= size;
        arena_free(&gc->arena, ptr, size);
    }

    bool objects_equal(Obj *a, Obj *b)
    {
        if (a == b)
            return true;
        if (!a || !b || a->type != b->type)
            return false;

        if (a->type == OBJ_STRING)
        {
            ObjString *sa = (ObjString *)a;
            ObjString *sb = (ObjString *)b;
            return sa->length == sb->length &&
                   memcmp(sa->chars, sb->chars, (size_t)sa->length) == 0;
        }

        return false;
    }

    // find_sep — substitui memmem, portável


    /* =========================================================
    ** GC Init
    ** ========================================================= */

    void gc_init(GC *gc)
    {
        gc->objects = nullptr;
        gc->gray_list = nullptr;
        gc->gray_count = 0;
        gc->gray_capacity = 0;
        gc->bytes_allocated = 0;
        gc->next_gc = kGCInitThreshold;
        gc->pause_saved_next_gc = kGCInitThreshold;
        gc->pause_depth = 0;
        gc->vm = nullptr;
        gc->extra_mark = nullptr;
        gc->extra_mark_ud = nullptr;

        /* Pool allocator */
        arena_init(&gc->arena);

        /* String interning table — começa com 64 slots */
        gc->string_capacity = 64;
        gc->string_count = 0;
        gc->strings = (ObjString **)calloc(gc->string_capacity, sizeof(ObjString *));
        gc->bytes_allocated += sizeof(ObjString *) * gc->string_capacity;
    }

    void gc_pause(GC *gc)
    {
        if (!gc)
            return;
        if (gc->pause_depth++ == 0)
        {
            gc->pause_saved_next_gc = gc->next_gc;
            gc->next_gc = (size_t)-1;
        }
    }

    void gc_resume(GC *gc)
    {
        if (!gc || gc->pause_depth <= 0)
            return;
        if (--gc->pause_depth == 0)
            gc->next_gc = gc->pause_saved_next_gc;
    }

    /* =========================================================
    ** Alocação de objectos — regista na linked list do GC.
    ** ========================================================= */

    static Obj *alloc_obj(GC *gc, size_t size, ObjType type)
    {
        /* Use zen_alloc_now to avoid triggering GC during object construction.
           Objects are born BLACK and added to the GC list, but they are not yet
           reachable from any root — a GC cycle here would reset them to WHITE
           and sweep them immediately. The GC will naturally run at the next
           zen_alloc/zen_realloc call after the object is rooted. */
        Obj *obj = (Obj *)zen_alloc_now(gc, size);
        obj->type = type;
        obj->color = GC_BLACK; /* born BLACK: survives current GC cycle */
        obj->interned = 0;
        obj->hash = 0;
        obj->gc_next = gc->objects;
        gc->objects = obj;
        return obj;
    }
    // static Obj *alloc_obj_now(GC *gc, size_t size, ObjType type)
    // {
    //     Obj *obj = (Obj *)zen_alloc_now(gc, size);  // não dispara GC
    //     obj->type = type;
    //     obj->color = GC_BLACK;
    //     obj->interned = 0;
    //     obj->hash = 0;
    //     obj->gc_next = gc->objects;
    //     gc->objects = obj;
    //     return obj;
    // }

    /* =========================================================
    ** Strings — interned, hash pré-calculado, buffer inline.
    ** ========================================================= */

    ObjString *find_interned(GC *gc, const char *chars, int length, uint32_t hash)
    {
        if (gc->string_count == 0)
            return nullptr;

        uint32_t idx = hash & (gc->string_capacity - 1);
        for (;;)
        {
            ObjString *s = gc->strings[idx];
            if (s == nullptr)
                return nullptr;
            if (s->obj.hash == hash && s->length == length &&
                memcmp(s->chars, chars, length) == 0)
            {
                return s;
            }
            idx = (idx + 1) & (gc->string_capacity - 1);
        }
    }

    static void intern_table_grow(GC *gc)
    {
        int new_cap = gc->string_capacity * 2;
        size_t old_bytes = sizeof(ObjString *) * gc->string_capacity;
        size_t new_bytes = sizeof(ObjString *) * new_cap;
        ObjString **new_table = (ObjString **)calloc(new_cap, sizeof(ObjString *));
        gc->bytes_allocated += new_bytes;

        /* re-hash tudo */
        for (int i = 0; i < gc->string_capacity; i++)
        {
            ObjString *s = gc->strings[i];
            if (s == nullptr)
                continue;
            uint32_t idx = s->obj.hash & (new_cap - 1);
            while (new_table[idx] != nullptr)
                idx = (idx + 1) & (new_cap - 1);
            new_table[idx] = s;
        }

        gc->bytes_allocated -= old_bytes;
        free(gc->strings);
        gc->strings = new_table;
        gc->string_capacity = new_cap;
    }

    static ObjString *alloc_string_raw(GC *gc, const char *chars, int length, uint32_t hash)
    {
        size_t total = sizeof(ObjString) + length + 1;
        ObjString *str = (ObjString *)alloc_obj(gc, total, OBJ_STRING);
        str->length = length;
        str->capacity = 0; /* tight allocation: obj_size uses length + 1 */
        memcpy(str->chars, chars, length);
        str->chars[length] = '\0';
        str->obj.hash = hash;
        return str;
    }

    ObjString *intern_string(GC *gc, const char *chars, int length, uint32_t hash)
    {
        /* Já existe? */
        ObjString *existing = find_interned(gc, chars, length, hash);
        if (existing)
            return existing;

        /* Grow se >75% — multiplication avoids division */
        if ((gc->string_count + 1) * 4 > gc->string_capacity * 3)
        {
            intern_table_grow(gc);
        }

        ObjString *str = alloc_string_raw(gc, chars, length, hash);
        str->obj.interned = 1;

        /* Insere na tabela */
        uint32_t idx = hash & (gc->string_capacity - 1);
        while (gc->strings[idx] != nullptr)
            idx = (idx + 1) & (gc->string_capacity - 1);
        gc->strings[idx] = str;
        gc->string_count++;

        return str;
    }

    ObjString *new_string_uninit(GC *gc, int length)
    {
        size_t total = sizeof(ObjString) + length + 1;
        ObjString *str = (ObjString *)alloc_obj(gc, total, OBJ_STRING);
        str->length = length;
        str->capacity = 0;
        str->chars[length] = '\0';
        str->obj.hash = 0; // caller define depois
        return str;
    }

    ObjString *new_string(GC *gc, const char *chars, int length)
    {
        uint32_t hash = hash_string(chars, length);
        return intern_string(gc, chars, length, hash);   
    }

    /* EXPERIMENTAL SSO: Create string as Value (small strings stored inline) */
    Value new_string_sso(GC *gc, const char *chars, int length)
    {
        /* Strings 1-7 bytes: store inline (no GC pressure) */
        if (length > 0 && length <= 7)
            return val_small_string(chars, length);
        
        /* Strings 0 or >= 8 bytes: allocate on heap as usual */
        ObjString *str = new_string(gc, chars, length);
        return val_obj((Obj *)str);
    }

    ObjString *create_string(GC *gc, const char *chars, int length)
    {
        uint32_t hash = hash_string(chars, length);
        return alloc_string_raw(gc, chars, length, hash);
    }

    ObjString *new_string_concat(GC *gc, ObjString *a, ObjString *b)
    {
        if (a->length == 0) return b;
        if (b->length == 0) return a;

        int64_t length64 = (int64_t)a->length + b->length;
        if (length64 > 0x7FFFFFFF)
        {
            fprintf(stderr, "[zen] string concatenation too large\n");
            return b; /* fallback — avoid crash */
        }
        int length = (int)length64;
        int new_cap = (length + 1) * 2;
        new_cap = new_cap < 64 ? 64 : new_cap;
        new_cap = (new_cap + 7) & ~7;

        size_t total = sizeof(ObjString) + new_cap;

        
        ObjString *str = (ObjString *)zen_alloc_now(gc, total);
        str->obj.type = OBJ_STRING;
        str->obj.color = GC_BLACK;
        str->obj.interned = 0;
        str->obj.hash = 0;
        str->obj.gc_next = gc->objects;
        gc->objects = (Obj *)str;

        str->length = length;
        str->capacity = new_cap;
        memcpy(str->chars, a->chars, a->length);
        memcpy(str->chars + a->length, b->chars, b->length);
        str->chars[length] = '\0';
        str->obj.hash = hash_string(str->chars, length);
        return str;
    }

    // ObjString *string_append_inplace(GC *gc, ObjString *a, ObjString *b)
    // {
    //     /* Can we realloc in-place? Only if NOT interned */
    //     if (a->obj.interned || a->length == 0)
    //         return new_string_concat(gc, a, b);
    //     if (b->length == 0)
    //         return a;

    //     int new_len = a->length + b->length;
    //     int needed = new_len + 1;

    //     if (a->capacity >= needed)
    //     {
    //         /* Enough room — just append */
    //         memcpy(a->chars + a->length, b->chars, b->length);
    //         a->length = new_len;
    //         a->chars[new_len] = '\0';
    //         a->obj.hash = 0; /* invalidate hash */
    //         return a;
    //     }

    //     /* Grow with geometric strategy (1.5x or needed, whichever is larger) */
    //     int new_cap = a->capacity + (a->capacity >> 1);
    //     if (new_cap < needed)
    //         new_cap = needed;
    //     /* Round up to 8-byte boundary */
    //     new_cap = (new_cap + 7) & ~7;

    //     int old_cap = a->capacity > 0 ? a->capacity : a->length + 1;
    //     size_t old_size = sizeof(ObjString) + old_cap;
    //     size_t new_size = sizeof(ObjString) + new_cap;

    //     /* Must fix gc_next linked list if realloc moves the pointer */
    //     ObjString *result = (ObjString *)zen_realloc(gc, a, old_size, new_size);
    //     if (result != a)
    //     {
    //         /* Pointer moved — patch GC linked list */
    //         Obj **pp = &gc->objects;
    //         while (*pp)
    //         {
    //             if (*pp == (Obj *)a)
    //             {
    //                 *pp = (Obj *)result;
    //                 break;
    //             }
    //             pp = &(*pp)->gc_next;
    //         }
    //     }
    //     memcpy(result->chars + result->length, b->chars, b->length);
    //     result->length = new_len;
    //     result->capacity = new_cap;
    //     result->chars[new_len] = '\0';
    //     result->obj.hash = 0;
    //     return result;
    // }
    ObjString *string_append_inplace(GC *gc, ObjString *a, ObjString *b)
{
    if (a->obj.interned || a->length == 0 || (a->obj.flags & OBJ_FLAG_SHARED))
        return new_string_concat(gc, a, b);
    if (b->length == 0)
        return a;

    int new_len = a->length + b->length;
    if (new_len < a->length) /* overflow check */
        return new_string_concat(gc, a, b);
    int needed = new_len + 1;

    if (a->capacity >= needed)
    {
        /* Fast path — espaço suficiente, zero alloc */
        memcpy(a->chars + a->length, b->chars, b->length);
        a->length = new_len;
        a->chars[new_len] = '\0';
        a->obj.hash = 0;
        return a;
    }

    /* Sem realloc — aloca nova string com capacidade generosa (2x needed) */
    /* Evita o patch O(n) da GC list completamente */
    int new_cap = needed * 2;
    new_cap = (new_cap + 7) & ~7;

    size_t total = sizeof(ObjString) + new_cap;
    ObjString *result = (ObjString *)alloc_obj(gc, total, OBJ_STRING);
    result->length = new_len;
    result->capacity = new_cap;
    memcpy(result->chars, a->chars, a->length);
    memcpy(result->chars + a->length, b->chars, b->length);
    result->chars[new_len] = '\0';
    result->obj.hash = 0;
    return result;
    /* 'a' fica orphan — GC trata dela no próximo sweep */
}

    /* =========================================================
    ** Criação de objectos compostos
    ** ========================================================= */

    ObjFunc *new_func(GC *gc)
    {
        ObjFunc *fn = (ObjFunc *)alloc_obj(gc, sizeof(ObjFunc), OBJ_FUNC);
        fn->arity = 0;
        fn->num_regs = 0;
        fn->code_count = 0;
        fn->code_capacity = 0;
        fn->const_count = 0;
        fn->const_capacity = 0;
        fn->upvalue_count = 0;
        fn->default_count = 0;
        fn->code = nullptr;
        fn->lines = nullptr;
        fn->constants = nullptr;
        fn->defaults = nullptr;
        fn->upval_descs = nullptr;
        fn->name = nullptr;
        fn->source = nullptr;
        fn->return_class = nullptr;
        fn->is_generator = false;
        return fn;
    }

    ObjNative *new_native(GC *gc, NativeFn fn, int arity, ObjString *name)
    {
        ObjNative *nat = (ObjNative *)alloc_obj(gc, sizeof(ObjNative), OBJ_NATIVE);
        nat->fn = fn;
        nat->arity = arity;
        nat->name = name;
        return nat;
    }

    ObjArray *new_array(GC *gc)
    {
        ObjArray *arr = (ObjArray *)alloc_obj(gc, sizeof(ObjArray), OBJ_ARRAY);
        arr->data = nullptr;
        arr->end = nullptr;
        arr->cap_end = nullptr;
        return arr;
    }

    ObjMap *new_map(GC *gc)
    {
        ObjMap *map = (ObjMap *)alloc_obj(gc, sizeof(ObjMap), OBJ_MAP);
        map->count = 0;
        map->capacity = 0;
        map->bucket_count = 0;
        map->free_head = -1;
        map->buckets = nullptr;
        map->nodes = nullptr;
        map->is_module = false;
        return map;
    }

    /* =========================================================
    ** Array Operations — end-pointer layout for minimal push cost.
    ** Growth: power-of-2, initial cap 8, doubles each time.
    ** ========================================================= */

    static inline int32_t grow_capacity(int32_t capacity, int32_t min_cap)
    {
        int32_t cap = capacity < min_cap ? min_cap : capacity;
        if (cap < 8)
            return 8;
        cap--;
        cap |= cap >> 1;
        cap |= cap >> 2;
        cap |= cap >> 4;
        cap |= cap >> 8;
        cap |= cap >> 16;
        cap++;
        return cap;
    }

    static inline void array_ensure_cap(GC *gc, ObjArray *arr, int32_t needed)
    {
        int32_t cur_cap = arr_capacity(arr);
        if (needed <= cur_cap)
            return;
        int32_t count = arr_count(arr);
        int32_t new_cap = grow_capacity(cur_cap, needed);
        gc_pause(gc);
        arr->data = (Value *)zen_realloc(gc, arr->data,
                                         sizeof(Value) * cur_cap,
                                         sizeof(Value) * new_cap);
        gc_resume(gc);
        arr->end = arr->data + count;
        arr->cap_end = arr->data + new_cap;
    }

    void array_push_slow(GC *gc, ObjArray *arr, Value val)
    {
        int32_t count = arr_count(arr);
        int32_t old_cap = arr_capacity(arr);
        int32_t new_cap = grow_capacity(old_cap, count + 1);
        gc_pause(gc);
        arr->data = (Value *)zen_realloc(gc, arr->data,
                                         sizeof(Value) * old_cap,
                                         sizeof(Value) * new_cap);
        gc_resume(gc);
        arr->end = arr->data + count;
        arr->cap_end = arr->data + new_cap;
        *arr->end++ = val;
        if (__builtin_expect(val.type == VAL_OBJ, 0))
            gc_write_barrier(gc, (Obj *)arr, val.as.obj);
    }

    void array_set(GC *gc, ObjArray *arr, int32_t index, Value val)
    {
        if ((uint32_t)index >= (uint32_t)arr_count(arr))
            return;
        arr->data[index] = val;
        if (is_obj(val))
            gc_write_barrier(gc, (Obj *)arr, val.as.obj);
    }

    Value array_pop(ObjArray *arr)
    {
        if (arr->end == arr->data)
            return val_nil();
        return *--arr->end;
    }

    void array_insert(GC *gc, ObjArray *arr, int32_t index, Value val)
    {
        int32_t count = arr_count(arr);
        if (index < 0)
            index = 0;
        if (index > count)
            index = count;
        array_ensure_cap(gc, arr, count + 1);
        /* memmove is overlap-safe */
        if (index < count)
        {
            memmove(arr->data + index + 1, arr->data + index,
                    (size_t)(count - index) * sizeof(Value));
        }
        arr->data[index] = val;
        arr->end++;
        if (is_obj(val))
            gc_write_barrier(gc, (Obj *)arr, val.as.obj);
    }

    void array_remove(ObjArray *arr, int32_t index)
    {
        int32_t count = arr_count(arr);
        if ((uint32_t)index >= (uint32_t)count)
            return;
        int32_t remaining = count - index - 1;
        if (remaining > 0)
        {
            memmove(arr->data + index, arr->data + index + 1,
                    (size_t)remaining * sizeof(Value));
        }
        arr->end--;
    }

    void array_clear(ObjArray *arr)
    {
        arr->end = arr->data;
    }

    void array_reserve(GC *gc, ObjArray *arr, int32_t cap)
    {
        int32_t cur_cap = arr_capacity(arr);
        if (cap <= cur_cap)
            return;
        int32_t count = arr_count(arr);
        gc_pause(gc);
        arr->data = (Value *)zen_realloc(gc, arr->data,
                                         sizeof(Value) * cur_cap,
                                         sizeof(Value) * cap);
        gc_resume(gc);
        arr->end = arr->data + count;
        arr->cap_end = arr->data + cap;
    }

    int32_t array_find(ObjArray *arr, Value val)
    {
        int32_t count = arr_count(arr);
        for (int32_t i = 0; i < count; i++)
        {
            if (values_equal(arr->data[i], val))
                return i;
        }
        return -1;
    }

    int32_t array_find_int(ObjArray *arr, int32_t target)
    {
        const Value *data = arr->data;
        const int32_t n = arr_count(arr);
        int32_t i = 0;
        for (; i + 8 <= n; i += 8)
        {
            __builtin_prefetch(data + i + 16, 0, 1);
            if (data[i + 0].as.integer == target)
                return i + 0;
            if (data[i + 1].as.integer == target)
                return i + 1;
            if (data[i + 2].as.integer == target)
                return i + 2;
            if (data[i + 3].as.integer == target)
                return i + 3;
            if (data[i + 4].as.integer == target)
                return i + 4;
            if (data[i + 5].as.integer == target)
                return i + 5;
            if (data[i + 6].as.integer == target)
                return i + 6;
            if (data[i + 7].as.integer == target)
                return i + 7;
        }
        for (; i < n; i++)
            if (data[i].as.integer == target)
                return i;
        return -1;
    }

    bool array_contains(ObjArray *arr, Value val)
    {
        return array_find(arr, val) >= 0;
    }

    void array_reverse(ObjArray *arr)
    {
        int32_t count = arr_count(arr);
        if (count < 2)
            return;
        Value *lo = arr->data;
        Value *hi = arr->end - 1;
        while (lo + 4 <= hi)
        {
            Value t0 = lo[0];
            lo[0] = hi[0];
            hi[0] = t0;
            Value t1 = lo[1];
            lo[1] = hi[-1];
            hi[-1] = t1;
            lo += 2;
            hi -= 2;
        }
        while (lo < hi)
        {
            Value t = *lo;
            *lo = *hi;
            *hi = t;
            lo++;
            hi--;
        }
    }

    /* Introsort: insertion sort for small partitions, quicksort for large */
    static void isort_int(Value *data, int32_t n)
    {
        for (int32_t i = 1; i < n; i++)
        {
            Value key = data[i];
            int32_t j = i - 1;
            while (j >= 0 && data[j].as.integer > key.as.integer)
            {
                data[j + 1] = data[j];
                j--;
            }
            data[j + 1] = key;
        }
    }

    static void qsort_int(Value *data, int32_t lo, int32_t hi)
    {
        while (lo < hi)
        {
            if (hi - lo < 16)
            {
                isort_int(data + lo, hi - lo + 1);
                return;
            }
            /* median-of-three pivot */
            int32_t mid = lo + (hi - lo) / 2;
            if (data[mid].as.integer < data[lo].as.integer)
            {
                Value t = data[lo];
                data[lo] = data[mid];
                data[mid] = t;
            }
            if (data[hi].as.integer < data[lo].as.integer)
            {
                Value t = data[lo];
                data[lo] = data[hi];
                data[hi] = t;
            }
            if (data[mid].as.integer < data[hi].as.integer)
            {
                Value t = data[mid];
                data[mid] = data[hi];
                data[hi] = t;
            }
            int32_t pivot = data[hi].as.integer;
            int32_t i = lo - 1, j = hi;
            for (;;)
            {
                do
                {
                    i++;
                } while (data[i].as.integer < pivot);
                do
                {
                    j--;
                } while (j > lo && data[j].as.integer > pivot);
                if (i >= j)
                    break;
                Value t = data[i];
                data[i] = data[j];
                data[j] = t;
            }
            Value t = data[i];
            data[i] = data[hi];
            data[hi] = t;
            /* Recurse smaller partition, iterate larger */
            if (i - lo < hi - i)
            {
                qsort_int(data, lo, i - 1);
                lo = i + 1;
            }
            else
            {
                qsort_int(data, i + 1, hi);
                hi = i - 1;
            }
        }
    }

    void array_sort_int(ObjArray *arr)
    {
        int32_t count = arr_count(arr);
        if (count < 2)
            return;
        qsort_int(arr->data, 0, count - 1);
    }

    void array_copy(GC *gc, ObjArray *dst, ObjArray *src)
    {
        int32_t n = arr_count(src);
        array_reserve(gc, dst, n);
        memcpy(dst->data, src->data, (size_t)n * sizeof(Value));
        dst->end = dst->data + n;
    }

    void array_push_n(GC *gc, ObjArray *arr, const Value *vals, int32_t n)
    {
        int32_t count = arr_count(arr);
        int32_t new_count = count + n;
        int32_t cur_cap = arr_capacity(arr);
        if (__builtin_expect(new_count > cur_cap, 0))
        {
            int32_t new_cap = grow_capacity(cur_cap, new_count);
            gc_pause(gc);
            arr->data = (Value *)zen_realloc(gc, arr->data,
                                             sizeof(Value) * cur_cap,
                                             sizeof(Value) * new_cap);
            gc_resume(gc);
            arr->end = arr->data + count;
            arr->cap_end = arr->data + new_cap;
        }
        memcpy(arr->end, vals, (size_t)n * sizeof(Value));
        arr->end += n;
        for (int32_t i = 0; i < n; i++)
            if (__builtin_expect(vals[i].type == VAL_OBJ, 0))
                gc_write_barrier(gc, (Obj *)arr, vals[i].as.obj);
    }

    void array_append(GC *gc, ObjArray *dst, ObjArray *src)
    {
        array_push_n(gc, dst, src->data, arr_count(src));
    }

    /* =========================================================
    ** Map/Set — Chained hash table with node pool.
    ** Bucket array (int32_t[]) + flat node array.
    ** Free nodes marked with hash == 0xFFFFFFFF sentinel.
    ** Load factor ≤ 1.0 → grow when count >= bucket_count.
    ** ========================================================= */

    static inline uint32_t hash_value(Value v)
    {
        uint32_t h;
        /* Fast path for the common case (int keys) — no branch */
        if (__builtin_expect(v.type == VAL_INT, 1))
        {
            h = (uint32_t)v.as.integer * 2654435761u;
        }
        else
        {
            switch (v.type)
            {
            case VAL_NIL:
                h = 0x9e3779b9u;
                break;
            case VAL_BOOL:
                h = v.as.boolean ? 0x9e3779bbu : 0x9e3779b9u;
                break;
            case VAL_INT:
                __builtin_unreachable();
            case VAL_FLOAT:
            {
                uint64_t bits;
                memcpy(&bits, &v.as.number, sizeof(bits));
                h = (uint32_t)((bits ^ (bits >> 32)) * 2654435761ULL);
                break;
            }
            case VAL_OBJ:
                h = v.as.obj->hash;
                break;
            case VAL_SMALL_STRING:
                /* Hash small string inline */
                h = hash_string(v.as.sso.chars, v.as.sso.len);
                break;
            default:
                h = 0;
                break;
            }
        }
        /* 0xFFFFFFFF is the free-node sentinel — never return it */
        return h + (h == 0xFFFFFFFFu);
    }

    /* ===== MAP (chained hash table with node pool) ===== */

    static void map_rebuild_buckets(ObjMap *map)
    {
        int32_t mask = map->bucket_count - 1;
        memset(map->buckets, 0xFF, sizeof(int32_t) * map->bucket_count); /* -1 fill */
        for (int32_t i = 0; i < map->capacity; i++)
        {
            MapNode *n = &map->nodes[i];
            if (n->hash == 0xFFFFFFFFu)
                continue; /* free node sentinel */
            int32_t b = (int32_t)(n->hash & (uint32_t)mask);
            n->next = map->buckets[b];
            map->buckets[b] = i;
        }
    }

    static void map_grow_buckets(GC *gc, ObjMap *map)
    {
        int32_t old_bc = map->bucket_count;
        int32_t new_bc = old_bc == 0 ? 16 : old_bc * 2;
        size_t old_bytes = sizeof(int32_t) * old_bc;
        size_t new_bytes = sizeof(int32_t) * new_bc;
        map->buckets = (int32_t *)realloc(map->buckets, new_bytes);
        gc->bytes_allocated += new_bytes - old_bytes;
        map->bucket_count = new_bc;
        map_rebuild_buckets(map);
    }

    static void map_grow_nodes(GC *gc, ObjMap *map)
    {
        int32_t old_cap = map->capacity;
        int32_t new_cap = old_cap == 0 ? 16 : old_cap * 2;
        size_t old_bytes = sizeof(MapNode) * old_cap;
        size_t new_bytes = sizeof(MapNode) * new_cap;
        map->nodes = (MapNode *)realloc(map->nodes, new_bytes);
        gc->bytes_allocated += new_bytes - old_bytes;

        /* Link new slots into free list */
        for (int32_t i = old_cap; i < new_cap; i++)
        {
            map->nodes[i].hash = 0xFFFFFFFFu;
            map->nodes[i].next = (i + 1 < new_cap) ? i + 1 : map->free_head;
        }
        map->free_head = old_cap;
        map->capacity = new_cap;
    }

    static inline int32_t map_alloc_node(GC *gc, ObjMap *map)
    {
        if (__builtin_expect(map->free_head == -1, 0))
            map_grow_nodes(gc, map);
        int32_t idx = map->free_head;
        map->free_head = map->nodes[idx].next;
        return idx;
    }

    static inline void map_free_node(ObjMap *map, int32_t idx)
    {
        map->nodes[idx].key = val_nil();
        map->nodes[idx].hash = 0xFFFFFFFFu; /* free sentinel */
        map->nodes[idx].next = map->free_head;
        map->free_head = idx;
    }

    bool map_set(GC *gc, ObjMap *map, Value key, Value val)
    {
        /* Ensure we have buckets */
        if (__builtin_expect(map->bucket_count == 0, 0))
        {
            map_grow_nodes(gc, map);
            map_grow_buckets(gc, map);
        }
        /* Grow buckets if load factor > 0.875 (count*8 >= bucket_count*7) */
        if (__builtin_expect(map->count * 8 >= map->bucket_count * 7, 0))
            map_grow_buckets(gc, map);

        uint32_t h = hash_value(key);
        int32_t mask = map->bucket_count - 1;
        int32_t bucket = (int32_t)(h & (uint32_t)mask);
        int32_t idx = map->buckets[bucket];

        /* Walk chain looking for existing key */
        while (idx != -1)
        {
            MapNode *n = &map->nodes[idx];
            if (n->hash == h && values_equal(n->key, key))
            {
                n->value = val;
                if (__builtin_expect(val.type == VAL_OBJ, 0))
                    gc_write_barrier(gc, (Obj *)map, val.as.obj);
                return false; /* updated existing */
            }
            idx = n->next;
        }

        /* Not found — insert new node at head of chain */
        int32_t ni = map_alloc_node(gc, map);
        MapNode *node = &map->nodes[ni];
        node->key = key;
        node->value = val;
        node->hash = h;
        node->next = map->buckets[bucket];
        map->buckets[bucket] = ni;
        map->count++;

        if (__builtin_expect(key.type == VAL_OBJ, 0))
            gc_write_barrier(gc, (Obj *)map, key.as.obj);
        if (__builtin_expect(val.type == VAL_OBJ, 0))
            gc_write_barrier(gc, (Obj *)map, val.as.obj);
        return true; /* new entry */
    }

    Value map_get(ObjMap *map, Value key, bool *found)
    {
        if (__builtin_expect(map->count == 0, 0))
        {
            *found = false;
            return val_nil();
        }
        uint32_t h = hash_value(key);
        int32_t mask = map->bucket_count - 1;
        int32_t idx = map->buckets[(int32_t)(h & (uint32_t)mask)];

        while (idx != -1)
        {
            MapNode *n = &map->nodes[idx];
            if (n->hash == h && values_equal(n->key, key))
            {
                *found = true;
                return n->value;
            }
            idx = n->next;
        }
        *found = false;
        return val_nil();
    }

    bool map_delete(ObjMap *map, Value key)
    {
        if (map->count == 0)
            return false;
        uint32_t h = hash_value(key);
        int32_t mask = map->bucket_count - 1;
        int32_t bucket = (int32_t)(h & (uint32_t)mask);
        int32_t idx = map->buckets[bucket];
        int32_t prev = -1;

        while (idx != -1)
        {
            MapNode *n = &map->nodes[idx];
            if (n->hash == h && values_equal(n->key, key))
            {
                /* Unlink from chain */
                if (prev == -1)
                    map->buckets[bucket] = n->next;
                else
                    map->nodes[prev].next = n->next;
                map_free_node(map, idx);
                map->count--;
                return true;
            }
            prev = idx;
            idx = n->next;
        }
        return false;
    }

    bool map_contains(ObjMap *map, Value key)
    {
        if (map->count == 0)
            return false;
        uint32_t h = hash_value(key);
        int32_t mask = map->bucket_count - 1;
        int32_t idx = map->buckets[(int32_t)(h & (uint32_t)mask)];

        while (idx != -1)
        {
            MapNode *n = &map->nodes[idx];
            if (n->hash == h && values_equal(n->key, key))
                return true;
            idx = n->next;
        }
        return false;
    }

    int32_t map_count(ObjMap *map) { return map->count; }

    void map_clear(GC *gc, ObjMap *map)
    {
        if (map->buckets)
        {
            gc->bytes_allocated -= sizeof(int32_t) * map->bucket_count;
            free(map->buckets);
        }
        if (map->nodes)
        {
            gc->bytes_allocated -= sizeof(MapNode) * map->capacity;
            free(map->nodes);
        }
        map->buckets = nullptr;
        map->nodes = nullptr;
        map->count = 0;
        map->capacity = 0;
        map->bucket_count = 0;
        map->free_head = -1;
    }

    void map_keys(GC *gc, ObjMap *map, ObjArray *out)
    {
        for (int32_t i = 0; i < map->capacity; i++)
            if (map->nodes[i].hash != 0xFFFFFFFFu)
                array_push(gc, out, map->nodes[i].key);
    }

    void map_values(GC *gc, ObjMap *map, ObjArray *out)
    {
        for (int32_t i = 0; i < map->capacity; i++)
            if (map->nodes[i].hash != 0xFFFFFFFFu)
                array_push(gc, out, map->nodes[i].value);
    }

    /* ===== SET (chained hash table with node pool) ===== */

    ObjSet *new_set(GC *gc)
    {
        ObjSet *s = (ObjSet *)alloc_obj(gc, sizeof(ObjSet), OBJ_SET);
        s->count = 0;
        s->capacity = 0;
        s->bucket_count = 0;
        s->free_head = -1;
        s->buckets = nullptr;
        s->nodes = nullptr;
        return s;
    }

    static void set_rebuild_buckets(ObjSet *set)
    {
        int32_t mask = set->bucket_count - 1;
        memset(set->buckets, 0xFF, sizeof(int32_t) * set->bucket_count);
        for (int32_t i = 0; i < set->capacity; i++)
        {
            SetNode *n = &set->nodes[i];
            if (n->hash == 0xFFFFFFFFu)
                continue;
            int32_t b = (int32_t)(n->hash & (uint32_t)mask);
            n->next = set->buckets[b];
            set->buckets[b] = i;
        }
    }

    static void set_grow_buckets(GC *gc, ObjSet *set)
    {
        int32_t old_bc = set->bucket_count;
        int32_t new_bc = old_bc == 0 ? 16 : old_bc * 2;
        size_t old_bytes = sizeof(int32_t) * old_bc;
        size_t new_bytes = sizeof(int32_t) * new_bc;
        set->buckets = (int32_t *)realloc(set->buckets, new_bytes);
        gc->bytes_allocated += new_bytes - old_bytes;
        set->bucket_count = new_bc;
        set_rebuild_buckets(set);
    }

    static void set_grow_nodes(GC *gc, ObjSet *set)
    {
        int32_t old_cap = set->capacity;
        int32_t new_cap = old_cap == 0 ? 16 : old_cap * 2;
        size_t old_bytes = sizeof(SetNode) * old_cap;
        size_t new_bytes = sizeof(SetNode) * new_cap;
        set->nodes = (SetNode *)realloc(set->nodes, new_bytes);
        gc->bytes_allocated += new_bytes - old_bytes;

        for (int32_t i = old_cap; i < new_cap; i++)
        {
            set->nodes[i].hash = 0xFFFFFFFFu;
            set->nodes[i].next = (i + 1 < new_cap) ? i + 1 : set->free_head;
        }
        set->free_head = old_cap;
        set->capacity = new_cap;
    }

    static inline int32_t set_alloc_node(GC *gc, ObjSet *set)
    {
        if (__builtin_expect(set->free_head == -1, 0))
            set_grow_nodes(gc, set);
        int32_t idx = set->free_head;
        set->free_head = set->nodes[idx].next;
        return idx;
    }

    static inline void set_free_node(ObjSet *set, int32_t idx)
    {
        set->nodes[idx].key = val_nil();
        set->nodes[idx].hash = 0xFFFFFFFFu;
        set->nodes[idx].next = set->free_head;
        set->free_head = idx;
    }

    bool set_add(GC *gc, ObjSet *set, Value key)
    {
        if (__builtin_expect(set->bucket_count == 0, 0))
        {
            set_grow_nodes(gc, set);
            set_grow_buckets(gc, set);
        }
        /* Grow buckets if load factor > 0.875 */
        if (__builtin_expect(set->count * 8 >= set->bucket_count * 7, 0))
            set_grow_buckets(gc, set);

        uint32_t h = hash_value(key);
        int32_t mask = set->bucket_count - 1;
        int32_t bucket = (int32_t)(h & (uint32_t)mask);
        int32_t idx = set->buckets[bucket];

        while (idx != -1)
        {
            SetNode *n = &set->nodes[idx];
            if (n->hash == h && values_equal(n->key, key))
                return false; /* already exists */
            idx = n->next;
        }

        int32_t ni = set_alloc_node(gc, set);
        SetNode *node = &set->nodes[ni];
        node->key = key;
        node->hash = h;
        node->next = set->buckets[bucket];
        set->buckets[bucket] = ni;
        set->count++;

        if (__builtin_expect(key.type == VAL_OBJ, 0))
            gc_write_barrier(gc, (Obj *)set, key.as.obj);
        return true;
    }

    bool set_contains(ObjSet *set, Value key)
    {
        if (__builtin_expect(set->count == 0, 0))
            return false;
        uint32_t h = hash_value(key);
        int32_t mask = set->bucket_count - 1;
        int32_t idx = set->buckets[(int32_t)(h & (uint32_t)mask)];

        while (idx != -1)
        {
            SetNode *n = &set->nodes[idx];
            if (n->hash == h && values_equal(n->key, key))
                return true;
            idx = n->next;
        }
        return false;
    }

    bool set_remove(ObjSet *set, Value key)
    {
        if (set->count == 0)
            return false;
        uint32_t h = hash_value(key);
        int32_t mask = set->bucket_count - 1;
        int32_t bucket = (int32_t)(h & (uint32_t)mask);
        int32_t idx = set->buckets[bucket];
        int32_t prev = -1;

        while (idx != -1)
        {
            SetNode *n = &set->nodes[idx];
            if (n->hash == h && values_equal(n->key, key))
            {
                if (prev == -1)
                    set->buckets[bucket] = n->next;
                else
                    set->nodes[prev].next = n->next;
                set_free_node(set, idx);
                set->count--;
                return true;
            }
            prev = idx;
            idx = n->next;
        }
        return false;
    }

    void set_clear(GC *gc, ObjSet *set)
    {
        if (set->buckets)
        {
            gc->bytes_allocated -= sizeof(int32_t) * set->bucket_count;
            free(set->buckets);
        }
        if (set->nodes)
        {
            gc->bytes_allocated -= sizeof(SetNode) * set->capacity;
            free(set->nodes);
        }
        set->buckets = nullptr;
        set->nodes = nullptr;
        set->count = 0;
        set->capacity = 0;
        set->bucket_count = 0;
        set->free_head = -1;
    }

    int32_t set_count(ObjSet *set) { return set->count; }

    /* =========================================================
    ** ObjBuffer — Typed buffer operations
    ** ========================================================= */

    ObjBuffer *new_buffer(GC *gc, BufferType btype, int32_t count)
    {
        ObjBuffer *buf = (ObjBuffer *)alloc_obj(gc, sizeof(ObjBuffer), OBJ_BUFFER);
        buf->btype = btype;
        buf->count = count;
        buf->capacity = count;
        buf->data = nullptr;
        int elem_sz = buffer_elem_size[btype];
        size_t bytes = (size_t)count * elem_sz;
        buf->data = (uint8_t *)zen_alloc_now(gc, bytes);
        memset(buf->data, 0, bytes);
        return buf;
    }

    void buffer_set(ObjBuffer *buf, int32_t index, double val)
    {
        uint8_t *p = buf->data + (size_t)index * buffer_elem_size[buf->btype];
        auto wrap_unsigned = [](double v, double mod) -> uint64_t
        {
            if (!std::isfinite(v))
                return 0;
            double wrapped = std::fmod(std::trunc(v), mod);
            if (wrapped < 0)
                wrapped += mod;
            return (uint64_t)wrapped;
        };
        auto wrap_signed = [&](double v, int bits) -> int64_t
        {
            double mod = std::ldexp(1.0, bits);
            uint64_t wrapped = wrap_unsigned(v, mod);
            uint64_t sign_bit = (uint64_t)1 << (bits - 1);
            if (wrapped >= sign_bit)
                return (int64_t)wrapped - (int64_t)((uint64_t)1 << bits);
            return (int64_t)wrapped;
        };
        switch (buf->btype)
        {
        case BUF_INT8:
            *(int8_t *)p = (int8_t)wrap_signed(val, 8);
            break;
        case BUF_INT16:
            *(int16_t *)p = (int16_t)wrap_signed(val, 16);
            break;
        case BUF_INT32:
            *(int32_t *)p = (int32_t)wrap_signed(val, 32);
            break;
        case BUF_UINT8:
            *(uint8_t *)p = (uint8_t)wrap_unsigned(val, 256.0);
            break;
        case BUF_UINT16:
            *(uint16_t *)p = (uint16_t)wrap_unsigned(val, 65536.0);
            break;
        case BUF_UINT32:
            *(uint32_t *)p = (uint32_t)wrap_unsigned(val, 4294967296.0);
            break;
        case BUF_FLOAT32:
            *(float *)p = (float)val;
            break;
        case BUF_FLOAT64:
            *(double *)p = val;
            break;
        }
    }

    double buffer_get(ObjBuffer *buf, int32_t index)
    {
        uint8_t *p = buf->data + (size_t)index * buffer_elem_size[buf->btype];
        switch (buf->btype)
        {
        case BUF_INT8:
            return (double)*(int8_t *)p;
        case BUF_INT16:
            return (double)*(int16_t *)p;
        case BUF_INT32:
            return (double)*(int32_t *)p;
        case BUF_UINT8:
            return (double)*(uint8_t *)p;
        case BUF_UINT16:
            return (double)*(uint16_t *)p;
        case BUF_UINT32:
            return (double)*(uint32_t *)p;
        case BUF_FLOAT32:
            return (double)*(float *)p;
        case BUF_FLOAT64:
            return *(double *)p;
        }
        return 0.0;
    }

    void buffer_fill(ObjBuffer *buf, double val)
    {
        for (int32_t i = 0; i < buf->count; i++)
            buffer_set(buf, i, val);
    }

    ObjClass *new_class(GC *gc, ObjString *name, ObjClass *parent)
    {
        ObjClass *cls = (ObjClass *)alloc_obj(gc, sizeof(ObjClass), OBJ_CLASS);
        cls->name = name;
        cls->parent = parent;
        cls->methods = nullptr; /* init before new_map to survive stress GC */
        cls->num_fields = 0;
        cls->field_names = nullptr;
        cls->field_defaults = nullptr;
        cls->num_field_defaults = 0;
        cls->vtable = nullptr;
        cls->vtable_size = 0;
        for (int i = 0; i < kOperatorSlotCount; i++)
            cls->operator_slots[i] = val_nil();
        cls->native_ctor = nullptr;
        cls->native_dtor = nullptr;
        cls->persistent = parent ? parent->persistent : false;
        cls->constructable = true;
        /* Allocate methods map AFTER all fields are safe for GC traversal */
        cls->methods = new_map(gc);
        return cls;
    }

    ObjInstance *new_instance(GC *gc, ObjClass *klass)
    {
        ObjInstance *inst;
        int nf = klass->num_fields;
        if (klass->persistent)
        {
            /* Persistent: arena alloc sem trigger GC, NUNCA entra na lista do GC.
            ** C++ é dono da memória. Chama vm.destroy_instance() para libertar. */
            inst = (ObjInstance *)zen_alloc_now(gc, sizeof(ObjInstance));
            inst->obj.type = OBJ_INSTANCE;
            inst->obj.color = GC_BLACK;
            inst->obj.interned = 0;
            inst->obj.hash = 0;
            inst->obj.gc_next = nullptr; /* not in any list */
        }
        else
        {
            /* The field array allocation below can trigger GC. Until this
            ** function returns, the new instance is not yet reachable from a
            ** VM root, so keep GC disabled during construction. */
            gc_pause(gc);
            inst = (ObjInstance *)alloc_obj(gc, sizeof(ObjInstance), OBJ_INSTANCE);
        }

        inst->klass = klass;
        inst->native_data = nullptr;
        inst->num_fields = nf;
        if (nf > 0)
        {
            if (klass->persistent)
            {
                /* Persistent instance: fields from arena (zen_alloc_now).
                ** The GC never sweeps these because the persistent instance
                ** is not in the GC object list. zen_free releases back to arena. */
                inst->fields = (Value *)zen_alloc_now(gc, sizeof(Value) * nf);
            }
            else
            {
                inst->fields = (Value *)zen_alloc(gc, sizeof(Value) * nf);
            }
            /* A field the class body gave a value starts on that value, the
            ** rest on None. This is what makes "class A:" with "speed = 5.0"
            ** work without a constructor, and it runs before __init__ so a
            ** constructor can still overwrite whatever it wants. */
            const int nd = klass->field_defaults ? klass->num_field_defaults : 0;
            for (int i = 0; i < nf; i++)
                inst->fields[i] = i < nd ? klass->field_defaults[i] : val_nil();
        }
        else
        {
            inst->fields = nullptr;
        }
        if (!klass->persistent)
            gc_resume(gc);
        return inst;
    }

    ObjRange *new_range(GC *gc, int64_t start, int64_t stop, int64_t step)
    {
        ObjRange *r = (ObjRange *)alloc_obj(gc, sizeof(ObjRange), OBJ_RANGE);
        r->start = start;
        r->stop = stop;
        r->step = step;
        return r;
    }

    /* Destroy a persistent instance — C++ owns lifetime, calls this explicitly */
    void destroy_instance(GC *gc, ObjInstance *inst)
    {
        if (!inst)
            return;
        /* Call native destructor — walk parent chain */
        if (inst->native_data)
        {
            ObjClass *dtor_src = inst->klass;
            while (dtor_src && !dtor_src->native_dtor)
                dtor_src = dtor_src->parent;
            if (dtor_src && dtor_src->native_dtor)
            {
                dtor_src->native_dtor((VM *)gc->vm, inst->native_data);
                inst->native_data = nullptr;
            }
        }
        /* Free fields back to arena */
        if (inst->fields)
        {
            zen_free(gc, inst->fields, sizeof(Value) * inst->num_fields);
        }
        /* Free the instance itself back to arena */
        zen_free(gc, inst, sizeof(ObjInstance));
    }

    /* =========================================================
    ** GC — Tri-color Mark & Sweep
    ** ========================================================= */

    void gc_mark_obj(GC *gc, Obj *obj)
    {
        if (obj == nullptr)
            return;
        if (obj->color != GC_WHITE)
            return; /* já visitado */

        obj->color = GC_GRAY;

        /* Adiciona ao gray list */
        if (gc->gray_count >= gc->gray_capacity)
        {
            gc->gray_capacity = gc->gray_capacity < 8 ? 8 : gc->gray_capacity * 2;
            gc->gray_list = (Obj **)realloc(gc->gray_list,
                                            sizeof(Obj *) * gc->gray_capacity);
        }
        gc->gray_list[gc->gray_count++] = obj;
    }

    void gc_mark_value(GC *gc, Value v)
    {
        if (is_obj(v))
            gc_mark_obj(gc, v.as.obj);
        /* VAL_SMALL_STRING doesn't need marking (inline, not allocated on heap) */
    }

    /* Processa um objecto gray → marca os filhos, torna-o black */
    static void gc_blacken(GC *gc, Obj *obj)
    {
        obj->color = GC_BLACK;

        switch (obj->type)
        {
        case OBJ_STRING:
            /* strings não têm referências filhas */
            break;

        case OBJ_FUNC:
        {
            ObjFunc *fn = (ObjFunc *)obj;
            gc_mark_obj(gc, (Obj *)fn->name);
            for (int i = 0; i < fn->const_count; i++)
                gc_mark_value(gc, fn->constants[i]);
            for (int i = 0; i < fn->default_count; i++)
                gc_mark_value(gc, fn->defaults[i]);
            break;
        }

        case OBJ_NATIVE:
        {
            ObjNative *nat = (ObjNative *)obj;
            gc_mark_obj(gc, (Obj *)nat->name);
            break;
        }

        case OBJ_ARRAY:
        {
            ObjArray *arr = (ObjArray *)obj;
            int32_t n = arr_count(arr);
            for (int i = 0; i < n; i++)
                gc_mark_value(gc, arr->data[i]);
            break;
        }

        case OBJ_MAP:
        {
            ObjMap *map = (ObjMap *)obj;
            for (int i = 0; i < map->capacity; i++)
            {
                if (map->nodes[i].hash != 0xFFFFFFFFu)
                {
                    gc_mark_value(gc, map->nodes[i].key);
                    gc_mark_value(gc, map->nodes[i].value);
                }
            }
            break;
        }

        case OBJ_SET:
        {
            ObjSet *set = (ObjSet *)obj;
            for (int i = 0; i < set->capacity; i++)
            {
                if (set->nodes[i].hash != 0xFFFFFFFFu)
                    gc_mark_value(gc, set->nodes[i].key);
            }
            break;
        }

        case OBJ_BUFFER:
            /* No GC references in raw byte data */
            break;

        case OBJ_STRUCT_DEF:
        {
            ObjStructDef *def = (ObjStructDef *)obj;
            gc_mark_obj(gc, (Obj *)def->name);
            for (int32_t i = 0; i < def->num_fields; i++)
                gc_mark_obj(gc, (Obj *)def->field_names[i]);
            break;
        }

        case OBJ_STRUCT:
        {
            ObjStruct *s = (ObjStruct *)obj;
            gc_mark_obj(gc, (Obj *)s->def);
            for (int32_t i = 0; i < s->def->num_fields; i++)
                gc_mark_value(gc, s->fields[i]);
            break;
        }

        case OBJ_NATIVE_STRUCT_DEF:
        {
            NativeStructDef *def = (NativeStructDef *)obj;
            gc_mark_obj(gc, (Obj *)def->name);
            for (int16_t i = 0; i < def->num_fields; i++)
                gc_mark_obj(gc, (Obj *)def->fields[i].name);
            break;
        }

        case OBJ_NATIVE_STRUCT:
        {
            ObjNativeStruct *ns = (ObjNativeStruct *)obj;
            gc_mark_obj(gc, (Obj *)ns->def);
            break;
        }

        case OBJ_CLASS:
        {
            ObjClass *cls = (ObjClass *)obj;
            gc_mark_obj(gc, (Obj *)cls->name);
            gc_mark_obj(gc, (Obj *)cls->parent);
            if (cls->methods) gc_mark_obj(gc, (Obj *)cls->methods);
            /* Field names are interned strings compared by pointer in
            ** OP_GETFIELD/OP_SETFIELD. A class built from C++ (ClassBuilder)
            ** is their only holder, so without this they are swept and the
            ** pointer compare fails against a freshly interned name. */
            for (int32_t i = 0; i < cls->num_fields; i++)
                if (cls->field_names[i])
                    gc_mark_obj(gc, (Obj *)cls->field_names[i]);
            /* A class body default can be a string, and the class is its
            ** only holder until an instance copies it. */
            if (cls->field_defaults)
                for (int32_t i = 0; i < cls->num_field_defaults; i++)
                    gc_mark_value(gc, cls->field_defaults[i]);
            /* Mark vtable entries */
            for (int32_t i = 0; i < cls->vtable_size; i++)
                if (cls->vtable[i].type == VAL_OBJ)
                    gc_mark_obj(gc, cls->vtable[i].as.obj);
            for (int32_t i = 0; i < kOperatorSlotCount; i++)
                if (cls->operator_slots[i].type == VAL_OBJ)
                    gc_mark_obj(gc, cls->operator_slots[i].as.obj);
            break;
        }

        case OBJ_INSTANCE:
        {
            ObjInstance *inst = (ObjInstance *)obj;
            gc_mark_obj(gc, (Obj *)inst->klass);
            int nf = inst->num_fields;
            for (int i = 0; i < nf; i++)
                gc_mark_value(gc, inst->fields[i]);
            break;
        }

        case OBJ_CLOSURE:
        {
            ObjClosure *cl = (ObjClosure *)obj;
            gc_mark_obj(gc, (Obj *)cl->func);
            for (int i = 0; i < cl->upvalue_count; i++)
                gc_mark_obj(gc, (Obj *)cl->upvalues[i]);
            break;
        }

        case OBJ_UPVALUE:
        {
            ObjUpvalue *uv = (ObjUpvalue *)obj;
            gc_mark_value(gc, uv->closed);
            break;
        }

        case OBJ_FIBER:
        {
            ObjFiber *fiber = (ObjFiber *)obj;
            /* Mark stack values */
            for (Value *v = fiber->stack; v < fiber->stack_top; v++)
                gc_mark_value(gc, *v);
            /* Mark call frame closures */
            for (int i = 0; i < fiber->frame_count; i++)
            {
                if (fiber->frames[i].closure)
                    gc_mark_obj(gc, (Obj *)fiber->frames[i].closure);
            }
            /* Mark open upvalues */
            ObjUpvalue *uv = fiber->open_upvalues;
            while (uv)
            {
                gc_mark_obj(gc, (Obj *)uv);
                uv = uv->next;
            }
            /* Mark caller fiber */
            if (fiber->caller)
                gc_mark_obj(gc, (Obj *)fiber->caller);
            /* Mark error string */
            if (fiber->error)
                gc_mark_obj(gc, (Obj *)fiber->error);
            break;
        }

        case OBJ_RANGE:
            /* ObjRange has no GC references (plain integers) */
            break;
        }
    }

    /* Trace: processa todos os grays */
    static void gc_trace_refs(GC *gc)
    {
        while (gc->gray_count > 0)
        {
            Obj *obj = gc->gray_list[--gc->gray_count];
            gc_blacken(gc, obj);
        }
    }

    /* Calcula tamanho de um objecto para desalocar */
    static size_t obj_size(Obj *obj)
    {
        switch (obj->type)
        {
        case OBJ_STRING:
        {
            ObjString *s = (ObjString *)obj;
            int32_t cap = s->capacity > 0 ? s->capacity : s->length + 1;
            return sizeof(ObjString) + cap;
        }
        case OBJ_FUNC:
            return sizeof(ObjFunc);
        case OBJ_NATIVE:
            return sizeof(ObjNative);
        case OBJ_CLOSURE:
            return sizeof(ObjClosure);
        case OBJ_UPVALUE:
            return sizeof(ObjUpvalue);
        case OBJ_FIBER:
            return sizeof(ObjFiber);
        case OBJ_ARRAY:
            return sizeof(ObjArray);
        case OBJ_MAP:
            return sizeof(ObjMap);
        case OBJ_SET:
            return sizeof(ObjSet);
        case OBJ_BUFFER:
            return sizeof(ObjBuffer);
        case OBJ_STRUCT_DEF:
            return sizeof(ObjStructDef);
        case OBJ_STRUCT:
            return sizeof(ObjStruct);
        case OBJ_NATIVE_STRUCT_DEF:
            return sizeof(NativeStructDef);
        case OBJ_NATIVE_STRUCT:
            return sizeof(ObjNativeStruct);
        case OBJ_CLASS:
            return sizeof(ObjClass);
        case OBJ_INSTANCE:
            return sizeof(ObjInstance);
        case OBJ_RANGE:
            return sizeof(ObjRange);
        }
        return 0;
    }

    /* Liberta um objecto e os seus buffers internos */
    static void free_obj(GC *gc, Obj *obj)
    {
        switch (obj->type)
        {
        case OBJ_STRING:
            /* nada extra — chars[] está inline */
            break;
        case OBJ_FUNC:
        {
            ObjFunc *fn = (ObjFunc *)obj;
            if (fn->code)
                zen_free(gc, fn->code, sizeof(Instruction) * fn->code_count);
            if (fn->lines)
                zen_free(gc, fn->lines, sizeof(int32_t) * fn->code_count);
            if (fn->constants)
                zen_free(gc, fn->constants, sizeof(Value) * fn->const_count);
            if (fn->upval_descs)
                zen_free(gc, fn->upval_descs, sizeof(UpvalDesc) * fn->upvalue_count);
            if (fn->defaults)
                zen_free(gc, fn->defaults, sizeof(Value) * fn->default_count);
            break;
        }
        case OBJ_NATIVE:
            break;
        case OBJ_ARRAY:
        {
            ObjArray *arr = (ObjArray *)obj;
            if (arr->data)
                zen_free(gc, arr->data, sizeof(Value) * arr_capacity(arr));
            break;
        }
        case OBJ_MAP:
        {
            ObjMap *map = (ObjMap *)obj;
            if (map->buckets)
            {
                gc->bytes_allocated -= sizeof(int32_t) * map->bucket_count;
                free(map->buckets);
            }
            if (map->nodes)
            {
                gc->bytes_allocated -= sizeof(MapNode) * map->capacity;
                free(map->nodes);
            }
            break;
        }
        case OBJ_SET:
        {
            ObjSet *set = (ObjSet *)obj;
            if (set->buckets)
            {
                gc->bytes_allocated -= sizeof(int32_t) * set->bucket_count;
                free(set->buckets);
            }
            if (set->nodes)
            {
                gc->bytes_allocated -= sizeof(SetNode) * set->capacity;
                free(set->nodes);
            }
            break;
        }
        case OBJ_BUFFER:
        {
            ObjBuffer *buf = (ObjBuffer *)obj;
            if (buf->data)
                zen_free(gc, buf->data, (size_t)buf->capacity * buffer_elem_size[buf->btype]);
            break;
        }
        case OBJ_STRUCT_DEF:
        {
            ObjStructDef *def = (ObjStructDef *)obj;
            if (def->field_names)
                zen_free(gc, def->field_names, sizeof(ObjString *) * def->num_fields);
            break;
        }
        case OBJ_STRUCT:
        {
            ObjStruct *s = (ObjStruct *)obj;
            if (s->fields)
                zen_free(gc, s->fields, sizeof(Value) * s->def->num_fields);
            break;
        }
        case OBJ_NATIVE_STRUCT_DEF:
        {
            NativeStructDef *def = (NativeStructDef *)obj;
            if (def->fields)
                zen_free(gc, def->fields, sizeof(NativeFieldDef) * def->num_fields);
            break;
        }
        case OBJ_NATIVE_STRUCT:
        {
            ObjNativeStruct *ns = (ObjNativeStruct *)obj;
            if (ns->def->dtor)
                ns->def->dtor(nullptr, ns->data);
            if (ns->data)
                zen_free(gc, ns->data, ns->def->struct_size);
            break;
        }
        case OBJ_CLASS:
        {
            ObjClass *cls = (ObjClass *)obj;
            if (cls->field_names)
                zen_free(gc, cls->field_names, sizeof(ObjString *) * cls->num_fields);
            if (cls->field_defaults)
                zen_free(gc, cls->field_defaults, sizeof(Value) * cls->num_field_defaults);
            if (cls->vtable)
                zen_free(gc, cls->vtable, sizeof(Value) * cls->vtable_size);
            break;
        }
        case OBJ_INSTANCE:
        {
            ObjInstance *inst = (ObjInstance *)obj;
            /* Call native destructor — walk parent chain to find dtor */
            if (inst->native_data)
            {
                ObjClass *dtor_src = inst->klass;
                while (dtor_src && !dtor_src->native_dtor)
                    dtor_src = dtor_src->parent;
                if (dtor_src && dtor_src->native_dtor)
                {
                    dtor_src->native_dtor((VM *)gc->vm, inst->native_data);
                    inst->native_data = nullptr;
                }
            }
            if (inst->fields)
                zen_free(gc, inst->fields, sizeof(Value) * inst->num_fields);
            break;
        }
        case OBJ_RANGE:
            /* No extra allocations to free */
            break;
        case OBJ_CLOSURE:
        {
            ObjClosure *cl = (ObjClosure *)obj;
            if (cl->upvalues)
                zen_free(gc, cl->upvalues, sizeof(ObjUpvalue *) * cl->upvalue_count);
            break;
        }
        case OBJ_UPVALUE:
            /* No extra allocs — closed value is inline */
            break;
        case OBJ_FIBER:
        {
            ObjFiber *fiber = (ObjFiber *)obj;
            if (fiber->stack)
                zen_free(gc, fiber->stack, sizeof(Value) * fiber->stack_capacity);
            if (fiber->frames)
                zen_free(gc, fiber->frames, sizeof(CallFrame) * fiber->frame_capacity);
            break;
        }
        }

        size_t sz = obj_size(obj);
        zen_free(gc, obj, sz);
    }

    /* Sweep: percorre TODOS os objectos, free os WHITE */
    static void gc_rebuild_intern_table(GC *gc)
    {
        ObjString **old_table = gc->strings;
        int old_capacity = gc->string_capacity;
        ObjString **new_table = (ObjString **)calloc((size_t)old_capacity, sizeof(ObjString *));
        int new_count = 0;

        for (int i = 0; i < old_capacity; i++)
        {
            ObjString *s = old_table[i];
            if (!s || s->obj.color == GC_WHITE)
                continue;

            uint32_t idx = s->obj.hash & (uint32_t)(old_capacity - 1);
            while (new_table[idx] != nullptr)
                idx = (idx + 1) & (uint32_t)(old_capacity - 1);
            new_table[idx] = s;
            new_count++;
        }

        /* old_table was calloc'd — free with free(), not arena.
           bytes_allocated stays the same (same capacity). */
        free(old_table);
        gc->strings = new_table;
        gc->string_count = new_count;
    }

    static void gc_sweep(GC *gc)
    {
        gc_rebuild_intern_table(gc);

        Obj **ptr = &gc->objects;
        while (*ptr)
        {
            if ((*ptr)->color == GC_WHITE)
            {
                Obj *dead = *ptr;
                *ptr = dead->gc_next;

                /* Remove da intern table se for string */
                // if (dead->type == OBJ_STRING && dead->interned)
                // {
                //     ObjString *s = (ObjString *)dead;
                //     uint32_t idx = s->obj.hash & (gc->string_capacity - 1);
                //     while (gc->strings[idx] != s)
                //     {
                //         if (gc->strings[idx] == nullptr)
                //             break;
                //         idx = (idx + 1) & (gc->string_capacity - 1);
                //     }
                //     if (gc->strings[idx] == s)
                //     {
                //         gc->strings[idx] = nullptr;
                //         gc->string_count--;
                //         /* NOTA: tombstone simplificado — pode degradar.
                //            Para produção: rehash periódico. */
                //     }
                // }

                free_obj(gc, dead);
            }
            else
            {
                /* Sobreviveu: repinta WHITE para o próximo ciclo */
                (*ptr)->color = GC_WHITE;
                ptr = &(*ptr)->gc_next;
            }
        }
    }

    /* GC constants */
    static const size_t kGCMinThreshold = 1024 * 64;         /* 64 KB */
    //static const size_t kGCMaxThreshold = 1024 * 1024 * 256; /* 256 MB */

    /* gc_sweep_all — free ALL objects unconditionally (used in VM destructor) */
    void gc_sweep_all(GC *gc)
    {
        Obj *obj = gc->objects;
        while (obj)
        {
            Obj *next = obj->gc_next;
            if (obj->type == OBJ_STRING)
            {
                ObjString *s = (ObjString *)obj;
                if (s->obj.interned)
                {
                    uint32_t idx = s->obj.hash & (gc->string_capacity - 1);
                    while (gc->strings[idx] != s)
                    {
                        if (gc->strings[idx] == nullptr)
                            break;
                        idx = (idx + 1) & (gc->string_capacity - 1);
                    }
                    if (gc->strings[idx] == s)
                    {
                        gc->strings[idx] = nullptr;
                        gc->string_count--;
                    }
                }
            }
            free_obj(gc, obj);
            obj = next;
        }
        gc->objects = nullptr;
    }

    /* gc_collect — chamado pelo VM quando bytes_allocated > next_gc */
    void gc_collect(VM *vm)
    {
        GC *gc = &vm->get_gc();

        /* Prevent re-entrant GC */
        void *saved_vm = gc->vm;
        gc->vm = nullptr;

#ifdef ZEN_DEBUG_GC
        size_t before = gc->bytes_allocated;
#endif

        /* Reset gray list */
        gc->gray_count = 0; 

        /* Full collection: reset all to WHITE, then re-mark reachable. */
        for (Obj *obj = gc->objects; obj; obj = obj->gc_next)
            obj->color = GC_WHITE;

        /* Mark roots */
        vm->gc_mark_roots();
        if (gc->extra_mark)
            gc->extra_mark(gc, gc->extra_mark_ud);

        /* Trace references */
        gc_trace_refs(gc);

        /* Sweep dead objects */
        gc_sweep(gc);

        /* Adjust next threshold */
        gc->next_gc = (size_t)(gc->bytes_allocated * kGCGrowFactor);
        if (gc->next_gc < kGCMinThreshold)
            gc->next_gc = kGCMinThreshold;
        // if (gc->next_gc > kGCMaxThreshold)
        //     gc->next_gc = kGCMaxThreshold;
      while (gc->next_gc <= gc->bytes_allocated)
            gc->next_gc *= 2;
#ifdef ZEN_DEBUG_GC
        fprintf(stderr, "[GC] collected %zu bytes (%zu -> %zu), next at %zu\n",
                before - gc->bytes_allocated, before, gc->bytes_allocated, gc->next_gc);
#endif

        /* Re-enable GC trigger */
        gc->vm = saved_vm;
    }

    /* =========================================================
    ** Deep equality — recursive comparison for arrays, maps, sets.
    ** Used by OP_EQ / OP_EQ_OBJ for Python-like == semantics.
    ** ========================================================= */

    bool values_deep_equal(Value a, Value b)
    {
        /* Fast path: same type + same bits */
        if (a.type == b.type)
        {
            switch (a.type)
            {
            case VAL_NIL:
                return true;
            case VAL_BOOL:
                return a.as.boolean == b.as.boolean;
            case VAL_INT:
                return a.as.integer == b.as.integer;
            case VAL_FLOAT:
                return a.as.number == b.as.number;
            case VAL_PTR:
                return a.as.pointer == b.as.pointer;
            case VAL_SMALL_STRING:
            {
                /* Compare small strings byte-by-byte */
                if (a.as.sso.len != b.as.sso.len)
                    return false;
                for (int i = 0; i < a.as.sso.len; i++)
                    if (a.as.sso.chars[i] != b.as.sso.chars[i])
                        return false;
                return true;
            }
            case VAL_OBJ:
            {
                if (a.as.obj == b.as.obj)
                    return true;
                if (a.as.obj->type != b.as.obj->type)
                    return false;

                switch (a.as.obj->type)
                {
                case OBJ_STRING:
                {
                    ObjString *sa = (ObjString *)a.as.obj;
                    ObjString *sb = (ObjString *)b.as.obj;
                    return sa->length == sb->length &&
                           memcmp(sa->chars, sb->chars, (size_t)sa->length) == 0;
                }

                case OBJ_ARRAY:
                {
                    ObjArray *aa = (ObjArray *)a.as.obj;
                    ObjArray *ab = (ObjArray *)b.as.obj;
                    int len = arr_count(aa);
                    if (len != arr_count(ab))
                        return false;
                    for (int i = 0; i < len; i++)
                    {
                        if (!values_deep_equal(aa->data[i], ab->data[i]))
                            return false;
                    }
                    return true;
                }

                case OBJ_MAP:
                {
                    ObjMap *ma = (ObjMap *)a.as.obj;
                    ObjMap *mb = (ObjMap *)b.as.obj;
                    if (ma->count != mb->count)
                        return false;
                    /* Iterate through bucket chains of ma (only live entries) */
                    for (int bi = 0; bi < ma->bucket_count; bi++)
                    {
                        int32_t idx = ma->buckets[bi];
                        while (idx >= 0)
                        {
                            MapNode *node = &ma->nodes[idx];
                            /* Look up this key in mb */
                            uint32_t h = node->hash;
                            uint32_t mb_bucket = h & (uint32_t)(mb->bucket_count - 1);
                            int32_t mb_idx = mb->buckets[mb_bucket];
                            bool found = false;
                            while (mb_idx >= 0)
                            {
                                MapNode *bn = &mb->nodes[mb_idx];
                                if (bn->hash == h && values_deep_equal(bn->key, node->key))
                                {
                                    if (!values_deep_equal(bn->value, node->value))
                                        return false;
                                    found = true;
                                    break;
                                }
                                mb_idx = bn->next;
                            }
                            if (!found)
                                return false;
                            idx = node->next;
                        }
                    }
                    return true;
                }

                default:
                    return false; /* other object types: reference equality */
                }
            }
            }
            return false;
        }
        /* Mixed int/float */
        if (a.type == VAL_INT && b.type == VAL_FLOAT)
            return (double)a.as.integer == b.as.number;
        if (a.type == VAL_FLOAT && b.type == VAL_INT)
            return a.as.number == (double)b.as.integer;
        return false;
    }

} /* namespace zen */
