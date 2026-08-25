/*
** compiler_expressions.cpp — Pratt expression parser.
**
** Handles: number, string, identifier, unary, binary, comparisons,
** logical and/or, function calls, subscript, dot access, array/map literals.
*/

#include "compiler.h"

namespace zen
{

    /* =========================================================
    ** Top-level expression entry point
    ** ========================================================= */

    int Compiler::expression(int dest)
    {
        return parse_precedence(PREC_ASSIGNMENT + 1, dest);
    }

    /* =========================================================
    ** Pratt parser: precedence climbing
    ** ========================================================= */

    int Compiler::parse_precedence(int prec, int dest)
    {
        advance();
        Token token = previous_;

        /* Prefix */
        int reg = prefix_rule(token, dest);
        if (had_error_)
            return reg;

        /* Infix — keep parsing while the next operator binds tighter */
        for (;;)
        {
            /* Generic call: fn<Type>(args).  Only treat '<' as generic syntax
            ** when its complete shape is <Identifier[, Identifier]*>(.  This
            ** keeps ordinary comparisons such as `a < b` unchanged. */
            if (current_.type == TOK_LT && generic_call_ahead())
            {
                reg = generic_call_expr(reg, dest);
                if (had_error_)
                    return reg;
                continue;
            }

            int infix_prec = get_precedence(current_.type);

            /* Special case: 'if' on a new line is a statement, not ternary operator */
            if (current_.type == TOK_IF && current_.line > previous_.line)
            {
                break;
            }

            /* 'not in' compound operator at PREC_COMPARISON */
            if (current_.type == TOK_NOT && infix_prec == PREC_NONE)
            {
                /* Peek if next would be 'in' — treat as PREC_COMPARISON */
                LexerState ls = lexer_.save_state();
                Token saved_cur = current_;
                advance(); /* consume 'not' */
                if (current_.type == TOK_IN && PREC_COMPARISON >= prec)
                {
                    /* 'not in' is the operator — hand to infix_rule with op=TOK_NOT */
                    Token op = previous_; /* the 'not' token */
                    reg = infix_rule(op, reg, dest);
                    if (had_error_)
                        return reg;
                    continue;
                }
                /* Not 'not in' — restore and break */
                lexer_.restore_state(ls);
                current_ = saved_cur;
                break;
            }
            if (infix_prec < prec)
                break;
            advance();
            Token op = previous_;
            reg = infix_rule(op, reg, dest);
            if (had_error_)
                return reg;
        }

        return reg;
    }

    /* =========================================================
    ** Prefix rules
    ** ========================================================= */

    int Compiler::prefix_rule(Token token, int dest)
    {
        switch (token.type)
        {
        case TOK_INT:
        case TOK_FLOAT:
            return number(token, dest);
        case TOK_STRING:
            return string_literal(token, dest);
        case TOK_FSTRING:
            return fstring_literal(token, dest);
        case TOK_TRUE:
        case TOK_FALSE:
        case TOK_NONE:
            return literal(token, dest);
        case TOK_IDENTIFIER:
        case TOK_UNDERSCORE:
            return variable(token, dest, true);
        case TOK_SELF:
            return variable(token, dest, false);
        case TOK_SUPER:
            return super_expr(dest);
        case TOK_MINUS:
        case TOK_TILDE:
        case TOK_NOT:
            return unary(token, dest);
        case TOK_LPAREN:
        {
            /* Empty tuple: () */
            if (check(TOK_RPAREN))
            {
                advance();
                int reg = (dest >= 0) ? dest : alloc_reg();
                state_->emitter.emit_abc(OP_NEWARRAY, reg, 0, 0, token.line);
                return reg;
            }
            int r = expression(dest);
            /* Tuple: (expr, ...) — comma after first expr → build array */
            if (check(TOK_COMMA))
            {
                /* Array reg must differ from r to avoid overwriting first value */
                int reg = alloc_reg();
                state_->emitter.emit_abc(OP_NEWARRAY, reg, 0, 0, token.line);
                state_->emitter.emit_abc(OP_APPEND, reg, r, 0, token.line);
                while (match(TOK_COMMA))
                {
                    if (check(TOK_RPAREN))
                        break; /* trailing comma */
                    int elem = expression(-1);
                    state_->emitter.emit_abc(OP_APPEND, reg, elem, 0, previous_.line);
                    free_reg(elem);
                }
                consume(TOK_RPAREN, "Expected ')' after tuple.");
                if (dest >= 0 && dest != reg)
                {
                    emit_move(dest, reg);
                    free_reg(reg);
                    return dest;
                }
                return reg;
            }
            consume(TOK_RPAREN, "Expected ')' after expression.");
            return r;
        }
        case TOK_LBRACKET:
            return array_literal(dest);
        case TOK_LBRACE:
            return map_literal(dest);
        case TOK_LAMBDA:
            return lambda_expr(dest);
        case TOK_YIELD:
            return yield_expr(dest);
        case TOK_AWAIT:
            return await_expr(dest);
        default:
            error("Expected expression.");
            return dest >= 0 ? dest : alloc_reg();
        }
    }

    /* =========================================================
    ** Infix rules
    ** ========================================================= */

    int Compiler::infix_rule(Token op, int left, int dest)
    {
        switch (op.type)
        {
        /* Arithmetic / bitwise */
        case TOK_PLUS:
        case TOK_MINUS:
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT:
        case TOK_DSLASH:
        case TOK_DSTAR:
        case TOK_AMP:
        case TOK_PIPE:
        case TOK_CARET:
        case TOK_LSHIFT:
        case TOK_RSHIFT:
            return binary(op, left, dest);

        /* Comparison */
        case TOK_EQEQ:
        case TOK_BANGEQ:
        case TOK_LT:
        case TOK_GT:
        case TOK_LTEQ:
        case TOK_GTEQ:
        case TOK_IS:
        case TOK_IN:
        case TOK_NOT:
            return comparison(op, left, dest);

        /* Logical */
        case TOK_AND:
            return logical_and(left, dest);
        case TOK_OR:
            return logical_or(left, dest);
        case TOK_IF:
            return ternary_expr(left, dest);

        /* Postfix-like (call, subscript, dot) */
        case TOK_LPAREN:
            return call_expr(left, dest);
        case TOK_LBRACKET:
            return subscript_expr(left, dest, true);
        case TOK_DOT:
            return dot_expr(left, dest, true);
        case TOK_QDOT:
            return safe_dot_expr(left, dest);
        case TOK_DQMARK:
            return null_coalesce(left, dest);

        default:
            error("Unexpected infix operator.");
            return left;
        }
    }

    /* =========================================================
    ** Number literal
    ** ========================================================= */

    int Compiler::number(Token token, int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        if (token.type == TOK_INT)
        {
            /* Try small integer (fits in sBx = signed 16-bit) */
            int64_t val = strtoll(token.start, nullptr, 0);
            if (val >= -32768 && val <= 32767)
            {
                state_->emitter.emit_asbx(OP_LOADI, reg, (int)val, token.line);
                return reg;
            }
            /* Large int → constant pool */
            int ki = state_->emitter.add_constant(val_int(val));
            state_->emitter.emit_abx(OP_LOADK, reg, ki, token.line);
            return reg;
        }

        /* Float */
        double val = strtod(token.start, nullptr);
        int ki = state_->emitter.add_constant(val_float(val));
        state_->emitter.emit_abx(OP_LOADK, reg, ki, token.line);
        return reg;
    }

