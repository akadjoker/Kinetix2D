/*
** compiler_statements.cpp — Statement and declaration parsing.
**
** Handles: def, class, if/elif/else, while, for, return,
** break, continue, pass, print, assignment, expression statements.
**
** Block structure: colon + INDENT ... DEDENT (Python style).
*/

#include "compiler.h"

namespace zen
{

    /* =========================================================
    ** Top-level dispatcher
    ** ========================================================= */

    void Compiler::declaration()
    {
        if (abort_parse_)
            return;

        if (panic_mode_)
        {
            /* Synchronize: skip until statement boundary */
            panic_mode_ = false;
            while (!check(TOK_EOF))
            {
                if (check(TOK_NEWLINE) || check(TOK_DEDENT))
                {
                    advance();
                    return;
                }
                switch (current_.type)
                {
                case TOK_DEF:
                case TOK_CLASS:
                case TOK_IF:
                case TOK_WHILE:
                case TOK_FOR:
                case TOK_RETURN:
                case TOK_IMPORT:
                case TOK_FROM:
                case TOK_USING:
                case TOK_ASYNC:
                case TOK_CONST:
                case TOK_ENUM:
                case TOK_RECORD:
                    return;
                default:
                    advance();
                }
            }
            return;
        }

        if (check(TOK_DEF))
        {
            advance();
            fun_declaration();
        }
        else if (check(TOK_ASYNC))
        {
            advance();
            consume(TOK_DEF, "Expected 'def' after 'async'.");
            fun_declaration(true);
        }
        else if (check(TOK_CLASS))
        {
            advance();
            class_declaration();
        }
        else if (check(TOK_RECORD))
        {
            advance();
            struct_declaration();
        }
        else if (check(TOK_ENUM))
        {
            advance();
            enum_declaration();
        }
        else if (check(TOK_CONST))
        {
            advance();
            const_declaration();
        }
        else if (check(TOK_AT))
        {
            /* Collect one or more decorators:
            **   @expr NEWLINE
            **   @expr NEWLINE
            **   def name(...): ...
            */
            pending_decorator_count_ = 0;
            while (match(TOK_AT))
            {
                if (pending_decorator_count_ >= kMaxDecorators)
                {
                    error("Too many decorators (max 8).");
                    break;
                }
                int reg = expression(alloc_reg());
                pending_decorator_regs_[pending_decorator_count_++] = reg;
                match(TOK_NEWLINE);
            }
            /* Now expect def or class */
            if (match(TOK_DEF))
                fun_declaration();
            else if (match(TOK_CLASS))
                class_declaration();
            else
                error("Expected 'def' or 'class' after decorator.");
        }
        else if (check(TOK_IMPORT))
        {
            advance();
            import_statement();
        }
        else if (check(TOK_USING))
        {
            advance();
            using_statement();
        }
        else if (check(TOK_FROM))
        {
            advance();
            from_import_statement();
        }
        else
        {
            statement();
        }

        /* Consume trailing newline or semicolons (semicolon = statement separator) */
        while (match(TOK_SEMICOLON))
        {
            /* After ';', if there's a newline or EOF, stop — otherwise parse next statement */
            if (check(TOK_NEWLINE) || check(TOK_EOF) || check(TOK_DEDENT))
                break;
            /* More statements on same line: re-enter declaration */
            declaration();
            return; /* declaration() will consume its own trailing newline */
        }
        match(TOK_NEWLINE);
    }

    /* =========================================================
    ** def — function declaration
    **
    ** def name(params):
    **     body
    ** ========================================================= */

    void Compiler::fun_declaration(bool force_async)
    {
        consume(TOK_IDENTIFIER, "Expected function name.");
        Token name = previous_;

        /* `def f<T, U>(...)` receives T and U as hidden leading runtime
        ** parameters.  They may then be used or forwarded inside the body. */
        static const int kMaxGenericParams = 32;
        Token generic_params[kMaxGenericParams];
        int generic_count = 0;
        if (match(TOK_LT))
        {
            do
            {
                consume(TOK_IDENTIFIER, "Expected generic parameter name.");
                if (generic_count >= kMaxGenericParams)
                    error("Too many generic parameters.");
                else
                    generic_params[generic_count++] = previous_;
            } while (match(TOK_COMMA));
            consume(TOK_GT, "Expected '>' after generic parameters.");
        }

        CompilerState fn_state;
        fn_state.parent = state_;
        fn_state.function = new_func(gc_);
        fn_state.emitter = Emitter(gc_);
        fn_state.local_count = 0;
        fn_state.scope_depth = 0;
        fn_state.next_reg = 0;
        fn_state.max_reg = 0;
        fn_state.upvalue_count = 0;
        fn_state.loop_depth = 0;
        fn_state.is_method = false;
        fn_state.is_generator = force_async;
        fn_state.global_count = 0;

        char name_buf[256];
        int len = name.length < 255 ? name.length : 255;
        memcpy(name_buf, name.start, len);
        name_buf[len] = '\0';

        fn_state.emitter.begin(name_buf, 0, current_file_);

        /* Pre-declare the function name as a local in the enclosing scope
        ** BEFORE compiling the body, so recursive calls can resolve as upvalue. */
        int local_reg = -1;
        if (state_->scope_depth > 0)
        {
            local_reg = add_local(name);
        }

        CompilerState *enclosing = state_;
        state_ = &fn_state;

        begin_scope();

        /* Parameters */
        consume(TOK_LPAREN, "Expected '(' after function name.");
        int arity = 0;
        for (int gi = 0; gi < generic_count; gi++)
        {
            add_local(generic_params[gi]);
            arity++;
        }
        bool is_vararg = false;
        static const int kMaxDefaults = 32;
        Value default_vals[kMaxDefaults];
        int default_start_idx = -1; /* param index of first default */
        if (!check(TOK_RPAREN))
        {
            do
            {
                if (match(TOK_STAR))
                {
                    /* *args — must be last parameter */
                    consume(TOK_IDENTIFIER, "Expected parameter name after '*'.");
                    add_local(previous_);
                    /* Ignore type hint on *args */
                    if (match(TOK_COLON))
                    {
                        consume(TOK_IDENTIFIER, "Expected type name.");
                        while (match(TOK_DOT)) consume(TOK_IDENTIFIER, "Expected type name.");
                    }
                    arity++;
                    is_vararg = true;
                    break; /* *args must be last */
                }
                consume(TOK_IDENTIFIER, "Expected parameter name.");
                add_local(previous_);
                /* Ignore type hint: param: Type */
                if (match(TOK_COLON))
                {
                    /* consume the type expression: string literal or dotted identifier */
                    if (check(TOK_STRING) || check(TOK_FSTRING))
                    {
                        advance(); /* e.g. 'list[int]' */
                    }
                    else
                    {
                        consume(TOK_IDENTIFIER, "Expected type name.");
                        while (match(TOK_DOT)) consume(TOK_IDENTIFIER, "Expected type name.");
                        if (match(TOK_LBRACKET))
                        {
                            int depth = 1;
                            while (depth > 0 && !check(TOK_EOF))
                            {
                                if (match(TOK_LBRACKET)) depth++;
                                else if (match(TOK_RBRACKET)) depth--;
                                else advance();
                            }
                        }
                    }
                }
                if (match(TOK_EQ))
                {
                    if (default_start_idx < 0) default_start_idx = arity;
                    int di = arity - default_start_idx;
                    if (check(TOK_MINUS)) { advance();
                        if (check(TOK_INT)) { advance(); default_vals[di] = val_int(-(int32_t)strtol(previous_.start, nullptr, 0)); }
                        else if (check(TOK_FLOAT)) { advance(); default_vals[di] = val_float(-strtod(previous_.start, nullptr)); }
                        else { error("Expected number after '-' in default."); default_vals[di] = val_nil(); }
                    }
                    else if (check(TOK_INT))   { advance(); default_vals[di] = val_int((int32_t)strtol(previous_.start, nullptr, 0)); }
                    else if (check(TOK_FLOAT)) { advance(); default_vals[di] = val_float(strtod(previous_.start, nullptr)); }
                    else if (check(TOK_STRING)) {
                        advance();
                        int ki = state_->emitter.add_escaped_string_constant(previous_.start + 1, previous_.length - 2);
                        /* Store ki as a tagged integer sentinel: high bit set */
                        default_vals[di].type = VAL_INT;
                        default_vals[di].as.integer = ki | (1 << 30); /* sentinel */
                    }
                    else if (match(TOK_TRUE))  { default_vals[di] = val_bool(true); }
                    else if (match(TOK_FALSE)) { default_vals[di] = val_bool(false); }
                    else if (match(TOK_NONE))  { default_vals[di] = val_nil(); }
                    else { error("Default value must be a constant."); default_vals[di] = val_nil(); }
                }
                else if (default_start_idx >= 0)
                {
                    error("Non-default parameter cannot follow a default parameter.");
                }
                arity++;
            } while (match(TOK_COMMA));
        }
        consume(TOK_RPAREN, "Expected ')' after parameters.");
        int default_count = (default_start_idx >= 0) ? (arity - default_start_idx) : 0;

        /* Ignore return type annotation: -> Type */
        if (match(TOK_ARROW))
        {
            if (check(TOK_STRING) || check(TOK_FSTRING))
            {
                advance(); /* quoted return type like 'list[int]' */
            }
            else
            {
                consume(TOK_IDENTIFIER, "Expected return type name.");
                while (match(TOK_DOT)) consume(TOK_IDENTIFIER, "Expected type name.");
                if (match(TOK_LBRACKET))
                {
                    int depth = 1;
                    while (depth > 0 && !check(TOK_EOF))
                    {
                        if (match(TOK_LBRACKET)) depth++;
                        else if (match(TOK_RBRACKET)) depth--;
                        else advance();
                    }
                }
            }
        }

        /* Body: colon + indented block */
        colon_block();

        /* Implicit return None */
        state_->emitter.emit_abc(OP_LOADNIL, 0, 0, 0, previous_.line);
        state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);

