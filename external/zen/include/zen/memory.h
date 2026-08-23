#ifndef ZEN_MEMORY_H
#define ZEN_MEMORY_H

#include "object.h"
#include "zen_arena.h"

namespace zen
{

    /*
    ** GC Tri-Color (Mark-Sweep incremental):
    **
    ** Fases:
    **   1. MARK — começa nas raízes (stack, globals, open upvalues)
    **              pinta roots de GRAY, depois processa grays→BLACK
    **   2. SWEEP — varre a linked list de objectos:
    **              WHITE → free; GRAY/BLACK → repinta WHITE para próximo ciclo
    **
    ** Tri-color invariant:
    **   "Um objecto BLACK nunca aponta directamente para WHITE"
    **   Se violar → write barrier pinta o target GRAY
    **
    ** Write barrier: quando código muta um campo de um objecto BLACK
    **   (ex: instance.field = new_string), o GC precisa saber.
    **   → marca o objecto como GRAY (re-scan no próximo step).
    **
    ** Strings interned: tabela separada; strings WHITE sem referências
    **   são removidas da tabela E libertadas.
    */

    struct GC
    {
        Obj *objects;    /* linked list de TODOS os objectos */
        Obj *gray_stack; /* lista de objectos gray (para processar) */
        int gray_count;
        int gray_capacity;
        Obj **gray_list; /* array dinâmico de gray objects */

        size_t bytes_allocated;
        size_t next_gc; /* threshold para próximo ciclo */
        size_t pause_saved_next_gc;
        int pause_depth;

        /* String interning table (open addressing) */
        ObjString **strings;
        int string_count;
        int string_capacity;

        /* Pool allocator for small objects */
        Arena arena;

        /* Back-pointer to VM (for GC trigger from zen_alloc) */
        void *vm;

        /* Optional external roots owned outside the GC object list. */
        void (*extra_mark)(GC *gc, void *ud);
        void *extra_mark_ud;
    };

    /* API de memória — substitui new/delete/malloc/free */
    void *zen_alloc(GC *gc, size_t size);
    void *zen_alloc_now(GC *gc, size_t size); /* alloc sem trigger GC (para objetos temporários durante construção) */
    void *zen_realloc(GC *gc, void *ptr, size_t old_size, size_t new_size);
    void zen_free(GC *gc, void *ptr, size_t size);

    /* Criação de objectos (aloca + regista no GC) */
    ObjString *new_string(GC *gc, const char *chars, int length);
    ObjString *new_string_uninit(GC *gc, int length);
    ObjString *create_string(GC *gc, const char *chars, int length);
    ObjString *new_string_concat(GC *gc, ObjString *a, ObjString *b);
    ObjString *string_append_inplace(GC *gc, ObjString *a, ObjString *b);
    
    /* EXPERIMENTAL SSO: Create string as Value (small strings inline, large on heap) */
    Value new_string_sso(GC *gc, const char *chars, int length);
    
    ObjFunc *new_func(GC *gc);
    ObjNative *new_native(GC *gc, NativeFn fn, int arity, ObjString *name);
    ObjArray *new_array(GC *gc);
    ObjMap *new_map(GC *gc);
    ObjClass *new_class(GC *gc, ObjString *name, ObjClass *parent);
    ObjInstance *new_instance(GC *gc, ObjClass *klass);
    ObjRange *new_range(GC *gc, int64_t start, int64_t stop, int64_t step);
    void destroy_instance(GC *gc, ObjInstance *inst); /* free persistent instance (C++ owned) */

    /* Array operations */
    void array_push_slow(GC *gc, ObjArray *arr, Value val); /* grow + push */
    void array_set(GC *gc, ObjArray *arr, int32_t index, Value val);
    Value array_pop(ObjArray *arr);
    void array_insert(GC *gc, ObjArray *arr, int32_t index, Value val);
    void array_remove(ObjArray *arr, int32_t index);
    void array_clear(ObjArray *arr);
    void array_reserve(GC *gc, ObjArray *arr, int32_t cap);
    int32_t array_find(ObjArray *arr, Value val);          /* -1 if not found */
    int32_t array_find_int(ObjArray *arr, int32_t target); /* fast unrolled int find */
    bool array_contains(ObjArray *arr, Value val);
    void array_reverse(ObjArray *arr);
    void array_sort_int(ObjArray *arr); /* ascending int sort */
    void array_copy(GC *gc, ObjArray *dst, ObjArray *src);
    void array_push_n(GC *gc, ObjArray *arr, const Value *vals, int32_t n);
    void array_append(GC *gc, ObjArray *dst, ObjArray *src);