    /* =========================================================
    ** String literal
    ** ========================================================= */

    int Compiler::string_literal(Token token, int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Strip quotes (single or triple) */
        const char *str = token.start + 1;
        int len = token.length - 2;

        /* Check for triple quotes */
        if (token.length >= 6 &&
            token.start[0] == token.start[1] && token.start[1] == token.start[2])
        {
            str = token.start + 3;
            len = token.length - 6;
        }

        int ki = state_->emitter.add_escaped_string_constant(str, len);
        if (state_->emitter.has_escape_error())
        {
            error(state_->emitter.escape_error());
            state_->emitter.clear_escape_error();
        }
        state_->emitter.emit_abx(OP_LOADK, reg, ki, token.line);
        return reg;
    }

    /* =========================================================
    ** F-string: f"hello {expr} world"
    ** Compiles to: str_part + str(expr) + str_part + ...
    ** ========================================================= */

    int Compiler::fstring_literal(Token token, int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Content is between f" and " — strip f" prefix and closing " */
        const char *raw = token.start + 2; /* skip f" */
        int raw_len = token.length - 3;    /* skip f" and closing " */
        /* Handle triple-quoted f-strings: f"""...""" */
        if (token.length >= 8 && token.start[2] == token.start[3] && token.start[3] == token.start[4])
        {
            raw = token.start + 5;
            raw_len = token.length - 8;
        }

        /* Accumulate literal chars to emit as string pieces */
        char buf[4096];
        int buf_len = 0;
        bool has_result = false;
        int line = token.line;

        /* Helper: emit current buf as string constant and ADD to reg */
        auto flush_literal = [&]()
        {
            if (buf_len == 0)
                return;
            int ki = state_->emitter.add_escaped_string_constant(buf, buf_len);
            int tmp = alloc_reg();
            state_->emitter.emit_abx(OP_LOADK, tmp, ki, line);
            if (!has_result)
            {
                emit_move(reg, tmp);
                has_result = true;
            }
            else
            {
                state_->emitter.emit_abc(OP_ADD, reg, reg, tmp, line);
            }
            free_reg(tmp);
            buf_len = 0;
        };

        const char *p = raw;
        const char *end = raw + raw_len;
        while (p < end)
        {
            if (*p == '{' && p + 1 < end && *(p + 1) == '{')
            {
                buf[buf_len++] = '{';
                p += 2;
                continue;
            }
            if (*p == '}' && p + 1 < end && *(p + 1) == '}')
            {
                buf[buf_len++] = '}';
                p += 2;
                continue;
            }
            if (*p == '{')
            {
                /* Start of expression */
                flush_literal();
                p++; /* skip { */
                /* Find matching } (no nested braces for now) */
                const char *expr_start = p;
                while (p < end && *p != '}')
                    p++;
                if (p >= end)
                {
                    error("Unterminated '{' in f-string.");
                    break;
                }
                int expr_len = (int)(p - expr_start);
                p++; /* skip } */

                /* Compile the expression by feeding it to a sub-lexer */
                char expr_buf[1024];
                if (expr_len >= (int)sizeof(expr_buf) - 4)
                {
                    error("F-string expression too long.");
                    break;
                }
                memcpy(expr_buf, expr_start, expr_len);
                expr_buf[expr_len] = '\n';
                expr_buf[expr_len + 1] = '\0';

                /* Save lexer + token state, init sub-lexer for expression */
                LexerState saved_lex = lexer_.save_state();
                Token saved_prev = previous_;
                Token saved_curr = current_;

                lexer_.init(expr_buf, current_file_);
                advance(); /* prime current_ */

                int expr_reg = expression(-1);

                /* Restore lexer */
                lexer_.restore_state(saved_lex);
                previous_ = saved_prev;
                current_ = saved_curr;

                /* Emit: str(expr_reg) */
                int str_gidx = find_or_add_global("str", 3);
                int str_reg = alloc_reg();
                int arg_reg = alloc_reg();
                state_->emitter.emit_abx(OP_GETGLOBAL, str_reg, str_gidx, line);
                emit_move(arg_reg, expr_reg);
                state_->emitter.emit_abc(OP_CALL, str_reg, 1, 1, line);
                free_reg(arg_reg);
                /* result in str_reg */

                if (!has_result)
                {
                    emit_move(reg, str_reg);
                    has_result = true;
                }
                else
                {
                    state_->emitter.emit_abc(OP_ADD, reg, reg, str_reg, line);
                }
                free_reg(str_reg);
                if (expr_reg != reg && expr_reg != str_reg)
                    free_reg(expr_reg);
                continue;
            }
            /* Regular char — handle escape sequences for the buffer */
            if (*p == '\\' && p + 1 < end)
            {
                p++;
                switch (*p)
                {
                case 'n':
                    buf[buf_len++] = '\n';
                    break;
                case 't':
                    buf[buf_len++] = '\t';
                    break;
                case 'r':
                    buf[buf_len++] = '\r';
                    break;
                case '\\':
                    buf[buf_len++] = '\\';
                    break;
                case '"':
                    buf[buf_len++] = '"';
                    break;
                case '\'':
                    buf[buf_len++] = '\'';
                    break;
                default:
                    buf[buf_len++] = '\\';
                    buf[buf_len++] = *p;
                    break;
                }
                p++;
            }
            else
            {
                buf[buf_len++] = *p++;
            }
        }
        flush_literal();

        /* If no content at all, emit empty string */
        if (!has_result)
        {
            int ki = state_->emitter.add_escaped_string_constant("", 0);
            state_->emitter.emit_abx(OP_LOADK, reg, ki, line);
        }

        return reg;
    }

    /* =========================================================
    ** Literals: True, False, None
    ** ========================================================= */

    int Compiler::literal(Token token, int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();
        switch (token.type)
        {
        case TOK_TRUE:
            state_->emitter.emit_abc(OP_LOADBOOL, reg, 1, 0, token.line);
            break;
        case TOK_FALSE:
            state_->emitter.emit_abc(OP_LOADBOOL, reg, 0, 0, token.line);
            break;
        case TOK_NONE:
            state_->emitter.emit_abc(OP_LOADNIL, reg, 0, 0, token.line);
            break;
        default:
            break;
        }
        return reg;
    }

    /* =========================================================
    ** Variable access (identifier)
    ** ========================================================= */

    int Compiler::variable(Token token, int dest, bool can_assign)
    {
        /* Intercept eval("...") as OP_EVAL intrinsic */
        if (token.length == 4 && memcmp(token.start, "eval", 4) == 0 && check(TOK_LPAREN))
        {
            advance(); /* consume '(' */
            int reg = (dest >= 0) ? dest : alloc_reg();
            /* Allocate arg slot one above result */
            int arg_reg = reg + 1;
            while (state_->next_reg <= arg_reg)
                alloc_reg();
            expression(arg_reg);
            consume(TOK_RPAREN, "Expected ')' after eval argument.");
            state_->emitter.emit_abc(OP_EVAL, reg, arg_reg, 0, token.line);
            state_->next_reg = reg + 1;
            return reg;
        }
        return named_variable(token, dest, can_assign);
    }

    /* =========================================================
    ** Unary operators: -x, ~x, not x
    ** ========================================================= */