        ObjFunc *fn = state_->emitter.end(state_->max_reg);
        fn->is_generator = state_->is_generator;
        /* Negative arity = variadic: -(required_params + 1)
           e.g. def f(*args) -> arity=-1 (0 required)
                def f(a, *args) -> arity=-2 (1 required) */
        fn->arity = is_vararg ? -(arity) : arity;

        /* Store default values */
        fn->default_count = default_count;
        if (default_count > 0)
        {
            fn->defaults = (Value *)zen_alloc(gc_, default_count * sizeof(Value));
            for (int di = 0; di < default_count; di++)
            {
                Value v = default_vals[di];
                /* String sentinel: VAL_INT with high bit set encodes constant pool index */
                if (v.type == VAL_INT && (v.as.integer & (1 << 30)))
                    fn->defaults[di] = fn->constants[v.as.integer & ~(1 << 30)];
                else
                    fn->defaults[di] = v;
            }
        }

        /* Copy upvalue descriptors */
        int nuv = state_->upvalue_count;
        fn->upvalue_count = nuv;
        if (nuv > 0)
        {
            fn->upval_descs = (UpvalDesc *)zen_alloc(gc_, nuv * sizeof(UpvalDesc));
            for (int i = 0; i < nuv; i++)
            {
                fn->upval_descs[i].index = (uint8_t)state_->upvalues[i].index;
                fn->upval_descs[i].is_local = state_->upvalues[i].is_local ? 1 : 0;
            }
        }

        /* Restore enclosing state */
        state_ = enclosing;

        /* Store function as closure */
        int ki = state_->emitter.add_constant(val_obj((Obj *)fn));

