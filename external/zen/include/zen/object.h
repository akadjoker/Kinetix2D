#ifndef ZEN_OBJECT_H
#define ZEN_OBJECT_H

#include "value.h"

/* Flag bits for Obj::flags */
#define OBJ_FLAG_SHARED 1 /* string has been stored in field/array/map/upvalue */

namespace zen
{

    /*
    ** Obj — Base de TODOS os objectos no heap.
    **
    ** Tri-color GC (como Lua 5.x):
    **   WHITE = não visitado (candidato a free)
    **   GRAY  = visitado mas filhos não processados
    **   BLACK = visitado e filhos processados
    **
    ** Todos os Obj vivem numa linked list (gc_next) para varrer.
    ** O tipo (ObjType) permite cast seguro sem virtual/RTTI.
    **
    ** Layout em memória:
    **   [Obj header (24 bytes)] [dados específicos do tipo]
    **   Sem vtable, sem RTTI, sem new/delete — tudo via zen::alloc().
    */

    enum ObjType : uint8_t
    {
        OBJ_STRING,
        OBJ_FUNC,
        OBJ_NATIVE,
        OBJ_UPVALUE,
        OBJ_CLOSURE,
        OBJ_FIBER,
        OBJ_ARRAY,
        OBJ_MAP,
        OBJ_SET,
        OBJ_BUFFER,
        OBJ_STRUCT_DEF,
        OBJ_STRUCT,
        OBJ_CLASS,
        OBJ_INSTANCE,
        OBJ_RANGE,
        OBJ_NATIVE_STRUCT_DEF,
        OBJ_NATIVE_STRUCT,
    };

    enum GCColor : uint8_t
    {
        GC_WHITE = 0, /* não marcado — vai ser libertado se ficar white */
        GC_GRAY = 1,  /* marcado mas filhos por processar */
        GC_BLACK = 2, /* marcado e filhos processados */
    };

    struct Obj
    {
        ObjType type;
        GCColor color;
        uint8_t interned; /* 1 = in intern table (immutable) */
        uint8_t flags;    /* bit 0 = shared (string stored in field/array/upvalue) */
        uint32_t hash;    /* hash para strings; 0 para outros */
        Obj *gc_next;     /* linked list de todos os objectos */
    };

    /* Helpers para cast seguro (sem RTTI, sem dynamic_cast) */
    inline bool is_obj_type(Value v, ObjType t)
    {
        return is_obj(v) && v.as.obj->type == t;
    }

    /* =========================================================
    ** ObjString — String interned.
    **
    ** Interning: todas as strings com o mesmo conteúdo partilham
    ** o mesmo ObjString*. Permite comparação por ponteiro (O(1)).
    ** Hash pré-calculado na criação (FNV-1a).
    **
    ** Layout: [Obj header][length][chars... \0]
    ** O buffer de chars está INLINE logo após o struct (flexible member).
    ** Zero allocation extra — um só bloco de memória.
    ** ========================================================= */

    struct ObjString
    {
        Obj obj;
        int32_t length;
        int32_t capacity; /* allocated size of chars[] (0 = tight, length+1) */
        char chars[];     /* flexible array member — C99/C++11 */
    };

    inline bool is_string(const Value &v)
    {
        return v.type == VAL_SMALL_STRING || is_obj_type(v, OBJ_STRING);
    }

    inline const char *safe_string_chars(const Value &v)
    {
        if (v.type == VAL_SMALL_STRING)
            return v.as.sso.chars;
        return is_obj_type(v, OBJ_STRING) ? ((ObjString *)v.as.obj)->chars : "";
    }

    inline int safe_string_len(const Value &v)
    {
        if (v.type == VAL_SMALL_STRING)
            return v.as.sso.len;
        return is_obj_type(v, OBJ_STRING) ? ((ObjString *)v.as.obj)->length : 0;
    }

    inline ObjString *as_string(Value v) { return (ObjString *)v.as.obj; }
    inline const char *as_cstring(Value v)
    {
        /* Now redirects to safe accessor to support both VAL_SMALL_STRING and OBJ_STRING */
        return safe_string_chars(v);
    }

    /* FNV-1a hash */
    inline uint32_t hash_string(const char *str, int length)
    {
        uint32_t h = 2166136261u;
        for (int i = 0; i < length; i++)
        {
            h ^= (uint8_t)str[i];
            h *= 16777619u;
        }
        return h;
    }

    /* =========================================================
    ** ObjFunc — Função script compilada.
    **
    ** Contém o bytecode, pool de constantes, e metadados.
    ** Equivalente a Proto* no Lua.
    ** ========================================================= */