    /* Map operations */
    bool map_set(GC *gc, ObjMap *map, Value key, Value val);
    Value map_get(ObjMap *map, Value key, bool *found);
    bool map_delete(ObjMap *map, Value key);
    bool map_contains(ObjMap *map, Value key);
    int32_t map_count(ObjMap *map);
    void map_clear(GC *gc, ObjMap *map);
    void map_keys(GC *gc, ObjMap *map, ObjArray *out);   /* collect all keys */
    void map_values(GC *gc, ObjMap *map, ObjArray *out); /* collect all values */

    /* Set operations */
    ObjSet *new_set(GC *gc);
    bool set_add(GC *gc, ObjSet *set, Value key);
    bool set_contains(ObjSet *set, Value key);
    bool set_remove(ObjSet *set, Value key);
    void set_clear(GC *gc, ObjSet *set);
    int32_t set_count(ObjSet *set);

    /* Buffer operations */
    ObjBuffer *new_buffer(GC *gc, BufferType btype, int32_t count);
    void buffer_set(ObjBuffer *buf, int32_t index, double val);
    double buffer_get(ObjBuffer *buf, int32_t index);
    void buffer_fill(ObjBuffer *buf, double val);

    /* GC control */
    void gc_init(GC *gc);
    void gc_collect(VM *vm);
    void gc_pause(GC *gc);
    void gc_resume(GC *gc);
    void gc_sweep_all(GC *gc);
    void gc_mark_value(GC *gc, Value v);
    void gc_mark_obj(GC *gc, Obj *obj);

    inline const uint8_t *get_ws_table(void)
    {
        static uint8_t ws[256] = {0};
        static int init = 0;
        if (!init)
        {
            ws[(uint8_t)' '] = 1;
            ws[(uint8_t)'\t'] = 1;
            ws[(uint8_t)'\n'] = 1;
            ws[(uint8_t)'\r'] = 1;
            init = 1;
        }
        return ws;
    }

    inline const char *find_sep(const char *hay, int hlen,
                                const char *sep, int slen)
    {
        for (int i = 0; i <= hlen - slen; i++)
            if (memcmp(hay + i, sep, slen) == 0)
                return hay + i;
        return nullptr;
    }

    /* Write barrier — chamar quando um obj BLACK recebe nova referência */
    inline void gc_write_barrier(GC *gc, Obj *parent, Obj *child)
    {
        if (parent->color == GC_BLACK && child && child->color == GC_WHITE)
        {
            parent->color = GC_GRAY;
            /* adiciona ao gray list para re-scan */
            if (gc->gray_count >= gc->gray_capacity)
            {
                gc->gray_capacity = gc->gray_capacity < 8 ? 8 : gc->gray_capacity * 2;
                gc->gray_list = (Obj **)realloc(gc->gray_list,
                                                sizeof(Obj *) * gc->gray_capacity);
            }
            gc->gray_list[gc->gray_count++] = parent;
        }
    }

    /* Inline fast paths — hot for game loops */
    inline void array_push(GC *gc, ObjArray *arr, Value val)
    {
        if (__builtin_expect(arr->end != arr->cap_end, 1))
        {
            *arr->end++ = val;
            if (__builtin_expect(val.type == VAL_OBJ, 0))
                gc_write_barrier(gc, (Obj *)arr, val.as.obj);
        }
        else
        {
            array_push_slow(gc, arr, val);
        }
    }

    /* Push int without GC barrier — caller knows it's not OBJ */
    inline void array_push_int(GC *gc, ObjArray *arr, int32_t n)
    {
        Value v;
        v.type = VAL_INT;
        v.as.integer = n;
        if (__builtin_expect(arr->end != arr->cap_end, 1))
        {
            *arr->end++ = v;
        }
        else
        {
            array_push_slow(gc, arr, v);
        }
    }

    inline void array_push_no_grow(GC *gc, ObjArray *arr, Value val)
    {

        *arr->end++ = val;
        if (__builtin_expect(val.type == VAL_OBJ, 0))
            gc_write_barrier(gc, (Obj *)arr, val.as.obj);
    }

    inline Value array_get(ObjArray *arr, int32_t index)
    {
        if (__builtin_expect((uint32_t)index < (uint32_t)arr_count(arr), 1))
            return arr->data[index];
        return val_nil();
    }

    /* Pop without branch — caller guarantees count > 0 */
    inline Value array_pop_unsafe(ObjArray *arr)
    {
        return *--arr->end;
    }

    /* String interning */
    ObjString *intern_string(GC *gc, const char *chars, int length, uint32_t hash);
    ObjString *find_interned(GC *gc, const char *chars, int length, uint32_t hash);

    /* Convenience: auto-hash */
    inline ObjString *intern_string(GC *gc, const char *chars, int length)
    {
        return intern_string(gc, chars, length, hash_string(chars, length));
    }

} /* namespace zen */

#endif /* ZEN_MEMORY_H */