        if (state_->scope_depth > 0)
        {
            /* Local function — local_reg was pre-declared before body compilation */
            int reg = local_reg;
            state_->emitter.emit_abx(OP_CLOSURE, reg, ki, name.line);

            /* Apply decorators (innermost first, i.e. reverse order) */
            for (int di = pending_decorator_count_ - 1; di >= 0; di--)
            {
                int dec_reg = pending_decorator_regs_[di];
                /* reg = dec_reg(reg): CALL dec_reg, reg, 1 result */
                /* Shift fn to dec_reg+1 so CALL base=dec_reg, args=[fn] */
                int tmp = alloc_reg();
                emit_move(tmp, reg);
                state_->emitter.emit_abc(OP_CALL, dec_reg, 1, 1, name.line);
                /* result lands in dec_reg — move to reg */
                emit_move(reg, dec_reg);
                free_reg(tmp);
                free_reg(dec_reg);
            }
            pending_decorator_count_ = 0;
        }
        else
        {
            /* Global function */
            int gidx = find_or_add_global(name.start, name.length);
            int reg = alloc_reg();
            state_->emitter.emit_abx(OP_CLOSURE, reg, ki, name.line);

            /* Apply decorators (innermost first, i.e. reverse order) */
            for (int di = pending_decorator_count_ - 1; di >= 0; di--)
            {
                int dec_reg = pending_decorator_regs_[di];
                /* OP_CALL: base=dec_reg, arg in dec_reg+1 */
                emit_move(dec_reg + 1, reg);
                state_->emitter.emit_abc(OP_CALL, dec_reg, 1, 1, name.line);
                /* result is in dec_reg */
                emit_move(reg, dec_reg);
                free_reg(dec_reg);
            }
            pending_decorator_count_ = 0;

            state_->emitter.emit_abx(OP_SETGLOBAL, reg, gidx, name.line);
            free_reg(reg);
        }
    }

    /* =========================================================
    ** class declaration
    **
    ** class Name:
    **     def method(self):
    **         body
    ** ========================================================= */

    void Compiler::class_declaration()
    {
        consume(TOK_IDENTIFIER, "Expected class name.");
        Token name = previous_;

        char name_buf[256];
        int len = name.length < 255 ? name.length : 255;
        memcpy(name_buf, name.start, len);
        name_buf[len] = '\0';

        /* Create the class object */
        int gidx = find_or_add_global(name.start, name.length);
        int class_reg = alloc_reg();

        /* Check for parent class: class Child(Parent): */
        int parent_reg = -1;
        Token parent_name_tok;
        parent_name_tok.type = TOK_EOF;
        if (match(TOK_LPAREN))
        {
            consume(TOK_IDENTIFIER, "Expected parent class name.");
            parent_name_tok = previous_;
            parent_reg = alloc_reg();
            named_variable(parent_name_tok, parent_reg, false);
            consume(TOK_RPAREN, "Expected ')' after parent class.");
        }

        /* Initialise field index table: inherit from parent or start fresh */
        if (parent_reg >= 0)
            inherit_class_fields(parent_name_tok); /* copies parent fields + count */
        else
            class_field_count_ = 0;

        /* The values are this class body's own - a subclass inherits the
        ** parent's field NAMES above, and the parent's values come with the
        ** parent class at runtime. */
        const int prev_field_default_count = class_field_default_count_;
        class_field_default_count_ = 0;

        ObjString *class_name = token_string(name);
        int name_ki = state_->emitter.add_constant(val_obj((Obj *)class_name));
        int c_operand = (parent_reg >= 0) ? parent_reg : 255;
        state_->emitter.emit_abc(OP_NEWCLASS, class_reg, name_ki, c_operand, name.line);

        if (parent_reg >= 0)
            free_reg(parent_reg);

        /* Store as global immediately (so methods can reference the class) */
        state_->emitter.emit_abx(OP_SETGLOBAL, class_reg, gidx, name.line);

        /* Save & set class context for super() resolution */
        bool prev_in_class = in_class_;
        bool prev_has_parent = class_has_parent_;
        Token prev_class = current_class_;
        Token prev_parent = current_class_parent_;
        in_class_ = true;
        current_class_ = name;
        class_has_parent_ = (parent_reg >= 0);
        if (class_has_parent_) current_class_parent_ = parent_name_tok;

        /* Parse colon + block of methods */
        consume(TOK_COLON, "Expected ':' after class name.");
        consume(TOK_NEWLINE, "Expected newline after ':'.");
        consume(TOK_INDENT, "Expected indented class body.");

        while (!check(TOK_DEDENT) && !check(TOK_EOF))
        {
            /* Skip blank lines */
            while (match(TOK_NEWLINE)) {}
            if (check(TOK_DEDENT)) break;

            if (match(TOK_DEF))
            {
                /* Method definition */
                consume(TOK_IDENTIFIER, "Expected method name.");
                Token method_name = previous_;

                /* Generic parameters are hidden leading method arguments,
                ** after the implicit `self` receiver. */
                static const int kMaxGenericParams = 32;
                Token generic_params[kMaxGenericParams];
                int generic_count = 0;
                if (match(TOK_LT))
                {
                    do
                    {
                        consume(TOK_IDENTIFIER, "Expected generic parameter name.");
                        if (generic_count >= kMaxGenericParams)
                            error("Too many generic parameters.");
                        else
                            generic_params[generic_count++] = previous_;
                    } while (match(TOK_COMMA));
                    consume(TOK_GT, "Expected '>' after generic parameters.");
                }

                CompilerState fn_state;
                fn_state.parent = state_;
                fn_state.function = new_func(gc_);
                fn_state.emitter = Emitter(gc_);
                fn_state.local_count = 0;
                fn_state.scope_depth = 0;
                fn_state.next_reg = 0;
                fn_state.max_reg = 0;
                fn_state.upvalue_count = 0;
                fn_state.loop_depth = 0;
                fn_state.is_method = true;
                fn_state.is_generator = false;
                fn_state.global_count = 0;

                char fn_name[192];
                snprintf(fn_name, sizeof(fn_name), "%s.%.*s", name_buf, method_name.length, method_name.start);

                fn_state.emitter.begin(fn_name, 0, current_file_);

                CompilerState *enclosing = state_;
                state_ = &fn_state;

                begin_scope();

                /* self is always the first parameter (register 0) */
                Token self_tok;
                self_tok.type = TOK_SELF;
                self_tok.start = "self";
                self_tok.length = 4;
                self_tok.line = method_name.line;
                add_local(self_tok);

                /* Parameters */
                consume(TOK_LPAREN, "Expected '(' after method name.");
                int arity = 0;
                for (int gi = 0; gi < generic_count; gi++)
                {
                    add_local(generic_params[gi]);
                    arity++;
                }
                /* Skip 'self' if user wrote it explicitly as first param */
                if (check(TOK_SELF))
                {
                    advance();
                    match(TOK_COMMA); /* consume trailing comma if present */
                }
                /* Parse remaining params (same logic as fun_declaration) */
                bool is_vararg = false;
                static const int kMaxDefaults = 32;
                Value default_vals[kMaxDefaults];
                int default_start_idx = -1;
                if (!check(TOK_RPAREN))
                {
                    do
                    {
                        if (match(TOK_STAR))
                        {
                            consume(TOK_IDENTIFIER, "Expected parameter name after '*'.");
                            add_local(previous_);
                            if (match(TOK_COLON))
                            {
                                consume(TOK_IDENTIFIER, "Expected type name.");
                                while (match(TOK_DOT)) consume(TOK_IDENTIFIER, "Expected type name.");
                            }
                            arity++;
                            is_vararg = true;
                            break;
                        }
                        consume(TOK_IDENTIFIER, "Expected parameter name.");
                        add_local(previous_);
                        /* Ignore type hint */
                        if (match(TOK_COLON))
                        {
                            if (check(TOK_STRING) || check(TOK_FSTRING))
                                advance();
                            else
                            {
                                consume(TOK_IDENTIFIER, "Expected type name.");
                                while (match(TOK_DOT)) consume(TOK_IDENTIFIER, "Expected type name.");
                                if (match(TOK_LBRACKET))
                                {
                                    int depth = 1;
                                    while (depth > 0 && !check(TOK_EOF))
                                    {
                                        if (match(TOK_LBRACKET)) depth++;
                                        else if (match(TOK_RBRACKET)) depth--;
                                        else advance();
                                    }
                                }
                            }
                        }
                        if (match(TOK_EQ))
                        {
                            if (default_start_idx < 0) default_start_idx = arity;
                            int di = arity - default_start_idx;
                            if (check(TOK_MINUS)) { advance();
                                if (check(TOK_INT)) { advance(); default_vals[di] = val_int(-(int64_t)strtoll(previous_.start, nullptr, 0)); }
                                else if (check(TOK_FLOAT)) { advance(); default_vals[di] = val_float(-strtod(previous_.start, nullptr)); }
                                else { error("Expected number after '-' in default."); default_vals[di] = val_nil(); }
                            }
                            else if (check(TOK_INT))   { advance(); default_vals[di] = val_int((int64_t)strtoll(previous_.start, nullptr, 0)); }
                            else if (check(TOK_FLOAT)) { advance(); default_vals[di] = val_float(strtod(previous_.start, nullptr)); }
                            else if (check(TOK_STRING)) {
                                advance();
                                int ki = state_->emitter.add_escaped_string_constant(previous_.start + 1, previous_.length - 2);
                                default_vals[di].type = VAL_INT;
                                default_vals[di].as.integer = ki | (1 << 30);
                            }
                            else if (match(TOK_TRUE))  { default_vals[di] = val_bool(true); }
                            else if (match(TOK_FALSE)) { default_vals[di] = val_bool(false); }
                            else if (match(TOK_NONE))  { default_vals[di] = val_nil(); }
                            else { error("Default value must be a constant."); default_vals[di] = val_nil(); }
                        }
                        else if (default_start_idx >= 0)
                        {
                            error("Non-default parameter cannot follow a default parameter.");
                        }
                        arity++;
                    } while (match(TOK_COMMA));
                }
                consume(TOK_RPAREN, "Expected ')' after parameters.");
                int default_count = (default_start_idx >= 0) ? (arity - default_start_idx) : 0;

                /* Body */
                colon_block();

                /* Implicit return None (or self for __init__) */
                bool is_init = (method_name.length == 8 &&
                                memcmp(method_name.start, "__init__", 8) == 0);
                if (is_init)
                {
                    state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);
                }
                else
                {
                    state_->emitter.emit_abc(OP_LOADNIL, 0, 0, 0, previous_.line);
                    state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);
                }

                ObjFunc *fn = state_->emitter.end(state_->max_reg);
                fn->is_generator = state_->is_generator;
                fn->arity = is_vararg ? -(arity) : arity;

                /* Store default values */
                fn->default_count = default_count;
                if (default_count > 0)
                {
                    fn->defaults = (Value *)zen_alloc(gc_, default_count * sizeof(Value));
                    for (int di = 0; di < default_count; di++)
                    {
                        Value v = default_vals[di];
                        if (v.type == VAL_INT && (v.as.integer & (1 << 30)))
                            fn->defaults[di] = fn->constants[v.as.integer & ~(1 << 30)];
                        else
                            fn->defaults[di] = v;
                    }
                }

                int nuv = state_->upvalue_count;
                fn->upvalue_count = nuv;
                if (nuv > 0)
                {
                    fn->upval_descs = (UpvalDesc *)zen_alloc(gc_, nuv * sizeof(UpvalDesc));
                    for (int i = 0; i < nuv; i++)
                    {
                        fn->upval_descs[i].index = (uint8_t)state_->upvalues[i].index;
                        fn->upval_descs[i].is_local = state_->upvalues[i].is_local ? 1 : 0;
                    }
                }

                state_ = enclosing;

                /* Store method on class: emit closure, then set on class */
                int ki = state_->emitter.add_constant(val_obj((Obj *)fn));
                int method_reg = alloc_reg();
                state_->emitter.emit_abx(OP_CLOSURE, method_reg, ki, method_name.line);

                /* OP_SETFIELD: class_reg.method_name = method_reg */
                int mname_ki = state_->emitter.add_string_constant(
                    method_name.start, method_name.length);
                state_->emitter.emit_abc(OP_SETFIELD, class_reg, mname_ki, method_reg,
                                         method_name.line);
                free_reg(method_reg);

                match(TOK_NEWLINE);
            }
            else if (match(TOK_PASS))
            {
                match(TOK_NEWLINE);
            }
            else if (check(TOK_IDENTIFIER))
            {
                /* Field declaration: "name = <literal>". Declaring a value on
                ** the class means an instance starts with it, so a script
                ** does not have to write a constructor just to give a field
                ** a starting value. Only literals: the class body is not a
                ** place to run code. */
                advance();
                Token field_name = previous_;
                consume(TOK_EQ, "Expected '=' after field name in class body.");

                int literal_ki = -1;
                if (!class_field_literal(literal_ki))
                {
                    error("A class body field must be a number, string, True, False or None.");
                    while (!check(TOK_NEWLINE) && !check(TOK_DEDENT) && !check(TOK_EOF))
                        advance();
                    match(TOK_NEWLINE);
                    continue;
                }

                const int field_idx = add_class_field(token_string(field_name));
                if (field_idx < 0)
                    error("Too many fields in class body.");
                else if (class_field_default_count_ < kMaxClassFields)
                {
                    class_field_defaults_[class_field_default_count_].field_index = field_idx;
                    class_field_defaults_[class_field_default_count_].const_index = literal_ki;
                    class_field_default_count_++;
                }

                match(TOK_NEWLINE);
            }
            else
            {
                error("Expected method definition in class body.");
                advance();
            }
        }

        consume(TOK_DEDENT, "Expected end of class body.");

        /* Emit OP_CLASSFIELD for each field — pre-registers names in klass at creation time.
        ** This makes SETFIELD_IDX safe (only needs to grow inst->fields, not klass). */
        for (int fi = 0; fi < class_field_count_; fi++)
        {
            int name_ki = state_->emitter.add_string_constant(
                class_field_table_[fi]->chars, class_field_table_[fi]->length);
            if (name_ki <= 255 && fi <= 255)
                state_->emitter.emit_abc(OP_CLASSFIELD, class_reg, fi, name_ki, previous_.line);
        }

        /* Then the values the class body gave them, after every name is
        ** registered so the field index is already valid at runtime. */
        for (int di = 0; di < class_field_default_count_; di++)
        {
            const ClassFieldDefault &def = class_field_defaults_[di];
            if (def.const_index <= 255 && def.field_index <= 255)
                state_->emitter.emit_abc(OP_CLASSFIELDDEF, class_reg, def.field_index,
                                         def.const_index, previous_.line);
        }
        class_field_default_count_ = prev_field_default_count;

        free_reg(class_reg);

        /* Save the completed class field table for potential subclasses */
        save_class_fields(name);

        /* Restore previous class context */
        in_class_ = prev_in_class;
        class_has_parent_ = prev_has_parent;
        current_class_ = prev_class;
        current_class_parent_ = prev_parent;
    }

    /* =========================================================
    ** struct declaration
    **
    ** struct Name { field1, field2, field3 }
    **
    ** Creates an ObjStructDef registered as a global.
    ** Instances are created by calling Name(arg1, arg2, arg3).
    ** ========================================================= */

    void Compiler::struct_declaration()
    {
        consume(TOK_IDENTIFIER, "Expected struct name.");
        Token name_tok = previous_;

        char name_buf[128];
        int nlen = name_tok.length < 127 ? name_tok.length : 127;
        memcpy(name_buf, name_tok.start, nlen);
        name_buf[nlen] = '\0';

        consume(TOK_LBRACE, "Expected '{' before struct body.");

        /* Collect field names */
        char fields[64][64];
        int field_count = 0;

        while (!check(TOK_RBRACE) && !check(TOK_EOF))
        {
            consume(TOK_IDENTIFIER, "Expected field name.");
            int flen = previous_.length < 63 ? previous_.length : 63;
            memcpy(fields[field_count], previous_.start, flen);
            fields[field_count][flen] = '\0';
            field_count++;

            if (!match(TOK_COMMA))
                break;
        }

        consume(TOK_RBRACE, "Expected '}' after struct body.");

        /* Create the struct def in the VM (registers as global) */
        VM::StructBuilder builder = vm_->def_struct(name_buf);
        for (int i = 0; i < field_count; i++)
            builder.field(fields[i]);
        builder.end();
    }

    /* =========================================================
    ** enum declaration
    **
    ** enum State { Idle, Running, Paused }
    ** enum Keys { Space = 32, Enter = 13 }
    **
    ** Creates an ObjMap registered as a global with is_module=true
    ** so dot access works: State.Idle, State.Running, etc.
    ** Values auto-increment from 0 unless explicitly assigned.
    ** ========================================================= */

    void Compiler::enum_declaration()
    {
        consume(TOK_IDENTIFIER, "Expected enum name.");
        Token name_tok = previous_;

        char name_buf[128];
        int nlen = name_tok.length < 127 ? name_tok.length : 127;
        memcpy(name_buf, name_tok.start, nlen);
        name_buf[nlen] = '\0';

        consume(TOK_LBRACE, "Expected '{' before enum body.");

        ObjMap *emap = new_map(gc_);
        emap->is_module = true; /* so dot access bypasses built-in map methods */

        int64_t next_val = 0;

        while (!check(TOK_RBRACE) && !check(TOK_EOF))
        {
            consume(TOK_IDENTIFIER, "Expected enum member name.");
            Token member = previous_;

            int64_t val = next_val;
            if (match(TOK_EQ))
            {
                /* Explicit value: must be an integer literal */
                bool negate = false;
                if (match(TOK_MINUS)) negate = true;
                consume(TOK_INT, "Expected integer value for enum member.");
                const char *s = previous_.start;
                int slen = previous_.length;
                val = 0;
                for (int i = 0; i < slen; i++)
                    val = val * 10 + (s[i] - '0');
                if (negate) val = -val;
            }

            ObjString *key = intern_string(gc_, member.start, member.length,
                                           hash_string(member.start, member.length));
            map_set(gc_, emap, val_obj((Obj *)key), val_int(val));
            next_val = val + 1;

            if (!match(TOK_COMMA))
                break;
        }

        consume(TOK_RBRACE, "Expected '}' after enum body.");

        vm_->def_global(name_buf, val_obj((Obj *)emap));
    }

    /* =========================================================
    ** const NAME = expr
    **
    ** Declares a variable and marks it read-only at compile-time.
    ** Works for both local and global scope.
    ** ========================================================= */

    void Compiler::const_declaration()
    {
        consume(TOK_IDENTIFIER, "Expected constant name after 'const'.");
        Token name_tok = previous_;

        consume(TOK_EQ, "Expected '=' after constant name.");

        if (state_->scope_depth > 0)
        {
            /* Local const */
            int reg = add_local(name_tok);
            state_->locals[state_->local_count - 1].is_const = true;
            int val = expression(reg);
            if (val != reg) emit_move(reg, val);
        }
        else
        {
            /* Global const — just define as global (no runtime enforcement) */
            int gidx = find_or_add_global(name_tok.start, name_tok.length);
            int reg = alloc_reg();
            int val = expression(reg);
            if (val != reg) emit_move(reg, val);
            state_->emitter.emit_abx(OP_SETGLOBAL, reg, gidx, name_tok.line);
            free_reg(reg);
        }
    }

    /* =========================================================
    ** import <module>
    **
    ** import math          — loads module ObjMap into global "math"
    ** ========================================================= */

    void Compiler::import_statement()
    {
        /* Accept both identifiers and 'struct' keyword as module name */
        if (!match(TOK_IDENTIFIER))
        {
            error("Expected module name after 'import'.");
            return;
        }

        /* Build dotted module path: "foo.bar.baz" */
        char mod_path[256];
        int mlen = previous_.length < 250 ? previous_.length : 250;
        memcpy(mod_path, previous_.start, mlen);
        const char *alias_start = previous_.start;
        int alias_len = previous_.length;
        int line = previous_.line;
        while (match(TOK_DOT))
        {
            consume(TOK_IDENTIFIER, "Expected identifier after '.' in module path.");
            if (mlen < 254)
                mod_path[mlen++] = '.';
            int add = previous_.length < (254 - mlen) ? previous_.length : (254 - mlen);
            memcpy(mod_path + mlen, previous_.start, add);
            mlen += add;
            /* The binding name is the last component */
            alias_start = previous_.start;
            alias_len = previous_.length;
        }
        mod_path[mlen] = '\0';

        /* Add module path string as a constant */
        ObjString *mod_str = intern_string(gc_, mod_path, mlen,
                                           hash_string(mod_path, mlen));
        int ki = state_->emitter.add_constant(val_obj((Obj *)mod_str));

        /* Allocate temp register, emit OP_IMPORT */
        int reg = alloc_reg();
        state_->emitter.emit_abx(OP_IMPORT, reg, ki, line);

        /* Store result as global with last component name
        ** e.g. "import pkg.utils" → global "utils" */
        int gidx = find_or_add_global(alias_start, alias_len);
        state_->emitter.emit_abx(OP_SETGLOBAL, reg, gidx, line);
        free_reg(reg);
    }

    /* =========================================================
    ** from <module> import <name1>, <name2>, ...
    **
    ** from math import sin, cos, pi
    ** ========================================================= */

    void Compiler::from_import_statement()
    {
        consume(TOK_IDENTIFIER, "Expected module name after 'from'.");

        /* Build dotted module path: consume "foo.bar.baz" up to "import" keyword */
        char mod_path[256];
        int mlen = previous_.length < 250 ? previous_.length : 250;
        memcpy(mod_path, previous_.start, mlen);
        int line = previous_.line;
        while (match(TOK_DOT))
        {
            if (check(TOK_IMPORT))
                break; /* "from foo. import" — unlikely but safe */
            consume(TOK_IDENTIFIER, "Expected identifier after '.' in module path.");
            if (mlen < 254)
                mod_path[mlen++] = '.';
            int add = previous_.length < (254 - mlen) ? previous_.length : (254 - mlen);
            memcpy(mod_path + mlen, previous_.start, add);
            mlen += add;
        }
        mod_path[mlen] = '\0';

        consume(TOK_IMPORT, "Expected 'import' after module name.");

        /* from <module> import *  (native modules/plugins only) */
        if (match(TOK_STAR))
        {
            const NativeLib *lib = vm_->find_lib(mod_path);
            if (!lib)
                lib = vm_->try_load_plugin(mod_path);
            if (!lib)
            {
                error("from <module> import * is only supported for registered native modules.");
                return;
            }

            /* Mirror `using`: open module and inject unqualified names. */
            vm_->open_lib_globals(lib);
            for (int i = 0; i < lib->num_functions; i++)
                vm_->def_native(lib->functions[i].name, lib->functions[i].fn, lib->functions[i].arity);
            for (int i = 0; i < lib->num_constants; i++)
                vm_->def_global(lib->constants[i].name, lib->constants[i].value);
            return;
        }

        /* Load the module into a temp register */
        ObjString *mod_str = intern_string(gc_, mod_path, mlen,
                                           hash_string(mod_path, mlen));
        int ki = state_->emitter.add_constant(val_obj((Obj *)mod_str));
        int mod_reg = alloc_reg();
        state_->emitter.emit_abx(OP_IMPORT, mod_reg, ki, line);

        /* Loop over comma-separated names */
        do {
            consume(TOK_IDENTIFIER, "Expected identifier after 'import'.");
            Token field_name = previous_;

            /* Add field name string as constant */
            ObjString *field_str = intern_string(gc_, field_name.start, field_name.length,
                                                  hash_string(field_name.start, field_name.length));
            int fki = state_->emitter.add_constant(val_obj((Obj *)field_str));

            /* dest = mod[field_name]:  R[dest] = R[mod_reg].K[fki] */
            int dest = alloc_reg();
            state_->emitter.emit_abc(OP_GETFIELD, dest, mod_reg, fki, field_name.line);

            /* Make it a global */
            int gidx = find_or_add_global(field_name.start, field_name.length);
            state_->emitter.emit_abx(OP_SETGLOBAL, dest, gidx, field_name.line);
            free_reg(dest);

        } while (match(TOK_COMMA));

        free_reg(mod_reg);
    }

    /* =========================================================
    ** using <module>
    **
    ** using raylib;
    **   - opens module symbols
    **   - injects unqualified function/constant names in globals
    ** ========================================================= */
    void Compiler::using_statement()
    {
        do
        {
            consume(TOK_IDENTIFIER, "Expected module name after 'using'.");

            char mod_name[128];
            int n = previous_.length < 127 ? previous_.length : 127;
            memcpy(mod_name, previous_.start, n);
            mod_name[n] = '\0';

            const NativeLib *lib = vm_->find_lib(mod_name);
            if (!lib)
                lib = vm_->try_load_plugin(mod_name);
            if (!lib)
            {
                error("Unknown module in 'using'.");
                return;
            }

            /* Ensure module is opened (qualified names + init_fn side effects). */
            vm_->open_lib_globals(lib);

            /* Inject unqualified aliases into globals, BuLang-style. */
            for (int i = 0; i < lib->num_functions; i++)
                vm_->def_native(lib->functions[i].name, lib->functions[i].fn, lib->functions[i].arity);

            for (int i = 0; i < lib->num_constants; i++)
                vm_->def_global(lib->constants[i].name, lib->constants[i].value);
        } while (match(TOK_COMMA));
    }

    /* =========================================================
    ** Statement dispatcher
    ** ========================================================= */

    void Compiler::statement()
    {
        if (match(TOK_IF))
        {
            if_statement();
        }
        else if (match(TOK_MATCH))
        {
            match_statement();
        }
        else if (match(TOK_WHILE))
        {
            while_statement();
        }
        else if (match(TOK_FOR))
        {
            for_statement();
        }
        else if (match(TOK_RETURN))
        {
            return_statement();
        }
        else if (match(TOK_BREAK))
        {
            break_statement();
        }
        else if (match(TOK_CONTINUE))
        {
            continue_statement();
        }
        else if (match(TOK_PASS))
        {
            pass_statement();
        }
        else if (match(TOK_ASSERT))
        {
            assert_statement();
        }
        else if (match(TOK_DEL))
        {
            del_statement();
        }
        else if (match(TOK_GLOBAL) || match(TOK_NONLOCAL))
        {
            /* Record names so bare assignment writes to global instead of creating local */
            do {
                consume(TOK_IDENTIFIER, "Expected variable name.");
                if (state_->global_count < CompilerState::kMaxGlobals)
                    state_->globals[state_->global_count++] = previous_;
            } while (match(TOK_COMMA));
        }
        else if (match(TOK_PRINT))
        {
            print_statement();
        }
        else
        {
            /* Variable annotation: `name: Type` or `name: Type = expr` — skip type, handle assignment */
            if (check(TOK_IDENTIFIER))
            {
                Token name_tok = current_;
                LexerState saved_lex = lexer_.save_state();
                Token saved_cur = current_;
                Token saved_prev = previous_;
                advance(); /* consume identifier */
                if (check(TOK_COLON))
                {
                    advance(); /* consume ':' */
                    /* skip type expression: string literal, identifier (dotted/generic), etc. */
                    if (check(TOK_STRING) || check(TOK_FSTRING))
                    {
                        advance(); /* quoted type hint like 'list[int]' */
                    }
                    else if (check(TOK_IDENTIFIER))
                    {
                        advance();
                        while (match(TOK_DOT)) consume(TOK_IDENTIFIER, "Expected type name.");
                        if (match(TOK_LBRACKET))
                        {
                            int depth = 1;
                            while (depth > 0 && !check(TOK_EOF))
                            {
                                if (match(TOK_LBRACKET)) depth++;
                                else if (match(TOK_RBRACKET)) depth--;
                                else advance();
                            }
                        }
                    }
                    if (match(TOK_EQ))
                    {
                        /* Compile as assignment: name = expr */
                        int local = resolve_local(state_, name_tok);
                        if (local >= 0)
                        {
                            int val = expression(local);
                            if (val != local) emit_move(local, val);
                        }
                        else
                        {
                        int gidx = find_or_add_global(name_tok.start, name_tok.length);
                            int val = expression(-1);
                            state_->emitter.emit_abx(OP_SETGLOBAL, val, gidx, name_tok.line);
                            free_reg(val);
                        }
                    }
                    /* else: pure annotation — no code */
                    return;
                }
                else
                {
                    /* Not an annotation — restore state and fall through */
                    lexer_.restore_state(saved_lex);
                    current_ = saved_cur;
                    previous_ = saved_prev;
                }
            }

            /* Python semantics: bare `name = expr` inside a function creates/assigns a local.
            ** We detect this BEFORE expression_statement to handle register allocation properly.
            ** Conditions: inside a function, current token is IDENTIFIER, next is '=',
            ** and name is NOT marked with `global`. */
            if (state_->parent != nullptr && check(TOK_IDENTIFIER) && !check(TOK_SELF))
            {
                Token name_tok = current_;
                LexerState saved_lex2 = lexer_.save_state();
                Token saved_cur2 = current_;
                Token saved_prev2 = previous_;
                advance(); /* consume identifier */

                if (check(TOK_EQ) && !is_declared_global(name_tok))
                {
                    advance(); /* consume '=' */

                    /* Check if local already exists */
                    int local = resolve_local(state_, name_tok);
                    if (local >= 0)
                    {
                        /* Check const */
                        for (int i = state_->local_count - 1; i >= 0; i--)
                        {
                            if (state_->locals[i].reg == local && state_->locals[i].is_const)
                            {
                                error("Cannot assign to const variable.");
                                return;
                            }
                        }
                        /* Reassign existing local.
                        ** Use dest=-1 because the RHS may reference this same local
                        ** (e.g. `a = a + 1` or `b = temp + b`). Using dest=local would
                        ** overwrite the local before the RHS finishes reading it. */
                        int val = expression(-1);
                        if (val != local) emit_move(local, val);
                        free_reg(val);
                    }
                    else
                    {
                        /* Create new local at FUNCTION scope depth (depth 1).
                        ** Python has no block scoping — locals created inside if/while/for
                        ** survive after the block ends. */
                        int local_reg = add_local(name_tok);
                        /* Override depth to function level so end_scope doesn't destroy it */
                        state_->locals[state_->local_count - 1].depth = 1;
                        int val = expression(local_reg);
                        if (val != local_reg) emit_move(local_reg, val);
                    }
                    return;
                }
                else
                {
                    /* Not a simple assignment — restore and fall through to expression_statement */
                    lexer_.restore_state(saved_lex2);
                    current_ = saved_cur2;
                    previous_ = saved_prev2;
                }
            }

            expression_statement();
        }
    }

    /* =========================================================
    ** if / elif / else
    **
    ** if cond:
    **     body
    ** elif cond:
    **     body
    ** else:
    **     body
    ** ========================================================= */

    void Compiler::if_statement()
    {
        /* Condition */
        int cond = expression(-1);
        int then_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond, previous_.line);
        free_reg(cond);

        /* Then block */
        colon_block();

        /* Jump over else/elif */
        int else_jump = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);
        state_->emitter.patch_jump(then_jump);

        /* elif chains */
        while (match(TOK_ELIF))
        {
            int elif_cond = expression(-1);
            int elif_jump = state_->emitter.emit_jump(OP_JMPIFNOT, elif_cond, previous_.line);
            free_reg(elif_cond);

            colon_block();

            /* Patch previous else_jump to here, set new one */
            state_->emitter.patch_jump(else_jump);
            else_jump = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);
            state_->emitter.patch_jump(elif_jump);
        }

        /* else */
        if (match(TOK_ELSE))
        {
            colon_block();
        }

        state_->emitter.patch_jump(else_jump);
    }

    /* =========================================================
    ** match statement — pattern matching on value
    **
    ** match expr:
    **     case Pattern1:
    **         body
    **     case Pattern2:
    **         body
    **     case _:
    **         default body
    **
    ** Compiles to equality tests + jumps (no new opcodes).
    ** ========================================================= */

    void Compiler::match_statement()
    {
        int subject = expression(-1);
        consume(TOK_COLON, "Expected ':' after match expression.");
        consume(TOK_NEWLINE, "Expected newline after ':'.");
        consume(TOK_INDENT, "Expected indented block after 'match'.");

        /* Collect end-jumps for each case (to skip remaining cases) */
        int end_jumps[128];
        int end_count = 0;

        while (!check(TOK_DEDENT) && !check(TOK_EOF))
        {
            /* Skip blank lines */
            while (match(TOK_NEWLINE)) {}
            if (check(TOK_DEDENT)) break;

            /* 'default:' — wildcard, always matches */
            if (check(TOK_IDENTIFIER) &&
                current_.length == 7 && memcmp(current_.start, "default", 7) == 0)
            {
                advance(); /* consume 'default' */
                colon_block();
                if (end_count < 128)
                    end_jumps[end_count++] = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);
                continue;
            }

            /* 'case <pattern>:' — equality test */
            if (!check(TOK_IDENTIFIER) ||
                current_.length != 4 || memcmp(current_.start, "case", 4) != 0)
            {
                error("Expected 'case' or 'default' in match block.");
                return;
            }
            advance(); /* consume 'case' */

            /* Parse the case pattern as an expression */
            int pattern = expression(-1);

            /* Emit: tmp = (subject == pattern), jump-if-not to next case */
            int tmp = alloc_reg();
            state_->emitter.emit_abc(OP_EQ, tmp, subject, pattern, previous_.line);
            free_reg(pattern);
            int skip_jump = state_->emitter.emit_jump(OP_JMPIFNOT, tmp, previous_.line);
            free_reg(tmp);

            /* Parse body as colon-block (colon_block consumes ':' itself) */
            colon_block();

            /* Jump to end after body */
            if (end_count < 128)
                end_jumps[end_count++] = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);

            /* Patch skip to here (next case) */
            state_->emitter.patch_jump(skip_jump);
        }

        if (check(TOK_DEDENT)) advance();

        /* Patch all end jumps to here */
        for (int i = 0; i < end_count; i++)
            state_->emitter.patch_jump(end_jumps[i]);

        free_reg(subject);
    }

    /* =========================================================
    ** while loop
    **
    ** while cond:
    **     body
    ** ========================================================= */

    void Compiler::while_statement()
    {
        /* Set up loop info */
        int loop_idx = state_->loop_depth++;
        LoopInfo &loop = state_->loops[loop_idx];
        loop.break_count = 0;
        loop.scope_depth = state_->scope_depth;

        int loop_start = state_->emitter.current_offset();
        loop.start_offset = loop_start;

        /* Condition */
        int cond = expression(-1);
        int exit_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond, previous_.line);
        free_reg(cond);

        /* Body */
        colon_block();

        /* Loop back */
        state_->emitter.emit_loop(loop_start, 0, previous_.line);

        /* Patch exit */
        state_->emitter.patch_jump(exit_jump);

        /* Patch breaks */
        for (int i = 0; i < loop.break_count; i++)
            state_->emitter.patch_jump(loop.breaks[i]);

        state_->loop_depth--;
    }

    /* =========================================================
    ** for loop (for x in iterable)
    **
    ** for x in expr:
    **     body
    **
    ** Compiles to: get iterator, loop with GETINDEX
    ** (simplified: iterates array indices for now)
    ** ========================================================= */

    void Compiler::for_statement()
    {
        begin_scope();

        /* Loop variable(s) — detect tuple unpack: for k, v in ... */
        consume(TOK_IDENTIFIER, "Expected variable name after 'for'.");
        Token var_names[8];
        int var_count = 1;
        var_names[0] = previous_;

        while (check(TOK_COMMA) && var_count < 8)
        {
            advance(); /* consume comma */
            consume(TOK_IDENTIFIER, "Expected variable name in for-unpack.");
            var_names[var_count++] = previous_;
        }

        consume(TOK_IN, "Expected 'in' after variable name.");

        /* Iterable expression — R[iter_reg] */
        int iter_reg = alloc_reg();
        int iter_result = expression(iter_reg);
        if (iter_result != iter_reg) emit_move(iter_reg, iter_result);

        /* Index counter — R[iter_reg+1] (used by FOR_ITER for arrays) */
        int idx_reg = alloc_reg(); /* must be iter_reg + 1 */
        state_->emitter.emit_asbx(OP_LOADI, idx_reg, 0, previous_.line);

        /* Loop variable(s) */
        int var_reg;
        if (var_count == 1)
        {
            var_reg = add_local(var_names[0]);
        }
        else
        {
            /* Tuple unpack: use a temp reg for the iterated value, then unpack */
            var_reg = alloc_reg(); /* temp — holds the array/tuple from iterator */
            for (int vi = 0; vi < var_count; vi++)
                add_local(var_names[vi]);
        }

        /* Loop info */
        int loop_idx = state_->loop_depth++;
        LoopInfo &loop = state_->loops[loop_idx];
        loop.break_count = 0;
        loop.scope_depth = state_->scope_depth;

        int loop_start = state_->emitter.current_offset();
        loop.start_offset = loop_start;

        /* FOR_ITER: R[var_reg] = next(R[iter_reg]); if done → exit */
        int exit_jump = state_->emitter.emit_for_iter(var_reg, iter_reg, previous_.line);

        /* Tuple unpack: R[var_reg] is an array — extract into individual locals */
        if (var_count > 1)
        {
            /* locals were added after var_reg, so they start at var_reg+1 */
            int base_local = var_reg + 1;
            int idx_tmp = alloc_reg(); /* temporary reg to hold index constant */
            for (int vi = 0; vi < var_count; vi++)
            {
                int dest = base_local + vi;
                state_->emitter.emit_asbx(OP_LOADI, idx_tmp, vi, previous_.line);
                state_->emitter.emit_abc(OP_GETINDEX, dest, var_reg, idx_tmp, previous_.line);
            }
            free_reg(idx_tmp);
        }

        /* Body */
        colon_block();

        /* Loop back */
        state_->emitter.emit_loop(loop_start, 0, previous_.line);

        /* Patch exit */
        state_->emitter.patch_for_iter(exit_jump);

        /* Patch breaks */
        for (int i = 0; i < loop.break_count; i++)
            state_->emitter.patch_jump(loop.breaks[i]);

        state_->loop_depth--;
        end_scope();
    }

    /* =========================================================
    ** return [expr]
    ** ========================================================= */

    void Compiler::return_statement()
    {
        if (check(TOK_NEWLINE) || check(TOK_EOF) || check(TOK_DEDENT) || check(TOK_SEMICOLON))
        {
            /* return None */
            close_captured_locals();
            state_->emitter.emit_abc(OP_LOADNIL, 0, 0, 0, previous_.line);
            state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);
        }
        else
        {
            int reg = expression(-1);

            /* Tuple return: return a, b, c
               Evaluate all expressions into temps FIRST, then move to R[0..N-1]
               to avoid clobbering locals used in subsequent expressions. */
            if (check(TOK_COMMA))
            {
                static const int kMaxReturn = 8;
                int tmps[kMaxReturn];
                int count = 0;
                tmps[count++] = reg; /* first expr already evaluated */

                while (match(TOK_COMMA) && count < kMaxReturn)
                {
                    tmps[count++] = expression(-1);
                }

                close_captured_locals();

                /* Move all temps to R[0..count-1]
                   Handle register permutations that may have cycles.
                   
                   For cycles: Save ALL registers that need reordering to temp space first,
                   then do all moves safely from temps.
                */
                
                bool needs_move = false;
                for (int i = 0; i < count; i++)
                {
                    if (tmps[i] != i)
                    {
                        needs_move = true;
                        break;
                    }
                }
                
                if (needs_move)
                {
                    /* Detect register cycles: destination also used as source */
                    bool has_cycle = false;
                    for (int i = 0; i < count && !has_cycle; i++)
                    {
                        if (tmps[i] != i)
                        {
                            for (int j = 0; j < count; j++)
                            {
                                if (tmps[j] == i)
                                {
                                    has_cycle = true;
                                    break;
                                }
                            }
                        }
                    }
                    
                    if (has_cycle)
                    {
                        /* Save all registers to temps first: R[240], R[241], ...
                           Use high register indices that won't conflict with locals.
                           Then do moves from temps. Safe for any permutation.
                        */
                        int temp_base = 240;  /* Use high registers to avoid conflicts with locals */
                        
                        /* Save phase: copy all needed values to temp registers */
                        for (int i = 0; i < count; i++)
                        {
                            if (tmps[i] != i)
                                emit_move(temp_base + i, tmps[i]);
                        }
                        
                        /* Move phase: move from temps to destinations */
                        for (int i = 0; i < count; i++)
                        {
                            if (tmps[i] != i)
                                emit_move(i, temp_base + i);
                        }
                    }
                    else
                    {
                        /* No cycles: straightforward forward moves are safe */
                        for (int i = 0; i < count; i++)
                        {
                            if (tmps[i] != i)
                                emit_move(i, tmps[i]);
                        }
                    }
                }
                state_->emitter.emit_abc(OP_RETURN, 0, count, 0, previous_.line);
            }
            else
            {
                close_captured_locals();
                if (reg != 0) emit_move(0, reg);
                state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);
            }
        }
    }

    /* =========================================================
    ** break / continue
    ** ========================================================= */

    void Compiler::break_statement()
    {
        if (state_->loop_depth == 0)
        {
            error("'break' outside loop.");
            return;
        }
        LoopInfo &loop = state_->loops[state_->loop_depth - 1];
        if (loop.break_count >= 64)
        {
            error("Too many break statements in loop.");
            return;
        }
        loop.breaks[loop.break_count++] = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);
    }

    void Compiler::continue_statement()
    {
        if (state_->loop_depth == 0)
        {
            error("'continue' outside loop.");
            return;
        }
        LoopInfo &loop = state_->loops[state_->loop_depth - 1];
        state_->emitter.emit_loop(loop.start_offset, 0, previous_.line);
    }

    /* =========================================================
    ** pass — do nothing (empty body placeholder)
    ** ========================================================= */

    void Compiler::pass_statement()
    {
        /* Nothing to emit */
    }

    /* =========================================================
    ** assert expr [, msg]
    ** ========================================================= */

    void Compiler::assert_statement()
    {
        int saved = state_->next_reg;
        int cond = expression(-1);
        int msg  = alloc_reg();
        if (match(TOK_COMMA))
        {
            int v = expression(msg);
            if (v != msg) emit_move(msg, v);
        }
        else
        {
            state_->emitter.emit_abc(OP_LOADNIL, msg, 0, 0, previous_.line);
        }
        state_->emitter.emit_abc(OP_ASSERT, cond, msg, 0, previous_.line);
        free_reg(msg);
        free_reg(cond);
        state_->next_reg = saved;
    }

    /* =========================================================
    ** del expr[key]
    ** ========================================================= */

    void Compiler::del_statement()
    {
        /* del name[key] — parse only the container (stop before '[') */
        int container = parse_precedence(PREC_CALL + 1, -1);

        consume(TOK_LBRACKET, "Expected '[' after 'del' target.");
        int key = expression(-1);
        consume(TOK_RBRACKET, "Expected ']' after index.");
        state_->emitter.emit_abc(OP_DELINDEX, container, key, 0, previous_.line);
        free_reg(key);
        free_reg(container);
    }

    /* =========================================================
    ** print(args) — built-in statement (temporary)
    ** ========================================================= */

    void Compiler::print_statement()
    {
        consume(TOK_LPAREN, "Expected '(' after 'print'.");

        if (check(TOK_RPAREN))
        {
            /* print() — just newline */
            advance();
            int reg = alloc_reg();
            state_->emitter.emit_abc(OP_PRINT, reg, 1, 1, previous_.line); /* C=1: no value */
            free_reg(reg);
            return;
        }

        do
        {
            int reg = expression(-1);
            bool last = check(TOK_RPAREN);
            state_->emitter.emit_abc(OP_PRINT, reg, last ? 1 : 0, 0, previous_.line);
            free_reg(reg);
        } while (match(TOK_COMMA));

        consume(TOK_RPAREN, "Expected ')' after print arguments.");
    }

    /* =========================================================
    ** Expression statement (assignment or bare expression)
    ** ========================================================= */

    void Compiler::store_to_lhs(const Token &name, int src_reg)
    {
        int local = resolve_local(state_, name);
        if (local != -1)
        {
            if (local != src_reg) emit_move(local, src_reg);
            return;
        }
        int upval = resolve_upvalue(state_, name);
        if (upval != -1)
        {
            state_->emitter.emit_abc(OP_SETUPVAL, src_reg, upval, 0, name.line);
            return;
        }
        /* Global */
        int gidx = find_or_add_global(name.start, name.length);
        state_->emitter.emit_abx(OP_SETGLOBAL, src_reg, gidx, name.line);
    }

    void Compiler::expression_statement()
    {
        int saved = state_->next_reg;
        int code_saved = state_->emitter.current_offset();
        LexerState lex_saved = lexer_.save_state();
        Token cur_saved = current_;
        Token prev_saved = previous_;
        int glob_saved = vm_->num_globals();

        int reg = expression(-1);

        /* Multi-assign detected when next token is ',' */
        bool is_subscript_lhs = (previous_.type == TOK_RBRACKET);
        bool is_name_lhs      = (previous_.type == TOK_IDENTIFIER);

        if (check(TOK_COMMA) && (is_name_lhs || is_subscript_lhs))
        {
            /* Rewind all emitted code for the first LHS expression */
            state_->emitter.shrink_to(code_saved);
            state_->next_reg = saved;
            vm_->shrink_globals(glob_saved);
            lexer_.restore_state(lex_saved);
            current_  = cur_saved;
            previous_ = prev_saved;

            /* ---- Parse LHS target list ---- */
            static const int kMaxLHS = 8;
            struct LHSTarget {
                enum { NAME, SUBSCRIPT } kind;
                Token name;        /* NAME */
                int container;     /* SUBSCRIPT: reg holding container */
                int index;         /* SUBSCRIPT: reg holding index */
            } targets[kMaxLHS];
            int lhs_count = 0;

            auto parse_one_lhs = [&]() -> bool {
                if (!match(TOK_IDENTIFIER) && !match(TOK_UNDERSCORE))
                    error_at_current("Expected variable name in multi-assign.");
                Token nm = previous_;
                if (match(TOK_LBRACKET))
                {
                    /* subscript LHS: name[expr] */
                    int cont = alloc_reg();
                    /* resolve name → container reg */
                    int local = resolve_local(state_, nm);
                    if (local != -1) emit_move(cont, local);
                    else {
                        int upval = resolve_upvalue(state_, nm);
                        if (upval != -1)
                            state_->emitter.emit_abc(OP_GETUPVAL, cont, upval, 0, nm.line);
                        else {
                            int gidx = find_or_add_global(nm.start, nm.length);
                            state_->emitter.emit_abx(OP_GETGLOBAL, cont, gidx, nm.line);
                        }
                    }
                    int idx = expression(-1);
                    consume(TOK_RBRACKET, "Expected ']' in LHS subscript.");
                    if (lhs_count < kMaxLHS) {
                        targets[lhs_count].kind      = LHSTarget::SUBSCRIPT;
                        targets[lhs_count].container = cont;
                        targets[lhs_count].index     = idx;
                        lhs_count++;
                    }
                }
                else
                {
                    if (lhs_count < kMaxLHS) {
                        targets[lhs_count].kind = LHSTarget::NAME;
                        targets[lhs_count].name = nm;
                        lhs_count++;
                    }
                }
                return true;
            };

            parse_one_lhs();
            while (match(TOK_COMMA))
                parse_one_lhs();

            consume(TOK_EQ, "Expected '=' in multi-assign.");

                /* ---- Evaluate RHS into temps above lhs infra regs ---- */
            int rhs_base = state_->next_reg;
            int rhs_start = state_->emitter.current_offset();
                state_->next_reg = rhs_base + lhs_count;
            if (state_->next_reg > state_->max_reg) state_->max_reg = state_->next_reg;
            int r0 = expression(rhs_base);

            if (check(TOK_COMMA))
            {
                /* Tuple RHS */
                if (r0 != rhs_base) emit_move(rhs_base, r0);
                int rhs_count = 1;
                while (match(TOK_COMMA))
                {
                    if (rhs_count < lhs_count)
                    {
                        int r = rhs_base + rhs_count;
                        int res = expression(r);
                        if (res != r) emit_move(r, res);
                    }
                    else
                    {
                        /* Extra value beyond lhs_count — parse it to consume tokens */
                        expression(-1);
                    }
                    rhs_count++;
                }
                if (rhs_count != lhs_count)
                {
                    char errbuf[128];
                    if (rhs_count < lhs_count)
                        snprintf(errbuf, sizeof(errbuf),
                                 "not enough values to unpack (expected %d, got %d)",
                                 lhs_count, rhs_count);
                    else
                        snprintf(errbuf, sizeof(errbuf),
                                 "too many values to unpack (expected %d, got %d)",
                                 lhs_count, rhs_count);
                    error(errbuf);
                    state_->next_reg = saved;
                    return;
                }
                int needed = rhs_base + lhs_count;
                if (needed > state_->max_reg) state_->max_reg = needed;
                for (int i = 0; i < lhs_count; i++)
                {
                    int src = rhs_base + i;
                    if (targets[i].kind == LHSTarget::NAME)
                        store_to_lhs(targets[i].name, src);
                    else
                        state_->emitter.emit_abc(OP_SETINDEX,
                            targets[i].container, targets[i].index, src, previous_.line);
                }
            }
            else
            {
                /* Single-expression RHS: patch CALL nresults */
                int call_offset = -1;
                for (int off = state_->emitter.current_offset() - 1; off >= rhs_start; off--)
                {
                    Instruction instr = state_->emitter.instruction_at(off);
                    uint32_t op = (instr >> 24);
                    if (op == (uint32_t)OP_CALL || op == (uint32_t)OP_INVOKE) { call_offset = off; break; }
                }
                if (call_offset < 0)
                {
                    error("Multi-assign right-hand side must be a function/method call or tuple.");
                    state_->next_reg = saved;
                    return;
                }
                state_->emitter.patch_c_at(call_offset, lhs_count);
                int result_base = (int)ZEN_A(state_->emitter.instruction_at(call_offset));
                int needed = result_base + lhs_count;
                if (needed > state_->max_reg) state_->max_reg = needed;
                for (int i = 0; i < lhs_count; i++)
                {
                    int src = result_base + i;
                    if (targets[i].kind == LHSTarget::NAME)
                        store_to_lhs(targets[i].name, src);
                    else
                        state_->emitter.emit_abc(OP_SETINDEX,
                            targets[i].container, targets[i].index, src, previous_.line);
                }
            }

            state_->next_reg = saved;
            return;
        }

        free_reg(reg);
        state_->next_reg = saved;
    }

    /* =========================================================
    ** Block parsing: colon + INDENT ... DEDENT
    ** ========================================================= */

    void Compiler::colon_block()
    {
        consume(TOK_COLON, "Expected ':' before block.");

        /* Single-line block: `if cond: stmt` or `if cond: stmt; stmt2` */
        if (!check(TOK_NEWLINE) && !check(TOK_EOF))
        {
            begin_scope();
            declaration();
            while (match(TOK_SEMICOLON) && !check(TOK_NEWLINE) && !check(TOK_EOF))
                declaration();
            end_scope();
            return;
        }

        consume(TOK_NEWLINE, "Expected newline after ':'.");
        consume(TOK_INDENT, "Expected indented block.");

        begin_scope();

        while (!abort_parse_ && !check(TOK_DEDENT) && !check(TOK_EOF))
        {
            if (match(TOK_NEWLINE))
                continue;
            declaration();
            if (abort_parse_)
                break;
        }

        if (!abort_parse_)
            consume(TOK_DEDENT, "Expected end of block.");
        end_scope();
    }

    void Compiler::block()
    {
        while (!abort_parse_ && !check(TOK_DEDENT) && !check(TOK_EOF))
        {
            if (match(TOK_NEWLINE))
                continue;
            declaration();
            if (abort_parse_)
                break;
        }
    }

} /* namespace zen */