    typedef uint32_t Instruction;

    /* Upvalue descriptor (stored on ObjFunc for OP_CLOSURE) */
    struct UpvalDesc
    {
        uint8_t index;    /* local reg in parent, or upvalue index in parent */
        uint8_t is_local; /* 1 = from parent's locals, 0 = from parent's upvalues */
    };

    struct ObjFunc
    {
        Obj obj;
        int32_t arity;      /* número de params (total, inclui os com default) */
        int32_t num_regs;   /* registos necessários (calculado pelo compiler) */
        int32_t code_count; /* número de instruções */
        int32_t code_capacity;
        int32_t const_count; /* número de constantes */
        int32_t const_capacity;
        int32_t upvalue_count;  /* número de upvalues que o closure captura */
        int32_t default_count;  /* número de params com valor default (últimos N) */
        Instruction *code;      /* bytecode array (fixo após compilação) */
        int32_t *lines;         /* lines[i] = source line da instrução i */
        Value *constants;       /* pool de constantes (ints, floats, strings) */
        Value *defaults;        /* default values para os últimos default_count params */
        UpvalDesc *upval_descs; /* upvalue descriptors (NULL se 0) */
        ObjString *name;        /* nome da função (debug) */
        ObjString *source;      /* ficheiro fonte (debug) */
        ObjClass *return_class; /* return type hint (class), NULL if unknown */
        bool is_generator;      /* true if function contains yield */
    };

    inline bool is_func(Value v) { return is_obj_type(v, OBJ_FUNC); }
    inline ObjFunc *as_func(Value v) { return (ObjFunc *)v.as.obj; }

    /* =========================================================
    ** ObjNative — Wrapper de função C++ registada.
    ** ========================================================= */

    typedef int (*NativeFn)(VM *vm, Value *args, int nargs);

    struct ObjNative
    {
        Obj obj;
        NativeFn fn;
        int32_t arity; /* -1 = variadic */
        ObjString *name;
    };

    inline bool is_native(Value v) { return is_obj_type(v, OBJ_NATIVE); }
    inline ObjNative *as_native(Value v) { return (ObjNative *)v.as.obj; }

    /* =========================================================
    ** ObjUpvalue — Célula de upvalue (captura de variável local).
    **
    ** Como Lua 5.x:
    **   - "open": aponta para um slot no stack do fiber (location != &closed)
    **   - "closed": quando o scope fecha, o valor é copiado para 'closed'
    **     e location passa a apontar para &closed
    **
    ** Upvalues abertos do mesmo slot partilham o mesmo ObjUpvalue*
    ** (deduplicados via open_upvalues list no Fiber).
    ** ========================================================= */

    struct ObjUpvalue
    {
        Obj obj;
        Value *location;  /* aponta para stack slot OU para &closed */
        Value closed;     /* valor capturado quando o scope fecha */
        ObjUpvalue *next; /* linked list de upvalues abertos (por fiber) */
    };

    inline bool is_upvalue(Value v) { return is_obj_type(v, OBJ_UPVALUE); }
    inline ObjUpvalue *as_upvalue(Value v) { return (ObjUpvalue *)v.as.obj; }

    /* =========================================================
    ** ObjClosure — Função + upvalues capturados.
    **
    ** Toda função que captura variáveis é wrappada num Closure.
    ** O compilador emite CLOSURE (não LOADK de func) quando há upvalues.
    ** Funções sem captures podem usar ObjFunc directamente (optimização).
    ** ========================================================= */

    struct ObjClosure
    {
        Obj obj;
        ObjFunc *func;         /* a função subjacente */
        ObjUpvalue **upvalues; /* array de ponteiros (tamanho = upvalue_count) */
        int32_t upvalue_count; /* quantos upvalues capturados */
    };

    inline bool is_closure(Value v) { return is_obj_type(v, OBJ_CLOSURE); }
    inline ObjClosure *as_closure(Value v) { return (ObjClosure *)v.as.obj; }

    /* =========================================================
    ** ObjFiber — Coroutine/Fiber (stack próprio, yield/resume).
    **
    ** Cada fiber tem a sua call stack e register stack independentes.
    ** O scheduler é cooperativo (yield explícito, não preemptivo).
    **
    ** Estados:
    **   FIBER_READY    — criado mas nunca executado
    **   FIBER_RUNNING  — activamente a executar
    **   FIBER_SUSPENDED— pausado por yield (pode ser resumido)
    **   FIBER_DONE     — terminou (não pode ser resumido)
    **   FIBER_ERROR    — terminou com erro
    **
    ** Uso no script:
    **   var f = Fiber.new(def() { yield 1; yield 2; })
    **   print(f.resume())  // 1
    **   print(f.resume())  // 2
    **   print(f.state)     // "done"
    ** ========================================================= */