    int Compiler::unary(Token token, int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();
        int operand = parse_precedence(PREC_UNARY, -1);

        switch (token.type)
        {
        case TOK_MINUS:
            state_->emitter.emit_abc(OP_NEG, reg, operand, 0, token.line);
            break;
        case TOK_TILDE:
            state_->emitter.emit_abc(OP_BNOT, reg, operand, 0, token.line);
            break;
        case TOK_NOT:
            state_->emitter.emit_abc(OP_NOT, reg, operand, 0, token.line);
            break;
        default:
            break;
        }
        if (operand != reg)
            free_reg(operand);
        return reg;
    }

    /* =========================================================
    ** Binary operators
    ** ========================================================= */

    int Compiler::binary(Token op, int left, int dest)
    {
        int prec = get_precedence(op.type);
        int adjust = is_right_associative(op.type) ? 0 : 1;
        int right = parse_precedence(prec + adjust, -1);
        int reg = (dest >= 0) ? dest : alloc_reg();

        OpCode opcode;
        switch (op.type)
        {
        case TOK_PLUS:
            opcode = OP_ADD;
            break;
        case TOK_MINUS:
            opcode = OP_SUB;
            break;
        case TOK_STAR:
            opcode = OP_MUL;
            break;
        case TOK_SLASH:
            opcode = OP_DIV;
            break;
        case TOK_PERCENT:
            opcode = OP_MOD;
            break;
        case TOK_DSLASH:
            opcode = OP_IDIV;
            break;
        case TOK_DSTAR:
            opcode = OP_POW;
            break;
        case TOK_AMP:
            opcode = OP_BAND;
            break;
        case TOK_PIPE:
            opcode = OP_BOR;
            break;
        case TOK_CARET:
            opcode = OP_BXOR;
            break;
        case TOK_LSHIFT:
            opcode = OP_SHL;
            break;
        case TOK_RSHIFT:
            opcode = OP_SHR;
            break;
        default:
            opcode = OP_ADD;
            break;
        }

        state_->emitter.emit_abc(opcode, reg, left, right, op.line);
        if (right != reg)
            free_reg(right);
        if (left != reg)
            free_reg(left);
        return reg;
    }

    /* =========================================================
    ** Comparison operators
    ** ========================================================= */

    int Compiler::comparison(Token op, int left, int dest)
    {
        /* 'not in' — consume 'in' first, then parse RHS */
        if (op.type == TOK_NOT)
        {
            if (!match(TOK_IN))
            {
                error("Expected 'in' after 'not' in comparison.");
                return left;
            }
            int right = parse_precedence(PREC_COMPARISON + 1, -1);
            int reg = (dest >= 0) ? dest : alloc_reg();
            state_->emitter.emit_abc(OP_CONTAINS, reg, left, right, op.line);
            state_->emitter.emit_abc(OP_NOT, reg, reg, 0, op.line);
            if (right != reg)
                free_reg(right);
            if (left != reg)
                free_reg(left);
            return reg;
        }

        int right = parse_precedence(get_precedence(op.type) + 1, -1);
        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Helper: emit a single comparison op into reg */
        auto emit_cmp = [&](TokenType t, int lhs, int rhs)
        {
            switch (t)
            {
            case TOK_EQEQ:
                state_->emitter.emit_abc(OP_EQ, reg, lhs, rhs, op.line);
                break;
            case TOK_BANGEQ:
                state_->emitter.emit_abc(OP_EQ, reg, lhs, rhs, op.line);
                state_->emitter.emit_abc(OP_NOT, reg, reg, 0, op.line);
                break;
            case TOK_LT:
                state_->emitter.emit_abc(OP_LT, reg, lhs, rhs, op.line);
                break;
            case TOK_GT:
                state_->emitter.emit_abc(OP_LT, reg, rhs, lhs, op.line);
                break;
            case TOK_LTEQ:
                state_->emitter.emit_abc(OP_LE, reg, lhs, rhs, op.line);
                break;
            case TOK_GTEQ:
                state_->emitter.emit_abc(OP_LE, reg, rhs, lhs, op.line);
                break;
            case TOK_IS:
                state_->emitter.emit_abc(OP_IS, reg, lhs, rhs, op.line);
                break;
            case TOK_IN:
                state_->emitter.emit_abc(OP_CONTAINS, reg, lhs, rhs, op.line);
                break;
            default:
                break;
            }
        };

        emit_cmp(op.type, left, right);
        if (left != reg && left != right)
            free_reg(left);

        /* Chained comparisons: 0 < x < 10 → (0 < x) and (x < 10) */
        static const TokenType chain_ops[] = {
            TOK_LT, TOK_GT, TOK_LTEQ, TOK_GTEQ, TOK_EQEQ, TOK_BANGEQ};
        while (true)
        {
            TokenType next = current_.type;
            bool is_chain = false;
            for (auto ct : chain_ops)
                if (next == ct)
                {
                    is_chain = true;
                    break;
                }
            if (!is_chain)
                break;

            /* Short-circuit: if first comparison false, skip rest */
            int and_jump = state_->emitter.emit_jump(OP_JMPIFNOT, reg, previous_.line);

            /* Advance past the operator */
            Token chain_op = current_;
            advance();

            /* Parse next operand; keep right alive as new left */
            int new_right = parse_precedence(PREC_COMPARISON + 1, -1);
            emit_cmp(chain_op.type, right, new_right);

            /* Free old right, new left for next iteration */
            if (right != reg)
                free_reg(right);
            right = new_right;

            state_->emitter.patch_jump(and_jump);
        }

        if (right != reg)
            free_reg(right);
        return reg;
    }

    /* =========================================================
    ** Logical and/or (short-circuit)
    ** ========================================================= */

    int Compiler::logical_and(int left, int dest)
    {
        int reg;
        if (dest >= 0)
        {
            reg = dest;
        }
        else
        {
            bool left_is_local = false;
            for (int i = 0; i < state_->local_count; i++)
                if (state_->locals[i].reg == left)
                {
                    left_is_local = true;
                    break;
                }
            reg = left_is_local ? alloc_reg() : left;
        }
        if (left != reg)
            emit_move(reg, left);

        /* If falsy, jump over RHS (short-circuit: keep LHS value) */
        int jump = state_->emitter.emit_jump(OP_JMPIFNOT, reg, previous_.line);

        /* Free left only if it's a different temporary from reg */
        if (left != reg)
            free_reg(left);

        /* Ensure next_reg is above reg so RHS sub-expressions don't overwrite it */
        int saved_next = state_->next_reg;
        if (state_->next_reg <= reg)
            state_->next_reg = reg + 1;

        int right = parse_precedence(PREC_AND + 1, reg);
        if (right != reg)
            emit_move(reg, right);

        state_->next_reg = saved_next > state_->next_reg ? saved_next : state_->next_reg;
        state_->emitter.patch_jump(jump);
        return reg;
    }

    /* =========================================================
    ** Ternary: true_val if condition else false_val
    ** `left` already holds true_val (evaluated before `if` token)
    ** ========================================================= */

