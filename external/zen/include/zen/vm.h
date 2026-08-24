#ifndef ZEN_VM_H
#define ZEN_VM_H

#include "memory.h"
#include "opcodes.h"
#include "module.h"

namespace zen
{

    /* Forward declarations */
    struct NativeStructDef;

    /* =========================================================
    ** ZenIO — platform-abstracted file I/O (like SDL_RWops).
    **
    ** Override to redirect all file I/O for embedded/SDL/WASM.
    ** Default implementations use standard C stdio.
    **
    ** Usage from C++ embedding:
    **   ZenCallbacks cb = zen_default_callbacks();
    **   cb.io.open = my_open;  // e.g. SDL_RWFromFile
    **   cb.io.read = my_read;  // e.g. SDL_RWread
    **   ...
    **   vm.set_callbacks(cb);
    ** ========================================================= */

    /* Opaque file handle returned by open(). */
    typedef void *ZenFile;

    struct ZenIO
    {
        /* Open a file. mode: "r", "rb", "w", "wb", "a", "ab".
        ** Returns opaque handle, or nullptr on failure. */
        ZenFile (*open)(const char *path, const char *mode, void *userdata);

        /* Read up to `size` bytes into `buf`. Returns bytes actually read. */
        long (*read)(ZenFile file, void *buf, long size, void *userdata);

        /* Write `size` bytes from `buf`. Returns bytes actually written. */
        long (*write)(ZenFile file, const void *buf, long size, void *userdata);

        /* Seek to offset. whence: 0=SET, 1=CUR, 2=END. Returns new position or -1. */
        long (*seek)(ZenFile file, long offset, int whence, void *userdata);

        /* Close handle. Returns 0 on success. */
        int (*close)(ZenFile file, void *userdata);

        /* Check if file exists (without opening). Returns non-zero if it does. */
        int (*exists)(const char *path, void *userdata);
    };

    /* =========================================================
    ** ZenCallbacks — all platform hooks in one struct.
    ** ========================================================= */
    struct ZenCallbacks
    {
        /* Granular file I/O (open/read/write/seek/close) */
        ZenIO io;

        /* Print output (used by the print statement). */
        void (*print)(const char *str, int len, void *userdata);

        /* Error output (used by runtime errors). */
        void (*print_err)(const char *str, int len, void *userdata);

        /* Opaque user pointer passed to all callbacks. */
        void *userdata;
    };

    /* Returns default callbacks using stdio (fopen/fread/fwrite/fclose). */
    ZenCallbacks zen_default_callbacks();

    /*
    ** CallFrame — estado de uma chamada activa.
    ** Igual ao conceito do toy_vm mas com multi-return.
    */
    struct CallFrame
    {
        ObjClosure *closure; /* closure a executar (nullptr = top-level script) */
        ObjFunc *func;       /* func shortcut (== closure->func) */
        Instruction *ip;     /* instruction pointer (ponteiro directo) */
        Value *base;         /* base dos registos deste frame */
        int ret_reg;         /* registo no caller onde começam os resultados */
        int ret_count;       /* quantos resultados o caller espera (-1=all) */
    };

    /*
    ** VM — estado completo do interpretador.
    **
    ** Classe em vez de struct+funções livres:
    **   - Encapsulamento natural (internals privados)
    **   - Métodos inline no hot path sem overhead
    **   - Permite herdar/mock para testes se necessário
    **   - this implícito em vez de passar vm* em tudo
    */
    class VM
    {
    public:
        VM();
        ~VM();

        /* Não copiável */
        VM(const VM &) = delete;
        VM &operator=(const VM &) = delete;

        /* --- Execução --- */
        void run(ObjFunc *func);
        void run(ObjClosure *closure);
        Value call_global(int idx, Value *args, int nargs);
        Value call_global(const char *name, Value *args, int nargs);
        Value call_fn(Value callee, Value *args, int nargs); /* call any callable from native */

        /* --- Globals (by index — O(1), used at runtime) --- */
        int def_global(const char *name, Value val); /* returns index */
        Value get_global(int idx) const { return globals_[idx]; }
        void set_global(int idx, Value val) { globals_[idx] = val; }

        /* --- Globals (by name — O(n), used at compile/embed time only) --- */
        int find_global(const char *name) const;
        Value get_global(const char *name) const;
        void set_global(const char *name, Value val);
        int num_globals() const { return num_globals_; }
        /* Trim global table back to `n` — used to undo speculative compilation */
        void shrink_globals(int n) { if (n < num_globals_) num_globals_ = n; }
        const char *global_name(int idx) const { return global_names_[idx] ? global_names_[idx]->chars : nullptr; }