    enum FiberState : uint8_t
    {
        FIBER_READY = 0,
        FIBER_RUNNING,
        FIBER_SUSPENDED,
        FIBER_DONE,
        FIBER_ERROR,
    };

    struct CallFrame; /* forward — definido em vm.h */

    struct ObjFiber
    {
        Obj obj;
        FiberState state;

        /* Stack próprio */
        Value *stack; /* register stack alocado */
        int32_t stack_capacity;
        Value *stack_top;

        /* Call frames próprios */
        CallFrame *frames;
        int32_t frame_count;
        int32_t frame_capacity;

        /* Upvalues abertos neste fiber (para close ao sair de scope) */
        ObjUpvalue *open_upvalues;

        /* Fiber que chamou este (para retornar após finish/yield) */
        ObjFiber *caller;

        /* Valor passado via yield/resume */
        Value transfer_value;
        int32_t yield_dest; /* register to receive value on resume */

        /* Mensagem de erro (se state == FIBER_ERROR) */
        ObjString *error;
    };

    inline bool is_fiber(Value v) { return is_obj_type(v, OBJ_FIBER); }
    inline ObjFiber *as_fiber(Value v) { return (ObjFiber *)v.as.obj; }

    /* =========================================================
    ** ObjArray — Array dinâmico de Values.
    **
    ** Não usa std::vector — gerido manualmente para controlo do GC.
    ** Quando cresce, aloca novo buffer e o velho é libertado.
    ** ========================================================= */

    struct ObjArray
    {
        Obj obj;
        Value *data;    /* buffer start */
        Value *end;     /* data + count (one past last element) */
        Value *cap_end; /* data + capacity */
    };

/* Convenience macros — zero cost */
#define arr_count(a) ((int32_t)((a)->end - (a)->data))
#define arr_capacity(a) ((int32_t)((a)->cap_end - (a)->data))

    inline bool is_array(Value v) { return is_obj_type(v, OBJ_ARRAY); }
    inline ObjArray *as_array(Value v) { return (ObjArray *)v.as.obj; }

    /* =========================================================
    ** ObjMap — Chained hash table with node pool.
    ** Bucket array (int32_t[]) → compact, cache-friendly.
    ** Node pool (flat array) — O(1) alloc via free-list.
    ** No tombstones, no rehash copies (only relink chains).
    ** Load factor ≤ 1.0 → avg chain length ~1.
    ** ========================================================= */

    struct MapNode
    {
        Value key;     /* 16 */
        Value value;   /* 16 */
        uint32_t hash; /*  4 */
        int32_t next;  /*  4: next in chain or free list (-1 = end) */
    }; /* 40 bytes */

    struct ObjMap
    {
        Obj obj;
        int32_t count;        /* live entries */
        int32_t capacity;     /* node array capacity */
        int32_t bucket_count; /* power of 2 */
        int32_t free_head;    /* head of free list (-1 = none) */
        int32_t *buckets;     /* bucket_count entries, -1 = empty */
        MapNode *nodes;       /* flat node array */
        bool is_module;       /* true = module map, skip built-in methods */
    };

    inline bool is_map(Value v) { return is_obj_type(v, OBJ_MAP); }
    inline ObjMap *as_map(Value v) { return (ObjMap *)v.as.obj; }

    /* =========================================================
    ** ObjSet — Same chaining approach, no value field.
    ** ========================================================= */

    struct SetNode
    {
        Value key;     /* 16 */
        uint32_t hash; /*  4 */
        int32_t next;  /*  4 */
    }; /* 24 bytes */

    struct ObjSet
    {
        Obj obj;
        int32_t count;
        int32_t capacity;
        int32_t bucket_count;
        int32_t free_head;
        int32_t *buckets;
        SetNode *nodes;
    };

    inline bool is_set(Value v) { return is_obj_type(v, OBJ_SET); }
    inline ObjSet *as_set(Value v) { return (ObjSet *)v.as.obj; }

    /* =========================================================
    ** ObjBuffer — Typed buffer (Int8Array, Uint8Array, etc.)
    **
    ** Fixed-size element array stored as raw bytes.
    ** JS-style names: Int8Array, Int16Array, Int32Array,
    **                 Uint8Array, Uint16Array, Uint32Array,
    **                 Float32Array, Float64Array
    ** ========================================================= */

