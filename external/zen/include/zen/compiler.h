#ifndef ZEN_COMPILER_H
#define ZEN_COMPILER_H

#include "lexer.h"
#include "emitter.h"
#include "object.h"
#include "vm.h"

namespace zen
{

    /* =========================================================
    ** Compiler — Single-pass, recursive descent, register-based.
    **
    ** Philosophy:
    **   - ALL name resolution happens at compile time
    **   - Locals → register index (zero-cost at runtime)
    **   - Globals → global table index (OP_GETGLOBAL/OP_SETGLOBAL with Bx)
    **   - Upvalues → upvalue array index (OP_GETUPVAL/OP_SETUPVAL)
    **   - Fields → field index when class is known (OP_GETFIELD_IDX)
    **   - Methods → vtable slot (OP_INVOKE_VT, O(1) dispatch)
    **   - Constants → pool index (OP_LOADK with Bx)
    **
    ** The VM does ZERO string lookups at runtime.
    **
    ** Structure:
    **   compile() → top-level, produces ObjFunc* (the script body)
    **   declaration() → def, class, or statement
    **   statement() → if/while/for/return/break/continue/pass/print/expr
    **   expression() → Pratt parser with precedence climbing
    **
    ** Blocks: COLON + INDENT ... DEDENT (Python style)
    ** ========================================================= */

    /* =========================================================
    ** Operator precedence levels (low to high, Python order)
    ** ========================================================= */

    enum Precedence
    {
        PREC_NONE = 0,
        PREC_ASSIGNMENT,    /* = += -= etc */
        PREC_TERNARY,       /* x if cond else y */
        PREC_OR,            /* or */
        PREC_AND,           /* and */
        PREC_NOT,           /* not (unary) */
        PREC_COMPARISON,    /* == != < > <= >= is in */
        PREC_BITOR,         /* | */
        PREC_BITXOR,        /* ^ */
        PREC_BITAND,        /* & */
        PREC_SHIFT,         /* << >> */
        PREC_TERM,          /* + - */
        PREC_FACTOR,        /* * / % // */
        PREC_UNARY,         /* - ~ not */
        PREC_POWER,         /* ** */
        PREC_CALL,          /* () [] . */
        PREC_PRIMARY,
    };

    /* Maximum constants/locals/upvalues per function */
    static const int kMaxLocals = 200;
    static const int kMaxUpvalues = 200;
    static const int kMaxRegisters = 250;

    /* =========================================================
    ** Local variable (compile-time only, not stored at runtime)
    ** ========================================================= */

    struct Local
    {
        Token name;
        int depth;      /* scope depth (0 = function body) */
        int reg;        /* which register holds this local */
        bool captured;  /* true if captured by a closure */
        bool is_const;  /* true if declared with 'const' */
        /* Type hint: `param: TypeName` — used for OP_GETFIELD_IDX */
        bool has_type_hint;
        Token type_hint; /* type name token (if has_type_hint) */
    };

    /* =========================================================
    ** Upvalue descriptor (compile-time, copied to ObjFunc)
    ** ========================================================= */

    struct Upvalue
    {
        int index;      /* local reg in parent, or upvalue index in grandparent */
        bool is_local;  /* true = from parent's locals, false = from parent's upvalues */
    };

    /* =========================================================
    ** Loop tracking (for break/continue)
    ** ========================================================= */

    struct LoopInfo
    {
        int start_offset;   /* bytecode offset of loop condition */
        int scope_depth;    /* scope depth at loop entry */
        int breaks[64];     /* patch offsets for break jumps */
        int break_count;
    };

    /* =========================================================
    ** CompilerState — one per function being compiled.
    ** Forms a linked list (parent) for nested functions.
    ** ========================================================= */

    struct CompilerState
    {
        CompilerState *parent;      /* enclosing function (NULL for script) */
        ObjFunc *function;          /* function being built */
        Emitter emitter;            /* bytecode emitter */

        /* Locals */
        Local locals[kMaxLocals];
        int local_count;
        int scope_depth;