    int Compiler::ternary_expr(int true_val, int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Move true_val into result reg */
        if (true_val != reg)
            emit_move(reg, true_val);

        /* condition */
        int cond = parse_precedence(PREC_OR, -1);

        /* JMPIFNOT: if cond is false, jump to else branch */
        int jump_to_else = state_->emitter.emit_jump(OP_JMPIFNOT, cond, previous_.line);
        free_reg(cond);

        /* Jump over else branch (taken when condition is true) */
        int jump_over_else = state_->emitter.emit_jump(OP_JMP, 0, previous_.line);

        /* Patch jump_to_else here (else branch) */
        state_->emitter.patch_jump(jump_to_else);

        consume(TOK_ELSE, "Expected 'else' in ternary expression.");

        int false_val = parse_precedence(PREC_TERNARY + 1, reg);
        if (false_val != reg)
            emit_move(reg, false_val);

        /* Patch jump over else */
        state_->emitter.patch_jump(jump_over_else);

        if (true_val != reg)
            free_reg(true_val);
        if (false_val != reg)
            free_reg(false_val);
        return reg;
    }

    int Compiler::logical_or(int left, int dest)
    {
        int reg;
        if (dest >= 0)
        {
            reg = dest;
        }
        else
        {
            bool left_is_local = false;
            for (int i = 0; i < state_->local_count; i++)
                if (state_->locals[i].reg == left)
                {
                    left_is_local = true;
                    break;
                }
            reg = left_is_local ? alloc_reg() : left;
        }
        if (left != reg)
            emit_move(reg, left);

        /* If truthy, jump over RHS (short-circuit: keep LHS value) */
        int jump = state_->emitter.emit_jump(OP_JMPIF, reg, previous_.line);

        /* Free left only if it's a different temporary from reg */
        if (left != reg)
            free_reg(left);

        /* Ensure next_reg is above reg so RHS sub-expressions don't overwrite it */
        int saved_next = state_->next_reg;
        if (state_->next_reg <= reg)
            state_->next_reg = reg + 1;

        int right = parse_precedence(PREC_OR + 1, reg);
        if (right != reg)
            emit_move(reg, right);

        state_->next_reg = saved_next > state_->next_reg ? saved_next : state_->next_reg;
        state_->emitter.patch_jump(jump);
        return reg;
    }

    /* =========================================================
    ** Function call: callee(args...)
    ** ========================================================= */

    int Compiler::call_expr(int callee, int dest)
    {
        /* Move callee to a fresh register if it's a local — OP_CALL overwrites R[base] with result */
        int base = callee;
        int saved_next = state_->next_reg;
        if (base < saved_next)
        {
            base = alloc_reg();
            emit_move(base, callee);
        }

        /* Parse arguments into consecutive registers after callee */
        int nargs = argument_list(base);

        consume(TOK_RPAREN, "Expected ')' after arguments.");

        /* OP_CALL: R[base](R[base+1]..R[base+nargs]) → R[base] */
        state_->emitter.emit_abc(OP_CALL, base, nargs, 1, previous_.line);

        /* Restore registers: call result is in base */
        state_->next_reg = base + 1;
        if (state_->next_reg > state_->max_reg)
            state_->max_reg = state_->next_reg;

        int result = base;
        if (dest >= 0 && dest != result)
        {
            emit_move(dest, result);
            free_reg(result);
            return dest;
        }
        return result;
    }

    int Compiler::generic_call_expr(int callee, int dest)
    {
        /* OP_CALL overwrites R[base] with the return value, just like a
        ** normal call. */
        int base = callee;
        int saved_next = state_->next_reg;
        if (base < saved_next)
        {
            base = alloc_reg();
            emit_move(base, callee);
        }

        int nargs = generic_argument_list(base);
        state_->emitter.emit_abc(OP_CALL, base, nargs, 1, previous_.line);

        state_->next_reg = base + 1;
        if (state_->next_reg > state_->max_reg)
            state_->max_reg = state_->next_reg;

        if (dest >= 0 && dest != base)
        {
            emit_move(dest, base);
            free_reg(base);
            return dest;
        }
        return base;
    }

    int Compiler::argument_list(int base, int initial_nargs)
    {
        int nargs = initial_nargs;
        bool has_spread = false;
        if (!check(TOK_RPAREN))
        {
            do
            {
                int arg_reg = base + 1 + nargs;
                if (arg_reg >= kMaxRegisters)
                {
                    error("Too many arguments.");
                    return nargs;
                }
                /* Ensure next_reg is at the right position */
                while (state_->next_reg <= arg_reg)
                    alloc_reg();

                if (match(TOK_STAR))
                {
                    /* *expr — spread: must be last arg */
                    has_spread = true;
                    expression(arg_reg);
                    nargs++;
                    break; /* no more args after spread */
                }

                expression(arg_reg);
                nargs++;
            } while (match(TOK_COMMA));
        }
        /* Encode spread flag in bit 7 of nargs */
        if (has_spread)
            nargs |= 0x80;
        return nargs;
    }

    /* Parse <T, U>(args) after a callee.  Generic values are normal runtime
    ** values (usually classes), placed before the explicit arguments. */
    int Compiler::generic_argument_list(int base)
    {
        consume(TOK_LT, "Expected '<' before generic arguments.");

        int nargs = 0;
        do
        {
            consume(TOK_IDENTIFIER, "Expected generic type name.");
            Token type_name = previous_;

            int arg_reg = base + 1 + nargs;
            if (arg_reg >= kMaxRegisters)
            {
                error("Too many generic arguments.");
                return nargs;
            }
            while (state_->next_reg <= arg_reg)
                alloc_reg();

            int type_reg = named_variable(type_name, arg_reg, false);
            if (type_reg != arg_reg)
                emit_move(arg_reg, type_reg);
            nargs++;
        } while (match(TOK_COMMA));

        consume(TOK_GT, "Expected '>' after generic arguments.");
        consume(TOK_LPAREN, "Expected '(' after generic arguments.");

        int total_nargs = argument_list(base, nargs);
        consume(TOK_RPAREN, "Expected ')' after arguments.");
        return total_nargs;
    }

    bool Compiler::generic_call_ahead()
    {
        if (!check(TOK_LT))
            return false;

        LexerState saved = lexer_.save_state();
        Token token = lexer_.next_token();
        if (token.type != TOK_IDENTIFIER)
        {
            lexer_.restore_state(saved);
            return false;
        }

        for (;;)
        {
            token = lexer_.next_token();
            if (token.type != TOK_COMMA)
                break;
            token = lexer_.next_token();
            if (token.type != TOK_IDENTIFIER)
            {
                lexer_.restore_state(saved);
                return false;
            }
        }

        bool is_generic_call = token.type == TOK_GT &&
                               lexer_.next_token().type == TOK_LPAREN;
        lexer_.restore_state(saved);
        return is_generic_call;
    }

    /* =========================================================
    ** Dot access: obj.field or obj.method(args)
    ** ========================================================= */