    enum BufferType : uint8_t
    {
        BUF_INT8 = 0,
        BUF_INT16,
        BUF_INT32,
        BUF_UINT8,
        BUF_UINT16,
        BUF_UINT32,
        BUF_FLOAT32,
        BUF_FLOAT64,
    };

    static const int buffer_elem_size[] = {1, 2, 4, 1, 2, 4, 4, 8};

    struct ObjBuffer
    {
        Obj obj;
        BufferType btype;
        uint8_t _pad[3];
        int32_t count;    /* number of elements */
        int32_t capacity; /* allocated elements */
        uint8_t *data;    /* raw byte buffer */
    };

    inline bool is_buffer(Value v) { return is_obj_type(v, OBJ_BUFFER); }
    inline ObjBuffer *as_buffer(Value v) { return (ObjBuffer *)v.as.obj; }

    /* =========================================================
    ** NativeStructDef + ObjNativeStruct — Zero-copy C++ struct binding.
    **
    ** C++ struct vive diretamente num buffer raw. O script acede aos
    ** campos por offset + tipo (como BuLang). Nenhum Value intermediário.
    **
    **   auto *def = vm.register_native_struct("Color", sizeof(Color), ctor, dtor);
    **   vm.add_native_field(def, "r", offsetof(Color, r), FIELD_BYTE);
    **   vm.add_native_field(def, "g", offsetof(Color, g), FIELD_BYTE);
    **
    ** Script: var c = Color(255, 0, 128, 255); print(c.r);
    ** → lê diretamente o byte no offset do buffer. O(1), zero cópia.
    ** ========================================================= */

    enum NativeFieldType : uint8_t
    {
        FIELD_BYTE,    /* uint8_t */
        FIELD_INT,     /* int32_t */
        FIELD_UINT,    /* uint32_t */
        FIELD_FLOAT,   /* float */
        FIELD_DOUBLE,  /* double */
        FIELD_BOOL,    /* bool */
        FIELD_POINTER, /* void* */
    };

    struct NativeFieldDef
    {
        ObjString *name;
        uint16_t offset; /* offsetof(Struct, field) */
        NativeFieldType type;
        bool read_only;
    };

    /* Callbacks for native struct lifecycle */
    typedef void (*NativeStructCtor)(VM *vm, void *buffer, int argc, Value *args);
    typedef void (*NativeStructDtor)(VM *vm, void *buffer);

    struct NativeStructDef
    {
        Obj obj; /* OBJ_NATIVE_STRUCT_DEF */
        ObjString *name;
        uint16_t struct_size; /* sizeof(CppStruct) */
        int16_t num_fields;
        NativeFieldDef *fields; /* array of field descriptors */
        NativeStructCtor ctor;  /* nullable — called on construction */
        NativeStructDtor dtor;  /* nullable — called on GC free */
    };

    struct ObjNativeStruct
    {
        Obj obj;              /* OBJ_NATIVE_STRUCT */
        NativeStructDef *def; /* shared definition */
        void *data;           /* raw buffer of def->struct_size bytes */
    };

    inline bool is_native_struct_def(Value v) { return is_obj_type(v, OBJ_NATIVE_STRUCT_DEF); }
    inline bool is_native_struct(Value v) { return is_obj_type(v, OBJ_NATIVE_STRUCT); }
    inline NativeStructDef *as_native_struct_def(Value v) { return (NativeStructDef *)v.as.obj; }
    inline ObjNativeStruct *as_native_struct(Value v) { return (ObjNativeStruct *)v.as.obj; }

/* Get raw C++ pointer from a Value that is a native struct.
** Usage: Color *c = zen_struct_ptr(args[2], Color);
*/
#define zen_struct_ptr(val, T) ((T *)as_native_struct(val)->data)

    inline bool val_is_native_struct(Value v)
    {
        return is_obj(v) && is_native_struct(v);
    }

    /* =========================================================
    ** ObjStructDef + ObjStruct — Lightweight named tuples.
    **
    **   struct Vec2 { x, y }
    **   var p = Vec2(1.0, 2.0)
    **   print(p.x)  // 1.0
    **
    ** No methods, no inheritance — just positional named fields.
    ** Much lighter than ObjClass/ObjInstance.
    ** ========================================================= */

    struct ObjStructDef
    {
        Obj obj;
        ObjString *name;
        int32_t num_fields;
        ObjString **field_names; /* interned field names (for positional init) */
    };

    struct ObjStruct
    {
        Obj obj;
        ObjStructDef *def; /* shared definition */
        Value *fields;     /* array of def->num_fields Values */
    };

