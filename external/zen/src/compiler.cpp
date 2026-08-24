/*
** compiler.cpp — Infrastructure: compile(), advance, consume, errors,
** variable/name resolution, register management, scope management.
**
** Split:
**   compiler.cpp              — this file (infrastructure + top-level compile)
**   compiler_expressions.cpp  — Pratt expression parser
**   compiler_statements.cpp   — statement/declaration parsing
*/

#include "compiler.h"

namespace zen
{

    /* =========================================================
    ** Top-level: compile source → ObjFunc*
    ** ========================================================= */

    ObjFunc *Compiler::compile(GC *gc, VM *vm, const char *source, const char *filename)
    {
        gc_ = gc;
        vm_ = vm;

        /* Pause GC during compilation — intermediate objects (strings,
           functions, constants) are not yet rooted and would be swept
           by a stress-mode GC cycle. */
        gc_pause(gc_);

        had_error_ = false;
        panic_mode_ = false;
        silent_ = false;
        error_count_ = 0;
        abort_parse_ = false;
        err_buf_[0] = '\0';
        err_buf_used_ = 0;
        err_count_ = 0;
        current_file_ = filename;
        in_class_ = false;
        class_has_parent_ = false;
        pending_decorator_count_ = 0;
        class_field_count_ = 0;
        class_field_default_count_ = 0;
        class_registry_count_ = 0;

        lexer_.init(source, filename);

        /* Set up the script-level compiler state */
        CompilerState script_state;
        script_state.parent = nullptr;
        script_state.function = new_func(gc_);
        script_state.emitter = Emitter(gc_);
        script_state.local_count = 0;
        script_state.scope_depth = 0;
        script_state.next_reg = 0;
        script_state.max_reg = 0;
        script_state.upvalue_count = 0;
        script_state.loop_depth = 0;
        script_state.is_method = false;
        script_state.eval_mode = false;
        script_state.had_eval_return = false;
        script_state.is_generator = false;
        script_state.global_count = 0;

        script_state.emitter.begin("<module>", 0, filename);
        state_ = &script_state;

        /* Prime the parser */
        advance();

        /* Parse top-level declarations until EOF */
        while (!check(TOK_EOF))
        {
            if (abort_parse_)
                break;
            /* Skip stray newlines at top level */
            if (match(TOK_NEWLINE))
                continue;
            declaration();
        }

        /* Implicit return nil at end of script */
        state_->emitter.emit_abc(OP_LOADNIL, 0, 0, 0, previous_.line);
        state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);

        ObjFunc *script = state_->emitter.end(state_->max_reg);
        script->arity = 0;