    int Compiler::dot_expr(int obj, int dest, bool can_assign)
    {
        consume(TOK_IDENTIFIER, "Expected field name after '.'.");
        Token field = previous_;

        int reg = (dest >= 0) ? dest : alloc_reg();

        /* --- Fast path: self.field inside a method → OP_GETFIELD_IDX / OP_SETFIELD_IDX --- */
        if (is_current_class_instance(obj))
        {
            ObjString *fname = token_string(field);
            int fidx = add_class_field(fname);
            if (fidx >= 0 && fidx <= 255)
            {
                /* Assignment: self.field = expr */
                if (can_assign && match(TOK_EQ))
                {
                    int val = expression(-1);
                    state_->emitter.emit_abc(OP_SETFIELD_IDX, obj, fidx, val, field.line);
                    if (val != reg)
                        free_reg(val);
                    if (obj != reg)
                        free_reg(obj);
                    return reg;
                }
                /* Augmented assign: self.field += expr  */
                if (can_assign && (check(TOK_PLUS_EQ) || check(TOK_MINUS_EQ) ||
                                   check(TOK_STAR_EQ) || check(TOK_SLASH_EQ) || check(TOK_PERCENT_EQ)))
                {
                    Token op = current_;
                    advance();
                    OpCode arith;
                    switch (op.type)
                    {
                    case TOK_PLUS_EQ:
                        arith = OP_ADD;
                        break;
                    case TOK_MINUS_EQ:
                        arith = OP_SUB;
                        break;
                    case TOK_STAR_EQ:
                        arith = OP_MUL;
                        break;
                    case TOK_SLASH_EQ:
                        arith = OP_DIV;
                        break;
                    default:
                        arith = OP_MOD;
                        break;
                    }
                    int tmp = alloc_reg();
                    state_->emitter.emit_abc(OP_GETFIELD_IDX, tmp, obj, fidx, field.line);
                    int rhs = expression(-1);
                    state_->emitter.emit_abc(arith, tmp, tmp, rhs, op.line);
                    state_->emitter.emit_abc(OP_SETFIELD_IDX, obj, fidx, tmp, op.line);
                    free_reg(rhs);
                    free_reg(tmp);
                    if (obj != reg)
                        free_reg(obj);
                    return reg;
                }
                /* Method call: self.method(args) — fall through to normal path */
                if (!check(TOK_LPAREN) && !generic_call_ahead())
                {
                    /* Read field */
                    state_->emitter.emit_abc(OP_GETFIELD_IDX, reg, obj, fidx, field.line);
                    if (obj != reg)
                        free_reg(obj);
                    return reg;
                }
            }
        }

        /* Assignment: obj.field = expr */
        if (can_assign && match(TOK_EQ))
        {
            int val = expression(-1);
            int name_ki = state_->emitter.add_string_constant(field.start, field.length);
            state_->emitter.emit_abc(OP_SETFIELD, obj, name_ki, val, previous_.line);
            free_reg(val);
            if (obj != reg)
                free_reg(obj);
            return reg;
        }

        /* Method call: obj.method(args) */
        if (check(TOK_LPAREN) || generic_call_ahead())
        {
            /* We need a contiguous [receiver, arg1, arg2, ...] block.
            ** The return value lands in R[base], so if obj is a local we
            ** must copy it to a fresh register to avoid clobbering the local. */
            int base = alloc_reg();
            if (base != obj)
                emit_move(base, obj);
            int nargs;
            if (check(TOK_LPAREN))
            {
                advance(); /* consume '(' */
                nargs = argument_list(base);
                consume(TOK_RPAREN, "Expected ')' after arguments.");
            }
            else
            {
                nargs = generic_argument_list(base);
            }

            /* 2-word instruction: OP_INVOKE + name constant */
            int sel = vm_->intern_selector(field.start, field.length);
            int name_ki = state_->emitter.add_string_constant(field.start, field.length);
            state_->emitter.emit_abc(OP_INVOKE, base, nargs, 1, field.line);
            state_->emitter.emit((uint32_t)((sel << 16) | (name_ki & 0xFFFF)), field.line);

            state_->next_reg = base + 1;
            if (dest >= 0 && dest != base)
            {
                emit_move(dest, base);
                free_reg(base);
                return dest;
            }
            return base;
        }

        /* Field read */
        int name_ki = state_->emitter.add_string_constant(field.start, field.length);
        state_->emitter.emit_abc(OP_GETFIELD, reg, obj, name_ki, field.line);
        if (obj != reg)
            free_reg(obj);
        return reg;
    }

    /* =========================================================
    ** Optional chaining: obj?.field
    **
    ** If obj is nil, result is nil (no error).
    ** Read-only — cannot assign through ?.
    ** ========================================================= */

    int Compiler::safe_dot_expr(int obj, int dest)
    {
        int line = previous_.line;
        int reg = dest >= 0 ? dest : alloc_reg();
        if (obj != reg)
            emit_move(reg, obj);

        /* If reg is nil (falsy), skip field access — result stays nil */
        int jump = state_->emitter.emit_jump(OP_JMPIFNOT, reg, line);

        /* Parse field name */
        consume(TOK_IDENTIFIER, "Expected field name after '?.'.");
        Token field = previous_;
        int name_ki = state_->emitter.add_string_constant(field.start, field.length);

        state_->emitter.emit_abc(OP_GETFIELD, reg, reg, name_ki, line);
        state_->emitter.patch_jump(jump);

        if (obj != reg)
            free_reg(obj);
        return reg;
    }

    /* =========================================================
    ** Null coalescing: left ?? right
    **
    ** If left is truthy, result is left. Otherwise, result is right.
    ** ========================================================= */

    int Compiler::null_coalesce(int left, int dest)
    {
        int line = previous_.line;
        int reg = dest >= 0 ? dest : alloc_reg();
        if (left != reg)
            emit_move(reg, left);

        /* If reg is truthy (not nil/false), skip right side */
        int jump = state_->emitter.emit_jump(OP_JMPIF, reg, line);

        /* Parse right operand */
        int right = parse_precedence(PREC_OR + 1, reg);
        if (right != reg)
        {
            emit_move(reg, right);
            free_reg(right);
        }

        state_->emitter.patch_jump(jump);

        if (left != reg)
            free_reg(left);
        return reg;
    }

    /* =========================================================
    ** Subscript: obj[index] or obj[index] = val
    ** ========================================================= */

    int Compiler::subscript_expr(int obj, int dest, bool can_assign)
    {
        /* Slice: a[start:stop:step] — starts with ':' (no start) */
        if (check(TOK_COLON))
            return slice_expr(obj, dest, -1);

        int index = expression(-1);

        /* Slice after parsing start expr: a[start:...] */
        if (check(TOK_COLON))
            return slice_expr(obj, dest, index);

        consume(TOK_RBRACKET, "Expected ']' after subscript.");

        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Assignment: obj[idx] = expr */
        if (can_assign && match(TOK_EQ))
        {
            /* Preserve container/index across RHS evaluation.
            ** RHS parsing can allocate/free temporaries and clobber these regs. */
            int obj_hold = alloc_reg();
            int idx_hold = alloc_reg();
            emit_move(obj_hold, obj);
            emit_move(idx_hold, index);

            int val = expression(-1);
            state_->emitter.emit_abc(OP_SETINDEX, obj_hold, idx_hold, val, previous_.line);
            free_reg(val);
            free_reg(idx_hold);
            free_reg(obj_hold);
            free_reg(index);
            if (obj != reg)
                free_reg(obj);
            return reg;
        }

        /* Augmented assignment: obj[idx] += expr etc. */
        if (can_assign && (check(TOK_PLUS_EQ) || check(TOK_MINUS_EQ) ||
                           check(TOK_STAR_EQ) || check(TOK_SLASH_EQ) ||
                           check(TOK_PERCENT_EQ) || check(TOK_DSLASH_EQ) ||
                           check(TOK_DSTAR_EQ)))
        {
            Token op = current_;
            advance();
            /* Read current value */
            state_->emitter.emit_abc(OP_GETINDEX, reg, obj, index, previous_.line);
            int rhs = expression(-1);
            OpCode arith = OP_ADD;
            switch (op.type)
            {
            case TOK_PLUS_EQ:
                arith = OP_ADD;
                break;
            case TOK_MINUS_EQ:
                arith = OP_SUB;
                break;
            case TOK_STAR_EQ:
                arith = OP_MUL;
                break;
            case TOK_SLASH_EQ:
                arith = OP_DIV;
                break;
            case TOK_PERCENT_EQ:
                arith = OP_MOD;
                break;
            case TOK_DSLASH_EQ:
                arith = OP_IDIV;
                break;
            case TOK_DSTAR_EQ:
                arith = OP_POW;
                break;
            default:
                break;
            }
            state_->emitter.emit_abc(arith, reg, reg, rhs, previous_.line);
            free_reg(rhs);
            state_->emitter.emit_abc(OP_SETINDEX, obj, index, reg, previous_.line);
            free_reg(index);
            if (obj != reg)
                free_reg(obj);
            return reg;
        }

        /* Read */
        state_->emitter.emit_abc(OP_GETINDEX, reg, obj, index, previous_.line);
        free_reg(index);
        if (obj != reg)
            free_reg(obj);
        return reg;
    }