    inline bool is_struct_def(Value v) { return is_obj_type(v, OBJ_STRUCT_DEF); }
    inline bool is_struct(Value v) { return is_obj_type(v, OBJ_STRUCT); }
    inline ObjStructDef *as_struct_def(Value v) { return (ObjStructDef *)v.as.obj; }
    inline ObjStruct *as_struct(Value v) { return (ObjStruct *)v.as.obj; }

    /* =========================================================
    ** ObjClass + ObjInstance — Classes com herança 1 nível.
    **
    ** class Enemy : NativeBase {
    **     var hp = 100;
    **     def attack(target) { ... }
    ** }
    **
    ** ObjClass guarda métodos (ObjFunc*) e o template de fields.
    ** ObjInstance guarda os valores dos fields.
    ** Herança: se method não existe no script, procura no parent (C++).
    ** ========================================================= */

    /* Native class lifecycle callbacks */
    typedef void *(*NativeClassCtor)(VM *vm, int argc, Value *args);
    typedef void (*NativeClassDtor)(VM *vm, void *data);

    struct ObjClass
    {
        Obj obj;
        ObjString *name;
        ObjClass *parent;                         /* NULL ou classe pai (1 nível) */
        ObjMap *methods;                          /* nome → ObjFunc/ObjNative */
        int32_t num_fields;                       /* quantos fields declarados */
        ObjString **field_names;                  /* nomes dos fields (para init) */
        /* Valor inicial por field, vindo de uma atribuição no corpo da classe.
        ** Tem contagem própria porque num_fields cresce noutros sítios
        ** (OP_SETFIELD, ClassBuilder::field): os que ficam de fora deste
        ** array começam em None, como sempre. */
        Value *field_defaults;
        int32_t num_field_defaults;
        Value *vtable;                            /* flat array indexed by selector slot */
        int32_t vtable_size;                      /* allocated slots in vtable */
        Value operator_slots[kOperatorSlotCount]; /* fixed slots for __add__, __str__, ... */
        /* Native class binding */
        NativeClassCtor native_ctor; /* returns void* userdata (NULL = no native ctor) */
        NativeClassDtor native_dtor; /* called on GC free or explicit destroy (NULL = nop) */
        bool persistent;             /* true = instances never collected by GC */
        bool constructable;          /* false = script cannot call ClassName(), only C++ can */
    };

    struct ObjInstance
    {
        Obj obj;
        ObjClass *klass;
        int32_t num_fields; /* actual allocated field count for this instance */
        Value *fields;      /* array de num_fields Values */
        void *native_data;  /* C++ object pointer (from native_ctor), NULL for pure script */
    };

    inline bool is_class(Value v) { return is_obj_type(v, OBJ_CLASS); }
    inline bool is_instance(Value v) { return is_obj_type(v, OBJ_INSTANCE); }
    inline ObjClass *as_class(Value v) { return (ObjClass *)v.as.obj; }
    inline ObjInstance *as_instance(Value v) { return (ObjInstance *)v.as.obj; }

    /* Get native C++ data from an instance Value */
    template <typename T>
    inline T *zen_instance_data(Value v) { return (T *)as_instance(v)->native_data; }

    /* Check if value is instance of a specific class name */
    inline bool val_is_instance_of(Value v, const char *name)
    {
        if (!is_instance(v))
            return false;
        ObjInstance *inst = as_instance(v);
        return inst->klass && inst->klass->name && strcmp(inst->klass->name->chars, name) == 0;
    }

    /* =========================================================
    ** ObjRange — lazy range(start, stop, step)
    ** ========================================================= */

    struct ObjRange
    {
        Obj obj;
        int64_t start;
        int64_t stop;
        int64_t step;
    };

    inline bool is_range(Value v) { return is_obj_type(v, OBJ_RANGE); }
    inline ObjRange *as_range(Value v) { return (ObjRange *)v.as.obj; }

    /* Full truthiness: 0, 0.0, nil, false, "", [], {} are falsy */
    inline bool is_truthy_full(Value v)
    {
        switch (v.type)
        {
        case VAL_NIL:
            return false;
        case VAL_BOOL:
            return v.as.boolean;
        case VAL_INT:
            return v.as.integer != 0;
        case VAL_FLOAT:
            return v.as.number != 0.0;
        case VAL_SMALL_STRING:
            return v.as.sso.len != 0;
        default:
            if (v.as.obj->type == OBJ_STRING)
                return as_string(v)->length != 0;
            if (v.as.obj->type == OBJ_ARRAY)
                return arr_count(as_array(v)) != 0;
            if (v.as.obj->type == OBJ_MAP)
                return as_map(v)->count != 0;
            return true;
        }
    }

} /* namespace zen */

#endif /* ZEN_OBJECT_H */