        gc_resume(gc_);
        return had_error_ ? nullptr : script;
    }

    ObjFunc *Compiler::compile_eval(GC *gc, VM *vm, const char *source, const char *filename, bool silent)
    {
        gc_ = gc;
        vm_ = vm;
        gc_pause(gc_);
        had_error_ = false;
        panic_mode_ = false;
        silent_ = silent;
        error_count_ = 0;
        abort_parse_ = false;
        err_buf_[0] = '\0';
        err_buf_used_ = 0;
        err_count_ = 0;
        current_file_ = filename;
        in_class_ = false;
        class_has_parent_ = false;
        pending_decorator_count_ = 0;

        lexer_.init(source, filename);

        CompilerState script_state;
        script_state.parent = nullptr;
        script_state.function = new_func(gc_);
        script_state.emitter = Emitter(gc_);
        script_state.local_count = 0;
        script_state.scope_depth = 0;
        script_state.next_reg = 0;
        script_state.max_reg = 0;
        script_state.upvalue_count = 0;
        script_state.loop_depth = 0;
        script_state.is_method = false;
        script_state.eval_mode = true;
        script_state.had_eval_return = false;
        script_state.is_generator = false;
        script_state.global_count = 0;

        script_state.emitter.begin("<eval>", 0, filename);
        state_ = &script_state;

        advance();

        /* EVAL_MODE: parse a single expression, put result in R[0], return it */
        /* Skip leading newlines */
        while (match(TOK_NEWLINE)) {}

        if (!check(TOK_EOF))
        {
            /* Reserve R[0] as the result register so sub-expressions don't stomp on it */
            state_->next_reg = 1;
            expression(0); /* result in R[0] */

            /* Consume any trailing newline then expect EOF */
            while (match(TOK_NEWLINE)) {}
            if (!check(TOK_EOF))
            {
                /* Leftover tokens — this isn't a pure expression (e.g. "x = 1") */
                had_error_ = true;
            }
            else
            {
                state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);
            }
        }
        else
        {
            state_->emitter.emit_abc(OP_LOADNIL, 0, 0, 0, previous_.line);
            state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);
        }

        ObjFunc *script = state_->emitter.end(state_->max_reg);
        script->arity = 0;

        gc_resume(gc_);
        return had_error_ ? nullptr : script;
    }

    /* =========================================================
    ** Lexer interface
    ** ========================================================= */

    void Compiler::advance()
    {
        previous_ = current_;
        for (;;)
        {
            current_ = lexer_.next_token();
            if (current_.type != TOK_ERROR)
                break;
            error_at_current(current_.start);
        }
    }

    void Compiler::consume(TokenType type, const char *msg)
    {
        if (current_.type == type)
        {
            advance();
            return;
        }
        error_at_current(msg);
    }

    bool Compiler::check(TokenType type) const
    {
        return current_.type == type;
    }

    bool Compiler::match(TokenType type)
    {
        if (!check(type))
            return false;
        advance();
        return true;
    }

    /* =========================================================
    ** Error reporting
    ** ========================================================= */

    void Compiler::error(const char *msg)
    {
        error_at(previous_, msg);
    }

    void Compiler::error_at_current(const char *msg)
    {
        error_at(current_, msg);
    }

    void Compiler::error_at(const Token &token, const char *msg)
    {
        if (panic_mode_ || abort_parse_)
            return;
        panic_mode_ = true;
        had_error_ = true;
        error_count_++;

        /* Capture into POD buffer */
        if (err_count_ < kMaxErrorInfos) {
            err_info_[err_count_].line = token.line;
            err_info_[err_count_].offset = err_buf_used_;
            int avail = kErrorBufSize - err_buf_used_;
            if (avail > 1) {
                int n = snprintf(err_buf_ + err_buf_used_, avail, "%s", msg);
                if (n > 0 && n < avail)
                    err_buf_used_ += n + 1; /* include null terminator */
            }
            err_count_++;
        }

        if (!silent_)
        {
            fprintf(stderr, "[line %d] Error", token.line);
            if (token.type == TOK_EOF)
                fprintf(stderr, " at end");
            else if (token.type != TOK_ERROR)
                fprintf(stderr, " at '%.*s'", token.length, token.start);
            fprintf(stderr, ": %s\n", msg);

            if (error_count_ >= kMaxCompileErrors)
            {
                fprintf(stderr, "Too many compile errors (%d). Aborting parse.\n", error_count_);
            }
        }

        /* Fail-fast: stop immediately on first compile error. */
        abort_parse_ = true;
    }

    /* =========================================================
    ** Register allocation
    **
    ** Simple bump allocator. Registers are freed in reverse order.
    ** max_reg tracks the high-water mark for the frame size.
    ** ========================================================= */

    int Compiler::alloc_reg()
    {
        int reg = state_->next_reg++;
        if (state_->next_reg > state_->max_reg)
            state_->max_reg = state_->next_reg;
        if (reg >= kMaxRegisters)
        {
            error("Too many registers needed (expression too complex).");
            return 0;
        }
        return reg;
    }

    void Compiler::free_reg(int reg)
    {
        /* Only free if it's the top register AND not a local */
        if (reg == state_->next_reg - 1)
        {
            /* Check it's not a declared local */
            for (int i = 0; i < state_->local_count; i++)
            {
                if (state_->locals[i].reg == reg)
                    return; /* don't free locals */
            }
            state_->next_reg--;
        }
    }

    void Compiler::emit_move(int dst, int src)
    {
        if (dst != src)
            state_->emitter.emit_abc(OP_MOVE, dst, src, 0, previous_.line);
    }

    /* =========================================================
    ** Scope management
    ** ========================================================= */

    void Compiler::begin_scope()
    {
        state_->scope_depth++;
    }

    void Compiler::end_scope()
    {
        state_->scope_depth--;

        /* Close locals that go out of scope */
        while (state_->local_count > 0 &&
               state_->locals[state_->local_count - 1].depth > state_->scope_depth)
        {
            Local &local = state_->locals[state_->local_count - 1];
            if (local.captured)
            {
                state_->emitter.emit_abc(OP_CLOSE, local.reg, 0, 0, previous_.line);
            }
            state_->local_count--;
            /* Free the register */
            if (local.reg == state_->next_reg - 1)
                state_->next_reg--;
        }
    }

    /* Emit OP_CLOSE for the lowest captured local register (before return/move) */
    void Compiler::close_captured_locals()
    {
        for (int i = 0; i < state_->local_count; i++)
        {
            if (state_->locals[i].captured)
            {
                state_->emitter.emit_abc(OP_CLOSE, state_->locals[i].reg, 0, 0, previous_.line);
                return; /* one OP_CLOSE at lowest reg closes all >= that reg */
            }
        }
    }

    /* =========================================================
    ** Local variable management
    ** ========================================================= */

    int Compiler::declare_local(const Token &name)
    {
        /* Check for redeclaration in same scope */
        for (int i = state_->local_count - 1; i >= 0; i--)
        {
            Local &local = state_->locals[i];
            if (local.depth < state_->scope_depth)
                break;
            if (identifiers_equal(local.name, name))
            {
                error("Variable already declared in this scope.");
                return -1;
            }
        }
        return add_local(name);
    }

    int Compiler::add_local(const Token &name)
    {
        if (state_->local_count >= kMaxLocals)
        {
            error("Too many local variables.");
            return -1;
        }
        int reg = alloc_reg();
        Local &local = state_->locals[state_->local_count++];
        local.name = name;
        local.depth = state_->scope_depth;
        local.reg = reg;
        local.captured = false;
        local.is_const = false;
        local.has_type_hint = false;
        return reg;
    }

    /* =========================================================
    ** Class field table — compile-time O(1) field index lookup
    ** ========================================================= */

    int Compiler::lookup_class_field(ObjString *name) const
    {
        for (int i = 0; i < class_field_count_; i++)
            if (class_field_table_[i] == name) return i;
        return -1;
    }

    int Compiler::add_class_field(ObjString *name)
    {
        int idx = lookup_class_field(name);
        if (idx >= 0) return idx;
        if (class_field_count_ >= kMaxClassFields) return -1;
        class_field_table_[class_field_count_] = name;
        return class_field_count_++;
    }

    bool Compiler::class_field_literal(int &out_const_index)
    {
        bool negate = false;
        if (match(TOK_MINUS))
            negate = true;

        if (match(TOK_INT))
        {
            int64_t v = strtoll(previous_.start, nullptr, 0);
            out_const_index = state_->emitter.add_constant(val_int(negate ? -v : v));
            return true;
        }
        if (match(TOK_FLOAT))
        {
            double v = strtod(previous_.start, nullptr);
            out_const_index = state_->emitter.add_constant(val_float(negate ? -v : v));
            return true;
        }
        if (negate)
            return false; /* "-" followed by something that is not a number */

        if (match(TOK_STRING))
        {
            /* Same quote handling as string_literal(): single or triple. */
            const char *str = previous_.start + 1;
            int len = previous_.length - 2;
            if (previous_.length >= 6 && previous_.start[0] == previous_.start[1] &&
                previous_.start[1] == previous_.start[2])
            {
                str = previous_.start + 3;
                len = previous_.length - 6;
            }
            out_const_index = state_->emitter.add_escaped_string_constant(str, len);
            if (state_->emitter.has_escape_error())
            {
                error(state_->emitter.escape_error());
                state_->emitter.clear_escape_error();
            }
            return true;
        }
        if (match(TOK_TRUE))
        {
            out_const_index = state_->emitter.add_constant(val_bool(true));
            return true;
        }
        if (match(TOK_FALSE))
        {
            out_const_index = state_->emitter.add_constant(val_bool(false));
            return true;
        }
        if (match(TOK_NONE))
        {
            out_const_index = state_->emitter.add_constant(val_nil());
            return true;
        }
        return false;
    }

    /* Returns true when `reg` holds a known instance of the current class.
    ** Always true for self (reg 0 in any method). */
    bool Compiler::is_current_class_instance(int reg) const
    {
        if (!state_->is_method || !in_class_) return false;
        if (reg == 0) return true; /* self is always reg 0 */
        /* Also check type-annotated locals */
        for (int i = 0; i < state_->local_count; i++)
        {
            if (state_->locals[i].reg == reg && state_->locals[i].has_type_hint &&
                identifiers_equal(state_->locals[i].type_hint, current_class_))
                return true;
        }
        return false;
    }

    /* Save current class field table to the registry. Called after class_declaration(). */
    void Compiler::save_class_fields(Token class_name)
    {
        if (class_registry_count_ >= kMaxClasses) return;
        ClassFieldRegistry &reg = class_registry_[class_registry_count_++];
        reg.name = class_name;
        reg.count = class_field_count_;
        for (int i = 0; i < class_field_count_; i++)
            reg.fields[i] = class_field_table_[i];
    }

    /* Look up a previously-compiled class's fields and initialise the
    ** class_field_table_ from them.  Returns true if parent was found. */
    bool Compiler::inherit_class_fields(Token parent_name)
    {
        for (int i = 0; i < class_registry_count_; i++)
        {
            if (identifiers_equal(class_registry_[i].name, parent_name))
            {
                int cnt = class_registry_[i].count;
                class_field_count_ = cnt;
                for (int j = 0; j < cnt; j++)
                    class_field_table_[j] = class_registry_[i].fields[j];
                return true;
            }
        }

        /* Fallback: parent may be a C++ class defined via ClassBuilder.
        ** Look it up in the VM globals and copy its field_names. */
        if (vm_)
        {
            char buf[256];
            int len = parent_name.length < (int)sizeof(buf) - 1
                          ? parent_name.length
                          : (int)sizeof(buf) - 1;
            memcpy(buf, parent_name.start, len);
            buf[len] = '\0';
            int idx = vm_->find_global(buf);
            if (idx >= 0 && is_class(vm_->get_global(idx)))
            {
                ObjClass *pcls = as_class(vm_->get_global(idx));
                int cnt = pcls->num_fields;
                class_field_count_ = cnt;
                for (int j = 0; j < cnt; j++)
                    class_field_table_[j] = pcls->field_names[j];
                return true;
            }
        }

        return false;
    }

    /* =========================================================
    ** Name resolution — the heart of compile-time work.
    **
    ** Searches: locals → upvalues → globals
    ** Returns the register (or emits load instruction).
    ** ========================================================= */

    int Compiler::resolve_local(CompilerState *state, const Token &name)
    {
        for (int i = state->local_count - 1; i >= 0; i--)
        {
            if (identifiers_equal(state->locals[i].name, name))
                return state->locals[i].reg;
        }
        return -1;
    }

    int Compiler::resolve_upvalue(CompilerState *state, const Token &name)
    {
        if (!state->parent)
            return -1;

        /* Check parent's locals */
        int local = resolve_local(state->parent, name);
        if (local != -1)
        {
            state->parent->locals[local].captured = true;
            return add_upvalue(state, local, true);
        }

        /* Check parent's upvalues (recursive) */
        int upvalue = resolve_upvalue(state->parent, name);
        if (upvalue != -1)
            return add_upvalue(state, upvalue, false);

        return -1;
    }

    int Compiler::add_upvalue(CompilerState *state, int index, bool is_local)
    {
        /* Check if already captured */
        for (int i = 0; i < state->upvalue_count; i++)
        {
            if (state->upvalues[i].index == index && state->upvalues[i].is_local == is_local)
                return i;
        }
        if (state->upvalue_count >= kMaxUpvalues)
        {
            error("Too many upvalues in function.");
            return 0;
        }
        state->upvalues[state->upvalue_count].index = index;
        state->upvalues[state->upvalue_count].is_local = is_local;
        return state->upvalue_count++;
    }

    int Compiler::named_variable(const Token &name, int dest, bool can_assign)
    {
        int reg = resolve_local(state_, name);

        if (reg != -1)
        {
            /* Local variable */
            if (can_assign && match(TOK_EQ))
            {
                /* Check const */
                for (int i = state_->local_count - 1; i >= 0; i--)
                {
                    Local &l = state_->locals[i];
                    if (l.reg == reg && l.is_const)
                    {
                        error("Cannot assign to const variable.");
                        return reg;
                    }
                }
                int val = expression(reg);
                if (val != reg)
                    emit_move(reg, val);
                return reg;
            }
            /* Augmented assignment */
            if (can_assign && (check(TOK_PLUS_EQ) || check(TOK_MINUS_EQ) ||
                              check(TOK_STAR_EQ) || check(TOK_SLASH_EQ) ||
                              check(TOK_PERCENT_EQ) || check(TOK_DSLASH_EQ) ||
                              check(TOK_DSTAR_EQ)))
            {
                /* Check const */
                for (int i = state_->local_count - 1; i >= 0; i--)
                {
                    Local &l = state_->locals[i];
                    if (l.reg == reg && l.is_const)
                    {
                        error("Cannot assign to const variable.");
                        return reg;
                    }
                }
                Token op = current_;
                advance();
                int rhs = expression(-1);
                OpCode arith = OP_ADD;
                switch (op.type)
                {
                case TOK_PLUS_EQ:    arith = OP_ADD;  break;
                case TOK_MINUS_EQ:   arith = OP_SUB;  break;
                case TOK_STAR_EQ:    arith = OP_MUL;  break;
                case TOK_SLASH_EQ:   arith = OP_DIV;  break;
                case TOK_PERCENT_EQ: arith = OP_MOD;  break;
                case TOK_DSLASH_EQ:  arith = OP_IDIV; break;
                case TOK_DSTAR_EQ:   arith = OP_POW;  break;
                default: break;
                }
                state_->emitter.emit_abc(arith, reg, reg, rhs, previous_.line);
                free_reg(rhs);
                return reg;
            }
            /* Read */
            if (dest >= 0 && dest != reg)
            {
                emit_move(dest, reg);
                return dest;
            }
            return reg;
        }

        /* Upvalue */
        int upval = resolve_upvalue(state_, name);
        if (upval != -1)
        {
            int r = (dest >= 0) ? dest : alloc_reg();
            if (can_assign && match(TOK_EQ))
            {
                int val = expression(r);
                if (val != r) emit_move(r, val);
                state_->emitter.emit_abc(OP_SETUPVAL, r, upval, 0, previous_.line);
                return r;
            }
            state_->emitter.emit_abc(OP_GETUPVAL, r, upval, 0, previous_.line);
            return r;
        }

        /* Intrinsic: len(x) → OP_LEN */
        if (name.length == 3 && memcmp(name.start, "len", 3) == 0 && check(TOK_LPAREN))
        {
            advance(); /* consume '(' */
            int r = (dest >= 0) ? dest : alloc_reg();
            int arg = expression(-1);
            consume(TOK_RPAREN, "Expected ')' after argument to len().");
            state_->emitter.emit_abc(OP_LEN, r, arg, 0, previous_.line);
            free_reg(arg);
            state_->next_reg = r + 1;
            if (state_->next_reg > state_->max_reg) state_->max_reg = state_->next_reg;
            return r;
        }

        /* Global */
        int gidx = find_or_add_global(name.start, name.length);
        int r = (dest >= 0) ? dest : alloc_reg();

        if (can_assign && match(TOK_EQ))
        {
            int val = expression(r);
            if (val != r) emit_move(r, val);
            state_->emitter.emit_abx(OP_SETGLOBAL, r, gidx, previous_.line);
            return r;
        }
        /* Augmented assignment on global */
        if (can_assign && (check(TOK_PLUS_EQ) || check(TOK_MINUS_EQ) ||
                           check(TOK_STAR_EQ) || check(TOK_SLASH_EQ) ||
                           check(TOK_PERCENT_EQ) || check(TOK_DSLASH_EQ) ||
                           check(TOK_DSTAR_EQ)))
        {
            Token op = current_;
            advance();
            state_->emitter.emit_abx(OP_GETGLOBAL, r, gidx, previous_.line);
            int rhs = expression(-1);
            OpCode arith = OP_ADD;
            switch (op.type)
            {
            case TOK_PLUS_EQ:    arith = OP_ADD;  break;
            case TOK_MINUS_EQ:   arith = OP_SUB;  break;
            case TOK_STAR_EQ:    arith = OP_MUL;  break;
            case TOK_SLASH_EQ:   arith = OP_DIV;  break;
            case TOK_PERCENT_EQ: arith = OP_MOD;  break;
            case TOK_DSLASH_EQ:  arith = OP_IDIV; break;
            case TOK_DSTAR_EQ:   arith = OP_POW;  break;
            default: break;
            }
            state_->emitter.emit_abc(arith, r, r, rhs, previous_.line);
            free_reg(rhs);
            state_->emitter.emit_abx(OP_SETGLOBAL, r, gidx, previous_.line);
            return r;
        }

        /* Read global */
        state_->emitter.emit_abx(OP_GETGLOBAL, r, gidx, previous_.line);
        return r;
    }

    /* =========================================================
    ** Global slot management
    ** ========================================================= */

    int Compiler::require_global_slot(const char *name, int len)
    {
        char buf[256];
        int n = len < 255 ? len : 255;
        memcpy(buf, name, n);
        buf[n] = '\0';

        int gidx = vm_->find_global(buf);
        if (gidx >= 0)
            return gidx;
        return vm_->def_global(buf, val_nil());
    }

    int Compiler::find_or_add_global(const char *name, int len)
    {
        return require_global_slot(name, len);
    }

    /* =========================================================
    ** Helpers
    ** ========================================================= */

    bool Compiler::identifiers_equal(const Token &a, const Token &b) const
    {
        if (a.length != b.length)
            return false;
        return memcmp(a.start, b.start, a.length) == 0;
    }

    ObjString *Compiler::token_string(const Token &t)
    {
        return intern_string(gc_, t.start, t.length);
    }

    bool Compiler::is_declared_global(const Token &name) const
    {
        for (int i = 0; i < state_->global_count; i++)
        {
            if (identifiers_equal(state_->globals[i], name))
                return true;
        }
        return false;
    }

    /* =========================================================
    ** Operator precedence table
    ** ========================================================= */

    int Compiler::get_precedence(TokenType type) const
    {
        switch (type)
        {
        case TOK_IF:        return PREC_TERNARY;
        case TOK_OR:        return PREC_OR;
        case TOK_AND:       return PREC_AND;
        case TOK_EQEQ:
        case TOK_BANGEQ:
        case TOK_LT:
        case TOK_GT:
        case TOK_LTEQ:
        case TOK_GTEQ:
        case TOK_IS:
        case TOK_IN:        return PREC_COMPARISON;
        case TOK_PIPE:      return PREC_BITOR;
        case TOK_CARET:     return PREC_BITXOR;
        case TOK_AMP:       return PREC_BITAND;
        case TOK_LSHIFT:
        case TOK_RSHIFT:    return PREC_SHIFT;
        case TOK_PLUS:
        case TOK_MINUS:     return PREC_TERM;
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
        case TOK_DSLASH:    return PREC_FACTOR;
        case TOK_DSTAR:     return PREC_POWER;
        case TOK_LPAREN:
        case TOK_LBRACKET:
        case TOK_DOT:
        case TOK_QDOT:      return PREC_CALL;
        case TOK_DQMARK:    return PREC_OR; /* ?? at same level as 'or' */
        default:            return PREC_NONE;
        }
    }

    bool Compiler::is_right_associative(TokenType type) const
    {
        return type == TOK_DSTAR; /* ** is right-associative */
    }

} /* namespace zen */