    /* =========================================================
    ** Slice: container[start:stop:step]
    ** start_reg >= 0  → already evaluated start
    ** start_reg == -1 → start omitted (None)
    ** R[base]=start, R[base+1]=stop, R[base+2]=step
    ** ========================================================= */

    int Compiler::slice_expr(int obj, int dest, int start_reg)
    {
        int line = previous_.line;
        int base = alloc_reg(); /* start */

        if (start_reg >= 0)
        {
            if (start_reg != base)
                emit_move(base, start_reg);
            free_reg(start_reg);
        }
        else
        {
            state_->emitter.emit_abc(OP_LOADNIL, base, 0, 0, line);
        }

        advance(); /* consume ':' */

        int stop_reg = alloc_reg(); /* must be base+1 */

        /* stop */
        if (check(TOK_RBRACKET) || check(TOK_COLON))
        {
            state_->emitter.emit_abc(OP_LOADNIL, stop_reg, 0, 0, line);
        }
        else
        {
            int v = expression(stop_reg);
            if (v != stop_reg)
                emit_move(stop_reg, v);
        }

        /* step */
        int step_reg = alloc_reg(); /* must be base+2 */
        if (match(TOK_COLON))
        {
            if (check(TOK_RBRACKET))
                state_->emitter.emit_abc(OP_LOADNIL, step_reg, 0, 0, line);
            else
            {
                int v = expression(step_reg);
                if (v != step_reg)
                    emit_move(step_reg, v);
            }
        }
        else
        {
            state_->emitter.emit_abc(OP_LOADNIL, step_reg, 0, 0, line);
        }

        consume(TOK_RBRACKET, "Expected ']' after slice.");

        int reg = (dest >= 0) ? dest : alloc_reg();
        state_->emitter.emit_abc(OP_GETSLICE, reg, obj, base, line);

        free_reg(step_reg);
        free_reg(stop_reg);
        free_reg(base);
        if (obj != reg)
            free_reg(obj);
        return reg;
    }

    /* =========================================================
    ** Array literal: [a, b, c]  or  [expr for var in iterable [if cond]]
    ** ========================================================= */