        /* Registers */
        int next_reg;               /* next free register */
        int max_reg;                /* high-water mark */

        /* Upvalues */
        Upvalue upvalues[kMaxUpvalues];
        int upvalue_count;

        /* Loops (nested) */
        static const int kMaxLoopNest = 16;
        LoopInfo loops[kMaxLoopNest];
        int loop_depth;

        /* Global declarations: names marked with `global name` */
        static const int kMaxGlobals = 64;
        Token globals[kMaxGlobals];
        int global_count;

        /* Flags */
        bool is_method;             /* inside a class method? (self in reg 0) */
        bool eval_mode;             /* top-level: last expression becomes return value */
        bool had_eval_return;       /* expression_statement emitted a return in eval mode */
        bool is_generator;          /* function contains yield → mark as generator */
    };

    /* =========================================================
    ** Compiler class
    ** ========================================================= */

    class Compiler
    {
    public:
        ObjFunc *compile(GC *gc, VM *vm, const char *source, const char *filename);
        ObjFunc *compile_eval(GC *gc, VM *vm, const char *source, const char *filename, bool silent = false);
        bool had_error() const { return had_error_; }

        /* --- Error capture (POD, no destructors) --- */
        struct ErrorInfo { int line; int offset; };
        static const int kMaxErrorInfos = 16;
        static const int kErrorBufSize  = 4096;

        int          error_info_count() const { return err_count_; }
        int          error_line(int i)    const { return err_info_[i].line; }
        const char*  error_message(int i) const { return err_buf_ + err_info_[i].offset; }
        const char*  error_buffer()       const { return err_buf_; }

    private:
        /* --- Lexing interface --- */
        void advance();
        void consume(TokenType type, const char *msg);
        bool check(TokenType type) const;
        bool match(TokenType type);

        /* --- Error handling --- */
        void error(const char *msg);
        void error_at_current(const char *msg);
        void error_at(const Token &token, const char *msg);

        /* --- Declarations --- */
        void declaration();
        void fun_declaration(bool force_async = false);
        void class_declaration();
        void struct_declaration();
        void enum_declaration();
        void const_declaration();
        void import_statement();
        void using_statement();
        void from_import_statement();

        /* --- Statements --- */
        void statement();
        void if_statement();
        void match_statement();
        void while_statement();
        void for_statement();
        void return_statement();
        void break_statement();
        void continue_statement();
        void pass_statement();
        void assert_statement();
        void del_statement();
        void print_statement();
        void expression_statement();
        void assignment_or_expr();

        /* --- Block parsing --- */
        void block();           /* consume INDENT, parse until DEDENT */
        void colon_block();     /* consume ':', expect NEWLINE+INDENT, parse block */

        /* --- Expressions (Pratt parser) --- */
        int expression(int dest = -1);
        int parse_precedence(int prec, int dest);
        int prefix_rule(Token token, int dest);
        int infix_rule(Token op, int left, int dest);
        int number(Token token, int dest);
        int string_literal(Token token, int dest);
        int fstring_literal(Token token, int dest);
        int literal(Token token, int dest);
        int variable(Token token, int dest, bool can_assign);
        int unary(Token token, int dest);
        int binary(Token op, int left, int dest);
        int comparison(Token op, int left, int dest);
        int logical_and(int left, int dest);
        int logical_or(int left, int dest);
        int ternary_expr(int true_val, int dest);
        int call_expr(int callee, int dest);
        int generic_call_expr(int callee, int dest);
        int dot_expr(int obj, int dest, bool can_assign);
        int safe_dot_expr(int obj, int dest);
        int null_coalesce(int left, int dest);
        int subscript_expr(int obj, int dest, bool can_assign);
        int slice_expr(int obj, int dest, int start_reg);
        int array_literal(int dest);
        int map_literal(int dest);
        int lambda_expr(int dest);
        int yield_expr(int dest);
        int await_expr(int dest);
        int super_expr(int dest);

        /* --- Argument list --- */
        int argument_list(int base, int initial_nargs = 0);
        int generic_argument_list(int base);
        bool generic_call_ahead();