        /* --- Natives --- */
        int def_native(const char *name, NativeFn fn, int arity);

        /* --- Module registry --- */
        void register_lib(const NativeLib *lib);  /* make available for import */
        void open_lib(const NativeLib *lib);       /* register + put in globals */
        const NativeLib *find_lib(const char *name) const;
        int open_lib_globals(const NativeLib *lib, bool warn_shadows = false); /* returns base gidx */
        const NativeLib *try_load_plugin(const char *name); /* dlopen fallback */
        void resolve_native_globals(); /* fill nil globals from registered libs */

        /* --- Script module import (.py files) --- */
        /* Try to import a .py file as a module. Returns ObjMap* or nullptr. */
        ObjMap *import_script_module(const char *mod_name);

        /* --- Class builder --- */
        struct ClassBuilder
        {
            ClassBuilder(VM *vm, const char *name);
            ClassBuilder &parent(const char *parent_name);
            ClassBuilder &field(const char *name);
            ClassBuilder &method(const char *name, NativeFn fn, int arity);
            ClassBuilder &ctor(NativeClassCtor fn);       /* native constructor (returns void*) */
            ClassBuilder &dtor(NativeClassDtor fn);       /* native destructor */
            ClassBuilder &persistent(bool p = true);      /* instances NOT managed by GC */
            ClassBuilder &constructable(bool c = true);   /* false = script cannot instantiate */
            ObjClass *end();

        private:
            VM *vm_;
            ObjClass *klass_;
        };
        ClassBuilder def_class(const char *name);

        /* --- Struct builder (lightweight named tuples) --- */
        struct StructBuilder
        {
            StructBuilder(VM *vm, const char *name);
            StructBuilder &field(const char *name);
            ObjStructDef *end();

        private:
            VM *vm_;
            ObjStructDef *def_;
        };
        StructBuilder def_struct(const char *name);

        /* --- Native struct builder (zero-copy C++ struct binding) --- */
        struct NativeStructBuilder
        {
            NativeStructBuilder(VM *vm, const char *name, uint16_t size,
                                NativeStructCtor ctor, NativeStructDtor dtor);
            NativeStructBuilder &field(const char *name, uint16_t offset,
                                       NativeFieldType type, bool read_only = false);
            /* Convenience shortcuts */
            NativeStructBuilder &i32(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_INT, ro); }
            NativeStructBuilder &u32(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_UINT, ro); }
            NativeStructBuilder &f32(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_FLOAT, ro); }
            NativeStructBuilder &f64(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_DOUBLE, ro); }
            NativeStructBuilder &byte(const char *name, uint16_t offset, bool ro = false) { return field(name, offset, FIELD_BYTE, ro); }
            NativeStructBuilder &boolean(const char *name, uint16_t offset, bool ro = false) { return field(name, offset, FIELD_BOOL, ro); }
            NativeStructBuilder &ptr(const char *name, uint16_t offset, bool ro = false)  { return field(name, offset, FIELD_POINTER, ro); }
            NativeStructDef *end();

        private:
            VM *vm_;
            NativeStructDef *def_;
        };
        NativeStructBuilder register_native_struct(const char *name, uint16_t size,
                                                   NativeStructCtor ctor = nullptr,
                                                   NativeStructDtor dtor = nullptr);

        /* Create a native struct instance from C++ (e.g. inside a NativeFn) */
        Value make_native_struct(NativeStructDef *def, Value *args = nullptr, int nargs = 0);

        /* --- Instance API (for C++ embedding) --- */
        Value make_instance(ObjClass *klass);                    /* creates instance, no init */
        Value make_instance(ObjClass *klass, Value *args, int nargs); /* creates + calls init */
        void destroy_instance(Value instance);                   /* destroy persistent instance */
        /* Create a native struct instance from C++ (e.g. inside a NativeFn). */
        Value invoke(Value instance, const char *method, Value *args, int nargs);
        Value invoke(Value instance, int slot, Value *args, int nargs); /* vtable slot, O(1) */
        Value invoke_operator(Value instance, int slot, Value *args, int nargs);

        enum OperatorSlot : int
        {
            SLOT_ADD = 0,
            SLOT_RADD,
            SLOT_SUB,
            SLOT_RSUB,
            SLOT_MUL,
            SLOT_RMUL,
            SLOT_DIV,
            SLOT_RDIV,
            SLOT_MOD,
            SLOT_RMOD,
            SLOT_NEG,
            SLOT_EQ,
            SLOT_LT,
            SLOT_LE,
            SLOT_STR,
            SLOT_OPERATOR_COUNT = kOperatorSlotCount
        };

              /* --- Fiber API --- */
        ObjFiber *new_fiber(ObjClosure *closure, int stack_size = 256, int max_frames = kMaxFrames);
        Value resume_fiber(ObjFiber *fiber, Value val);

        /* --- File I/O (for import) --- */
        void add_search_path(const char *dir);
        char *read_file(const char *path, const char *relative_to, long *out_size,
                        char *resolved_path = nullptr, int resolved_max = 0);

        /* --- Callbacks (platform I/O) --- */
        void set_callbacks(const ZenCallbacks &cb) { callbacks_ = cb; }
        const ZenCallbacks &callbacks() const { return callbacks_; }

        /* --- Strings (para embedding — internadas) --- */
        ObjString *make_string(const char *str, int length = -1);

        /* --- GC --- */
        GC &get_gc() { return gc_; }
        ObjFiber *current_fiber() const { return current_fiber_; }
        void collect();
        void gc_mark_roots(); /* mark all VM roots for GC */

        /* --- Error reporting (for native functions) --- */
        void runtime_error(const char *fmt, ...);

    private:
/*
** Dispatch mode — seleccionado em compile-time:
**   -DZEN_COMPUTED_GOTO   → computed goto (GCC/Clang, ~45% mais rápido)
**   default               → switch (portável, MSVC)
**
** Detecção automática se não definido explicitamente:
*/
#ifndef ZEN_DISPATCH_MODE
#if defined(__GNUC__) || defined(__clang__)
#define ZEN_COMPUTED_GOTO
#endif
#endif

        /* --- Dispatch (hot path) — implementado em vm_dispatch.cpp --- */
        void execute(ObjFiber *fiber);

        /* --- Helpers internos --- */
        ObjUpvalue *capture_upvalue(ObjFiber *fiber, Value *local);
        void close_upvalues(ObjFiber *fiber, Value *last);
        bool call_value(ObjFiber *fiber, Value callee, int nargs, int nresults);
        bool call_closure(ObjFiber *fiber, ObjClosure *closure, int nargs, int nresults);
        void run_nested(ObjClosure *closure);
        char *try_read_cb(const char *path, long *out_size); /* read file via callbacks */

        /* find_global moved to public */

        /* --- Estado --- */
        GC gc_;

        /* Globals — dynamic. Grows on demand via def_global up to
        ** kMaxGlobalsHard (65536, dictated by OP_GETGLOBAL Bx encoding).
        ** Same safety argument as selectors_: callers only hold integer
        ** indices, never raw pointers into the array, so realloc is safe. */
        Value *globals_;
        ObjString **global_names_;
        int num_globals_;
        int globals_capacity_;
        bool grow_globals(int required); /* internal — returns false on hard cap */

        /* Main fiber (script top-level corre aqui) */
        ObjFiber *main_fiber_;
        ObjFiber *current_fiber_; /* fiber actualmente a executar */
        int fiber_depth_;         /* current nested execute() depth */
        int run_depth_;           /* nested VM::run() depth — see run_nested */
        int external_call_stop_depth_; /* return to C++ when nested script call unwinds here */
        bool had_error_;          /* runtime error occurred */

        /* Search paths for include/import */
        static const int MAX_SEARCH_PATHS = 16;
        char *search_paths_[MAX_SEARCH_PATHS];
        int num_search_paths_;

        /* Platform I/O callbacks */
        ZenCallbacks callbacks_;

        /* Module registry */
        static const int MAX_LIBS = 32;
        const NativeLib *libs_[MAX_LIBS];
        int num_libs_;

        /* Loaded plugin handles (for cleanup) */
        static const int MAX_PLUGINS = 16;
        void *plugin_handles_[MAX_PLUGINS];
        int num_plugins_;

        /* Method selector table (for vtable dispatch).
        ** Dynamic: grows on demand via intern_selector().
        ** The array of pointers may move on realloc, but:
        **   - Slot indices (int) returned to callers stay valid forever
        **     (we only append, never reorder).
        **   - The ObjString* objects themselves are GC-allocated and never
        **     move when this array reallocates.
        ** So no caller is at risk of dangling references after a grow. */
        static const int kInitSelectorCapacity = 32;
        ObjString **selectors_;       /* interned method names (heap-allocated) */
        int num_selectors_;
        int selectors_capacity_;

    public:
        bool had_error() const { return had_error_; }

        /* Intern a method name → slot index (0..255). Same name → same slot. */
        int intern_selector(const char *name, int len);
        int find_selector(const char *name, int len) const;
        int num_selectors() const { return num_selectors_; }
        const char *selector_name(int idx) const { return (idx >= 0 && idx < num_selectors_ && selectors_[idx]) ? selectors_[idx]->chars : nullptr; }
    };

} /* namespace zen */

#endif /* ZEN_VM_H */