    int Compiler::array_literal(int dest)
    {
        int reg;
        if (dest >= 0)
        {
            reg = dest;
            /* Ensure next_reg is past dest so element expressions don't reuse it */
            if (state_->next_reg <= dest)
                state_->next_reg = dest + 1;
        }
        else
        {
            reg = alloc_reg();
        }
        int line = previous_.line;
        state_->emitter.emit_abc(OP_NEWARRAY, reg, 0, 0, line);

        if (!check(TOK_RBRACKET))
        {
            while (match(TOK_NEWLINE))
            {
            }
            if (!check(TOK_RBRACKET))
            {
                /* Save state before parsing first expression — may be a comprehension */
                LexerState lex_save = lexer_.save_state();
                Token cur_save = current_;
                Token prev_save = previous_;
                int off_save = state_->emitter.current_offset();
                int reg_save = state_->next_reg;
                int glob_save = vm_->num_globals();

                int first = expression(-1);

                /* List comprehension: [expr for var in iterable [if cond]] */
                if (check(TOK_FOR))
                {
                    /* Rollback emitted code AND any speculatively-added global slots */
                    state_->emitter.shrink_to(off_save);
                    state_->next_reg = reg_save;
                    vm_->shrink_globals(glob_save);

                    advance(); /* consume 'for' */
                    begin_scope();

                    consume(TOK_IDENTIFIER, "Expected variable name after 'for'.");
                    Token var_name = previous_;
                    consume(TOK_IN, "Expected 'in' after variable name.");

                    int iter_reg = alloc_reg();
                    /* Parse iter at PREC_OR so the comprehension 'if' filter token
                       is not treated as a ternary operator. */
                    int iter_result = parse_precedence(PREC_OR, iter_reg);
                    if (iter_result != iter_reg)
                        emit_move(iter_reg, iter_result);

                    int idx_reg = alloc_reg();
                    state_->emitter.emit_asbx(OP_LOADI, idx_reg, 0, line);

                    int len_reg = alloc_reg();
                    state_->emitter.emit_abc(OP_LEN, len_reg, iter_reg, 0, line);

                    int var_reg = add_local(var_name);

                    int loop_start = state_->emitter.current_offset();

                    int cmp_reg = alloc_reg();
                    state_->emitter.emit_abc(OP_LT, cmp_reg, idx_reg, len_reg, line);
                    int exit_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cmp_reg, line);
                    free_reg(cmp_reg);

                    state_->emitter.emit_abc(OP_GETINDEX, var_reg, iter_reg, idx_reg, line);

                    /* Optional `if cond` filter */
                    int filter_jump = -1;
                    if (match(TOK_IF))
                    {
                        int cond = expression(-1);
                        filter_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond, line);
                        free_reg(cond);
                    }

                    /* Save lex state AFTER filter — this is where we continue after the body */
                    LexerState lex_after_filter = lexer_.save_state();
                    Token cur_after = current_;
                    Token prev_after = previous_;

                    /* Restore lex to before the body expression and re-parse it inside the loop */
                    lexer_.restore_state(lex_save);
                    current_ = cur_save;
                    previous_ = prev_save;
                    (void)first; /* was emitted speculatively — discarded */

                    int body = expression(-1);
                    state_->emitter.emit_abc(OP_APPEND, reg, body, 0, line);
                    free_reg(body);

                    /* Restore lex to after the filter (skip re-scanning for/in/iter/if) */
                    lexer_.restore_state(lex_after_filter);
                    current_ = cur_after;
                    previous_ = prev_after;

                    if (filter_jump >= 0)
                        state_->emitter.patch_jump(filter_jump);

                    state_->emitter.emit_abc(OP_ADDI, idx_reg, idx_reg, 1, line);
                    state_->emitter.emit_loop(loop_start, 0, line);
                    state_->emitter.patch_jump(exit_jump);

                    end_scope();
                    /* Restore next_reg to just above the result array reg */
                    state_->next_reg = reg_save;
                    while (match(TOK_NEWLINE))
                    {
                    }
                    consume(TOK_RBRACKET, "Expected ']' after list comprehension.");
                    return reg;
                }

                /* Regular array literal */
                state_->emitter.emit_abc(OP_APPEND, reg, first, 0, line);
                free_reg(first);

                while (match(TOK_COMMA))
                {
                    while (match(TOK_NEWLINE))
                    {
                    }
                    if (check(TOK_RBRACKET))
                        break;
                    int elem = expression(-1);
                    state_->emitter.emit_abc(OP_APPEND, reg, elem, 0, previous_.line);
                    free_reg(elem);
                }
            }
        }
        while (match(TOK_NEWLINE))
        {
        }
        consume(TOK_RBRACKET, "Expected ']' after array elements.");
        return reg;
    }

    /* =========================================================
    ** Map/dict literal: {k: v, k: v}
    ** Set literal:      {a, b, c}
    ** Dict/set comprehension: {k: v for x in iter} / {expr for x in iter}
    ** ========================================================= */

    int Compiler::map_literal(int dest)
    {
        int reg;
        if (dest >= 0)
        {
            reg = dest;
            if (state_->next_reg <= dest)
                state_->next_reg = dest + 1;
        }
        else
            reg = alloc_reg();
        int line = previous_.line;

        /* Empty: {} → empty map */
        if (check(TOK_RBRACE))
        {
            advance();
            state_->emitter.emit_abc(OP_NEWMAP, reg, 0, 0, line);
            return reg;
        }

        while (match(TOK_NEWLINE))
        {
        }

        /* Save state before first expression to detect set vs map vs comprehension */
        LexerState lex_save = lexer_.save_state();
        Token cur_save = current_;
        Token prev_save = previous_;
        int off_save = state_->emitter.current_offset();
        int reg_save = state_->next_reg;
        int glob_save = vm_->num_globals();

        int first_key = expression(-1);

        /* Dict comprehension: {key: val for var in iter} */
        if (check(TOK_COLON))
        {
            advance();
            state_->emitter.emit_abc(OP_NEWMAP, reg, 0, 0, line);

            int first_val = expression(-1);

            if (check(TOK_FOR))
            {
                /* Dict comprehension */
                state_->emitter.shrink_to(off_save);
                state_->next_reg = reg_save;
                vm_->shrink_globals(glob_save);

                /* NEWMAP was rolled back — re-emit it */
                state_->emitter.emit_abc(OP_NEWMAP, reg, 0, 0, line);

                advance(); /* consume 'for' */
                begin_scope();
                consume(TOK_IDENTIFIER, "Expected variable name after 'for'.");
                Token var_name = previous_;
                consume(TOK_IN, "Expected 'in' after variable name.");

                int iter_reg = alloc_reg();
                int ir = parse_precedence(PREC_OR, iter_reg);
                if (ir != iter_reg)
                    emit_move(iter_reg, ir);
                int idx_reg = alloc_reg();
                state_->emitter.emit_asbx(OP_LOADI, idx_reg, 0, line);
                int len_reg = alloc_reg();
                state_->emitter.emit_abc(OP_LEN, len_reg, iter_reg, 0, line);
                int var_reg = add_local(var_name);

                int loop_start = state_->emitter.current_offset();
                int cmp_reg = alloc_reg();
                state_->emitter.emit_abc(OP_LT, cmp_reg, idx_reg, len_reg, line);
                int exit_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cmp_reg, line);
                free_reg(cmp_reg);
                state_->emitter.emit_abc(OP_GETINDEX, var_reg, iter_reg, idx_reg, line);

                int filter_jump = -1;
                if (match(TOK_IF))
                {
                    int cond = expression(-1);
                    filter_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond, line);
                    free_reg(cond);
                }

                LexerState lex_after = lexer_.save_state();
                Token cur_after = current_;
                Token prev_after = previous_;

                lexer_.restore_state(lex_save);
                current_ = cur_save;
                previous_ = prev_save;
                (void)first_key;
                (void)first_val;

                int k = expression(-1);
                consume(TOK_COLON, "Expected ':' in dict comprehension.");
                int v = expression(-1);
                state_->emitter.emit_abc(OP_SETINDEX, reg, k, v, line);
                free_reg(v);
                free_reg(k);

                lexer_.restore_state(lex_after);
                current_ = cur_after;
                previous_ = prev_after;

                if (filter_jump >= 0)
                    state_->emitter.patch_jump(filter_jump);
                state_->emitter.emit_abc(OP_ADDI, idx_reg, idx_reg, 1, line);
                state_->emitter.emit_loop(loop_start, 0, line);
                state_->emitter.patch_jump(exit_jump);
                end_scope();
                while (match(TOK_NEWLINE))
                {
                }
                consume(TOK_RBRACE, "Expected '}' after dict comprehension.");
                return reg;
            }

            /* Regular dict */
            state_->emitter.emit_abc(OP_SETINDEX, reg, first_key, first_val, line);
            free_reg(first_val);
            free_reg(first_key);
            while (match(TOK_COMMA))
            {
                while (match(TOK_NEWLINE))
                {
                }
                if (check(TOK_RBRACE))
                    break;
                int k = expression(-1);
                consume(TOK_COLON, "Expected ':' after map key.");
                int v = expression(-1);
                state_->emitter.emit_abc(OP_SETINDEX, reg, k, v, previous_.line);
                free_reg(v);
                free_reg(k);
            }
            while (match(TOK_NEWLINE))
            {
            }
            consume(TOK_RBRACE, "Expected '}' after map entries.");
            return reg;
        }

        /* Set literal or set comprehension: first expr not followed by ':' */
        state_->emitter.shrink_to(off_save);
        state_->next_reg = reg_save;
        vm_->shrink_globals(glob_save);
        state_->emitter.emit_abc(OP_NEWSET, reg, 0, 0, line);
        (void)first_key;

        /* Check for set comprehension: {expr for var in iter} */
        /* Re-parse first expression inside the set logic */
        LexerState lex_before_first = lex_save;
        Token cur_before = cur_save;
        Token prev_before = prev_save;

        lexer_.restore_state(lex_before_first);
        current_ = cur_before;
        previous_ = prev_before;

        int elem0 = expression(-1);
        if (check(TOK_FOR))
        {
            /* Set comprehension */
            state_->emitter.shrink_to(off_save + 1); /* keep NEWSET */
            state_->next_reg = reg_save;
            (void)elem0;

            advance(); /* consume 'for' */
            begin_scope();
            consume(TOK_IDENTIFIER, "Expected variable name after 'for'.");
            Token var_name = previous_;
            consume(TOK_IN, "Expected 'in' after variable name.");

            int iter_reg = alloc_reg();
            int ir = parse_precedence(PREC_OR, iter_reg);
            if (ir != iter_reg)
                emit_move(iter_reg, ir);
            int idx_reg = alloc_reg();
            state_->emitter.emit_asbx(OP_LOADI, idx_reg, 0, line);
            int len_reg = alloc_reg();
            state_->emitter.emit_abc(OP_LEN, len_reg, iter_reg, 0, line);
            int var_reg = add_local(var_name);

            int loop_start = state_->emitter.current_offset();
            int cmp_reg = alloc_reg();
            state_->emitter.emit_abc(OP_LT, cmp_reg, idx_reg, len_reg, line);
            int exit_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cmp_reg, line);
            free_reg(cmp_reg);
            state_->emitter.emit_abc(OP_GETINDEX, var_reg, iter_reg, idx_reg, line);

            int filter_jump = -1;
            if (match(TOK_IF))
            {
                int cond = expression(-1);
                filter_jump = state_->emitter.emit_jump(OP_JMPIFNOT, cond, line);
                free_reg(cond);
            }

            LexerState lex_after = lexer_.save_state();
            Token cur_after = current_;
            Token prev_after = previous_;

            lexer_.restore_state(lex_before_first);
            current_ = cur_before;
            previous_ = prev_before;

            int body = expression(-1);
            state_->emitter.emit_abc(OP_SETADD, reg, body, 0, line);
            free_reg(body);

            lexer_.restore_state(lex_after);
            current_ = cur_after;
            previous_ = prev_after;

            if (filter_jump >= 0)
                state_->emitter.patch_jump(filter_jump);
            state_->emitter.emit_abc(OP_ADDI, idx_reg, idx_reg, 1, line);
            state_->emitter.emit_loop(loop_start, 0, line);
            state_->emitter.patch_jump(exit_jump);
            end_scope();
            while (match(TOK_NEWLINE))
            {
            }
            consume(TOK_RBRACE, "Expected '}' after set comprehension.");
            return reg;
        }

        /* Regular set */
        state_->emitter.emit_abc(OP_SETADD, reg, elem0, 0, line);
        free_reg(elem0);
        while (match(TOK_COMMA))
        {
            while (match(TOK_NEWLINE))
            {
            }
            if (check(TOK_RBRACE))
                break;
            int e = expression(-1);
            state_->emitter.emit_abc(OP_SETADD, reg, e, 0, previous_.line);
            free_reg(e);
        }
        while (match(TOK_NEWLINE))
        {
        }
        consume(TOK_RBRACE, "Expected '}' after set literal.");
        return reg;
    }

    /* =========================================================
    ** Lambda: lambda params: expr
    ** ========================================================= */

    /* =========================================================
    ** yield expression: yield expr
    ** ========================================================= */

    int Compiler::yield_expr(int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Mark this function as a generator */
        state_->is_generator = true;

        /* Parse the value to yield */
        int val = expression(reg);
        if (val != reg)
            emit_move(reg, val);

        /* Emit OP_YIELD: A=dest for resumed value, B=value to yield */
        state_->emitter.emit_abc(OP_YIELD, reg, reg, 0, previous_.line);

        return reg;
    }

    int Compiler::await_expr(int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

        /* Parse the fiber/value to await */
        int val = expression(reg);
        if (val != reg)
            emit_move(reg, val);

        /* Emit OP_AWAIT: A=dest, B=fiber */
        state_->emitter.emit_abc(OP_AWAIT, reg, reg, 0, previous_.line);

        return reg;
    }

    int Compiler::lambda_expr(int dest)
    {
        int reg = (dest >= 0) ? dest : alloc_reg();

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
        fn_state.is_generator = false;
        fn_state.global_count = 0;

        fn_state.emitter.begin("<lambda>", 0, current_file_);

        CompilerState *enclosing = state_;
        state_ = &fn_state;

        begin_scope();

        /* Parameters */
        int arity = 0;
        if (!check(TOK_COLON))
        {
            do
            {
                consume(TOK_IDENTIFIER, "Expected parameter name.");
                add_local(previous_);
                arity++;
            } while (match(TOK_COMMA));
        }
        consume(TOK_COLON, "Expected ':' after lambda parameters.");

        /* Body: single expression */
        int result = expression(0);
        if (result != 0)
            emit_move(0, result);
        state_->emitter.emit_abc(OP_RETURN, 0, 1, 0, previous_.line);

        ObjFunc *fn = state_->emitter.end(state_->max_reg);
        fn->arity = arity;

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

        state_ = enclosing;

        int ki = state_->emitter.add_constant(val_obj((Obj *)fn));
        state_->emitter.emit_abx(OP_CLOSURE, reg, ki, previous_.line);
        return reg;
    }

    /* =========================================================
    ** super().method(args) — parent class method invocation
    **
    ** Emits OP_SUPER_INVOKE (3-word instruction):
    **   word1: [OP_SUPER_INVOKE | base | argc | 0]
    **   word2: (selector_slot << 16) | name_ki
    **   word3: parent_class_constant_index
    ** ========================================================= */

    int Compiler::super_expr(int dest)
    {
        if (!in_class_)
        {
            error("Cannot use 'super' outside of a class.");
            return dest >= 0 ? dest : alloc_reg();
        }
        if (!class_has_parent_)
        {
            error("Cannot use 'super' in a class with no parent.");
            return dest >= 0 ? dest : alloc_reg();
        }

        /* Consume super() — the parens are mandatory but empty */
        consume(TOK_LPAREN, "Expected '(' after 'super'.");
        consume(TOK_RPAREN, "Expected ')' after 'super('.");

        /* Must be followed by .method(...) */
        consume(TOK_DOT, "Expected '.method(...)' after 'super()'.");
        consume(TOK_IDENTIFIER, "Expected method name after 'super().'.");
        Token method_name = previous_;

        consume(TOK_LPAREN, "Expected '(' after method name.");

        /* self is the receiver — copy to temp base reg */
        int base = alloc_reg();
        emit_move(base, 0); /* reg 0 = self */

        /* Arguments */
        int nargs = argument_list(base);
        consume(TOK_RPAREN, "Expected ')' after arguments.");

        /* Resolve parent class as a global → store as constant */
        ObjString *parent_str = token_string(current_class_parent_);
        state_->emitter.add_constant(val_obj((Obj *)parent_str));
        /* At runtime we'll need the actual class object, so store it as a
        ** global-load constant. Actually, the VM handler reads parent from
        ** frame->func->constants[parent_ki]. We need to store the class VALUE
        ** there. Since the class is a global, we emit a special constant that
        ** will be resolved at runtime. Let's store the name and do a global
        ** lookup at runtime. But the existing handler does as_class(K[parent_ki]).
        ** We need to change the approach: store the global index instead. */

        /* Simpler: emit OP_GETGLOBAL into a temp, store that temp's value...
        ** Actually the cleanest: use the parent global index and have the VM
        ** read it from globals at runtime. Let's patch the OP_SUPER_INVOKE
        ** to use global index in word3 instead of constant index. */

        int parent_gidx = find_or_add_global(
            current_class_parent_.start, current_class_parent_.length);

        int sel = vm_->intern_selector(method_name.start, method_name.length);
        int name_ki = state_->emitter.add_string_constant(
            method_name.start, method_name.length);

        state_->emitter.emit_abc(OP_SUPER_INVOKE, base, nargs, 0, method_name.line);
        state_->emitter.emit((uint32_t)((sel << 16) | (name_ki & 0xFFFF)), method_name.line);
        state_->emitter.emit((uint32_t)parent_gidx, method_name.line);

        state_->next_reg = base + 1;
        if (dest >= 0 && dest != base)
        {
            emit_move(dest, base);
            free_reg(base);
            return dest;
        }
        return base;
    }

} /* namespace zen */