        /* --- Name resolution (the core of compile-time work) --- */
        int resolve_local(CompilerState *state, const Token &name);
        int resolve_upvalue(CompilerState *state, const Token &name);
        int add_upvalue(CompilerState *state, int index, bool is_local);

        /* --- Variable declaration/assignment --- */
        int declare_local(const Token &name);
        int add_local(const Token &name);
        int named_variable(const Token &name, int dest, bool can_assign);

        /* --- Scope management --- */
        void begin_scope();
        void end_scope();
        void close_captured_locals();
        void store_to_lhs(const Token &name, int src_reg);

        /* --- Register allocation --- */
        int alloc_reg();
        void free_reg(int reg);
        void emit_move(int dst, int src);

        /* --- Global resolution (compile-time lookup) --- */
        int require_global_slot(const char *name, int len);
        int find_or_add_global(const char *name, int len);

        /* --- Operator precedence --- */
        int get_precedence(TokenType type) const;
        bool is_right_associative(TokenType type) const;

        /* --- Helpers --- */
        bool identifiers_equal(const Token &a, const Token &b) const;
        ObjString *token_string(const Token &t);
        bool is_declared_global(const Token &name) const;

        /* --- State --- */
        GC *gc_;
        VM *vm_;
        Lexer lexer_;
        Token current_;
        Token previous_;
        CompilerState *state_;  /* current function state */
        bool had_error_;
        bool panic_mode_;
        bool silent_;          /* suppress error output (for try-compile) */
        int error_count_;      /* number of reported compile errors */
        bool abort_parse_;     /* stop parsing early after too many errors */
        static const int kMaxCompileErrors = 12;
        const char *current_file_;

        /* Error capture buffer (POD) */
        char      err_buf_[kErrorBufSize];
        int       err_buf_used_;
        ErrorInfo err_info_[kMaxErrorInfos];
        int       err_count_;

        /* Class compilation context */
        Token current_class_;       /* name token of class being compiled */
        Token current_class_parent_; /* parent class name token (if any) */
        bool in_class_;             /* true while compiling a class body */
        bool class_has_parent_;     /* true if current class has a parent */

        /* Decorator context — filled by declaration() before fun/class */
        static const int kMaxDecorators = 8;
        int pending_decorator_regs_[kMaxDecorators];
        int pending_decorator_count_;

        /* Class field index table — built as we see self.field = ... in methods.
        ** Enables OP_GETFIELD_IDX / OP_SETFIELD_IDX (O(1) field access).
        ** Also used for params annotated with the current class type. */
        static const int kMaxClassFields = 64;
        static const int kMaxClasses = 32;

        ObjString *class_field_table_[kMaxClassFields]; /* indexed by field order */
        int class_field_count_;

        /* Values a class body gave its fields ("class A:" then "speed = 5.0").
        ** Held until the body closes so they can be emitted after the run of
        ** OP_CLASSFIELD that registers the names. */
        struct ClassFieldDefault
        {
            int field_index;
            int const_index; /* slot in the enclosing function's constant pool */
        };
        ClassFieldDefault class_field_defaults_[kMaxClassFields];
        int class_field_default_count_;

        /* Per-class field registry — saved after each class is compiled.
        ** Allows subclasses to inherit parent field indices. */
        struct ClassFieldRegistry
        {
            Token name;
            ObjString *fields[kMaxClassFields];
            int count;
        };
        ClassFieldRegistry class_registry_[kMaxClasses];
        int class_registry_count_;

        int lookup_class_field(ObjString *name) const;
        int add_class_field(ObjString *name);
        /* Reads one literal as the value of a class body field and puts it in
        ** the constant pool. False if what follows is not a literal. */
        bool class_field_literal(int &out_const_index);
        bool is_current_class_instance(int reg) const; /* true if reg is self or typed param */
        void save_class_fields(Token class_name);
        bool inherit_class_fields(Token parent_name);
    };

} /* namespace zen */

#endif /* ZEN_COMPILER_H */
