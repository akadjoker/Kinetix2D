#include "emitter.h"
#include "memory.h"

namespace zen
{

    Emitter::Emitter(GC *gc) : gc_(gc), func_(nullptr), last_line_(0) {}

    void Emitter::begin(const char *name, int arity, const char *source)
    {
        func_ = new_func(gc_);
        func_->arity = arity;
        func_->name = name ? intern_string(gc_, name, (int)strlen(name)) : nullptr;
        func_->source = source ? intern_string(gc_, source, (int)strlen(source)) : nullptr;
    }

    /* --- Grow buffers --- */

    void Emitter::grow_code()
    {
        int old_cap = func_->code_capacity;
        int new_cap = old_cap < 8 ? 8 : old_cap * 2;
        func_->code = (Instruction *)zen_realloc(gc_,
                                                 func_->code, old_cap * sizeof(Instruction), new_cap * sizeof(Instruction));
        func_->lines = (int32_t *)zen_realloc(gc_,
                                              func_->lines, old_cap * sizeof(int32_t), new_cap * sizeof(int32_t));
        func_->code_capacity = new_cap;
    }

    void Emitter::grow_constants()
    {
        int old_cap = func_->const_capacity;
        int new_cap = old_cap < 8 ? 8 : old_cap * 2;
        func_->constants = (Value *)zen_realloc(gc_,
                                                func_->constants, old_cap * sizeof(Value), new_cap * sizeof(Value));
        func_->const_capacity = new_cap;
    }

    /* --- Emit --- */

    int Emitter::emit(Instruction instr, int line)
    {
        if (func_->code_count >= func_->code_capacity)
        {
            grow_code();
        }
        int offset = func_->code_count;
        func_->code[offset] = instr;
        func_->lines[offset] = line;
        func_->code_count++;
        last_line_ = line;
        return offset;
    }

    int Emitter::emit_abc(OpCode op, int a, int b, int c, int line)
    {
        return emit(ZEN_ENCODE(op, a, b, c), line);
    }

    int Emitter::emit_abx(OpCode op, int a, int bx, int line)
    {
        return emit(ZEN_ENCODE_BX(op, a, bx), line);
    }

    int Emitter::emit_asbx(OpCode op, int a, int sbx, int line)
    {
        return emit(ZEN_ENCODE_SBX(op, a, sbx), line);
    }

    /* --- Constants --- */

    int Emitter::add_constant(Value val)
    {
        /* Deduplica ints/floats simples (strings já são internadas).
        ** O tipo tem de bater certo: values_equal() é a semântica do operador
        ** '==', que compara int e float numericamente (3 == 3.0), e reusar o
        ** slot de um 3.0 para um 3 devolvia um float onde o código escreveu
        ** um int. Identidade de constante é mais estrita do que igualdade. */
        for (int i = 0; i < func_->const_count; i++)
        {
            if (func_->constants[i].type == val.type && values_equal(func_->constants[i], val))
                return i;
        }
        if (func_->const_count >= func_->const_capacity)
        {
            grow_constants();
        }
        int idx = func_->const_count;
        func_->constants[idx] = val;
        func_->const_count++;
        return idx;
    }

    int Emitter::add_string_constant(const char *str, int len)
    {
        if (len < 0)
            len = (int)strlen(str);
        ObjString *s = intern_string(gc_, str, len);
        return add_constant(val_obj((Obj *)s));
    }

    /* --- Escape-processing string constant ---
    ** Converts \n, \t, \r, \\, \", \', \0, \a, \b, \f, \v, \e,
    ** \xHH, \uHHHH, \UHHHHHHHH to actual bytes (UTF-8 encoded).
    */

    static int hex_digit(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    }

    static int encode_utf8(uint32_t cp, char *out)
    {
        if (cp <= 0x7F)
        {
            out[0] = (char)cp;
            return 1;
        }
        if (cp <= 0x7FF)
        {
            out[0] = (char)(0xC0 | (cp >> 6));
            out[1] = (char)(0x80 | (cp & 0x3F));
            return 2;
        }
        if (cp <= 0xFFFF)
        {
            out[0] = (char)(0xE0 | (cp >> 12));
            out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[2] = (char)(0x80 | (cp & 0x3F));
            return 3;
        }
        if (cp <= 0x10FFFF)
        {
            out[0] = (char)(0xF0 | (cp >> 18));
            out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
            out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
            out[3] = (char)(0x80 | (cp & 0x3F));
            return 4;
        }
        /* Invalid codepoint — emit replacement character U+FFFD */
        out[0] = (char)0xEF;
        out[1] = (char)0xBF;
        out[2] = (char)0xBD;
        return 3;
    }

    int Emitter::add_escaped_string_constant(const char *str, int len)
    {
        /* Clear any previous escape error */
        escape_error_[0] = '\0';

        /* Worst case: each char stays as-is. UTF-8 can expand \uHHHH → 3 bytes
           but the escape itself is 6 chars, so output is always <= input. */
        char *buf = (char *)malloc(len + 1);
        int out = 0;

        for (int i = 0; i < len; i++)
        {
            if (str[i] != '\\')
            {
                buf[out++] = str[i];
                continue;
            }

            /* We have a backslash */
            i++;
            if (i >= len)
            {
                buf[out++] = '\\';
                break;
            }

            switch (str[i])
            {
            case 'n':
                buf[out++] = '\n';
                break;
            case 't':
                buf[out++] = '\t';
                break;
            case 'r':
                buf[out++] = '\r';
                break;
            case '\\':
                buf[out++] = '\\';
                break;
            case '"':
                buf[out++] = '"';
                break;
            case '\'':
                buf[out++] = '\'';
                break;
            case '0':
                buf[out++] = '\0';
                break;
            case 'a':
                buf[out++] = '\a';
                break;
            case 'b':
                buf[out++] = '\b';
                break;
            case 'f':
                buf[out++] = '\f';
                break;
            case 'v':
                buf[out++] = '\v';
                break;
            case 'e':
                buf[out++] = '\x1B';
                break;
            case 'x':
            {
                /* \xHH — exactly 2 hex digits */
                if (i + 2 < len)
                {
                    int h1 = hex_digit(str[i + 1]);
                    int h2 = hex_digit(str[i + 2]);
                    if (h1 >= 0 && h2 >= 0)
                    {
                        buf[out++] = (char)((h1 << 4) | h2);
                        i += 2;
                        break;
                    }
                }
                /* Invalid hex escape */
                snprintf(escape_error_, sizeof(escape_error_),
                         "Invalid \\x escape: expected 2 hex digits.");
                buf[out++] = '?';
                break;
            }
            case 'u':
            {
                /* \uHHHH — exactly 4 hex digits → UTF-8 */
                if (i + 4 < len)
                {
                    uint32_t cp = 0;
                    bool valid = true;
                    for (int j = 1; j <= 4; j++)
                    {
                        int d = hex_digit(str[i + j]);
                        if (d < 0)
                        {
                            valid = false;
                            break;
                        }
                        cp = (cp << 4) | (uint32_t)d;
                    }
                    if (valid)
                    {
                        /* Reject surrogate codepoints (invalid in UTF-8) */
                        if (cp >= 0xD800 && cp <= 0xDFFF)
                        {
                            out += encode_utf8(0xFFFD, buf + out);
                        }
                        else
                        {
                            out += encode_utf8(cp, buf + out);
                        }
                        i += 4;
                        break;
                    }
                }
                snprintf(escape_error_, sizeof(escape_error_),
                         "Invalid \\u escape: expected 4 hex digits.");
                buf[out++] = '?';
                break;
            }
            case 'U':
            {
                /* \UHHHHHHHH — exactly 8 hex digits → UTF-8 */
                if (i + 8 < len)
                {
                    uint32_t cp = 0;
                    bool valid = true;
                    for (int j = 1; j <= 8; j++)
                    {
                        int d = hex_digit(str[i + j]);
                        if (d < 0)
                        {
                            valid = false;
                            break;
                        }
                        cp = (cp << 4) | (uint32_t)d;
                    }
                    if (valid && cp <= 0x10FFFF)
                    {
                        out += encode_utf8(cp, buf + out);
                        i += 8;
                        break;
                    }
                }
                snprintf(escape_error_, sizeof(escape_error_),
                         "Invalid \\U escape: expected 8 hex digits.");
                buf[out++] = '?';
                break;
            }
            default:
                snprintf(escape_error_, sizeof(escape_error_),
                         "Unknown escape sequence '\\%c'.", str[i]);
                buf[out++] = '?';
                break;
            }
        }

        ObjString *s = intern_string(gc_, buf, out);
        free(buf);
        return add_constant(val_obj((Obj *)s));
    }

    /* --- Verbatim string constant ---
    ** No escape processing. Only "" → " (doubled quote = literal quote).
    */
    int Emitter::add_verbatim_string_constant(const char *str, int len)
    {
        char *buf = (char *)malloc(len + 1);
        int out = 0;

        for (int i = 0; i < len; i++)
        {
            if (str[i] == '"' && i + 1 < len && str[i + 1] == '"')
            {
                buf[out++] = '"';
                i++; /* skip second quote */
            }
            else
            {
                buf[out++] = str[i];
            }
        }

        ObjString *s = intern_string(gc_, buf, out);
        free(buf);
        return add_constant(val_obj((Obj *)s));
    }

    /* --- Jumps --- */

    int Emitter::emit_jump(OpCode op, int a, int line)
    {
        /* Emite com placeholder sBx=0, retorna offset para patch */
        return emit(ZEN_ENCODE_SBX(op, a, 0), line);
    }

    void Emitter::patch_jump(int offset)
    {
        /* Calcula distância: de (offset+1) até current */
        int jump = func_->code_count - (offset + 1);
        uint32_t instr = func_->code[offset];
        uint8_t op = ZEN_OP(instr);
        uint8_t a = ZEN_A(instr);
        func_->code[offset] = ZEN_ENCODE_SBX((OpCode)op, a, jump);
    }

    void Emitter::patch_jump_to(int offset, int target)
    {
        int jump = target - (offset + 1);
        uint32_t instr = func_->code[offset];
        uint8_t op = ZEN_OP(instr);
        uint8_t a = ZEN_A(instr);
        func_->code[offset] = ZEN_ENCODE_SBX((OpCode)op, a, jump);
    }

    int Emitter::emit_loop(int loop_start, int a, int line)
    {
        /* Jump negativo para loop_start */
        int jump = loop_start - (func_->code_count + 1);
        return emit(ZEN_ENCODE_SBX(OP_JMP, a, jump), line);
    }

    /* --- Fused compare+jump (2-word superinstructions) --- */

    int Emitter::emit_lt_jmpifnot(int b, int c, int line)
    {
        emit(ZEN_ENCODE(OP_LTJMPIFNOT, 0, b, c), line);
        /* Second word: sBx placeholder (to be patched) */
        return emit(ZEN_ENCODE_SBX(OP_JMP, 0, 0), line); /* offset of the sBx word */
    }

    int Emitter::emit_le_jmpifnot(int b, int c, int line)
    {
        emit(ZEN_ENCODE(OP_LEJMPIFNOT, 0, b, c), line);
        return emit(ZEN_ENCODE_SBX(OP_JMP, 0, 0), line);
    }

    void Emitter::patch_fused_jump(int sbx_offset)
    {
        /* The sBx word is at sbx_offset. Jump distance = from (sbx_offset+1) to current. */
        int jump = func_->code_count - (sbx_offset + 1);
        func_->code[sbx_offset] = ZEN_ENCODE_SBX(OP_JMP, 0, jump);
    }

    /* --- FOR_ITER (2-word) --- */

    int Emitter::emit_for_iter(int a, int b, int line)
    {
        emit(ZEN_ENCODE(OP_FOR_ITER, a, b, 0), line);
        int word2_offset = func_->code_count;
        emit(0, line); /* placeholder word2: jump offset */
        return word2_offset;
    }

    void Emitter::patch_for_iter(int word2_offset)
    {
        /* Jump from (word2_offset+1) to current offset */
        int jump = func_->code_count - (word2_offset + 1);
        func_->code[word2_offset] = (uint32_t)jump;
    }

    /* --- Fused global call (2-word) --- */

    void Emitter::emit_callglobal(int a, int nargs, int nresults, int global_idx, int line)
    {
        emit(ZEN_ENCODE(OP_CALLGLOBAL, a, nargs, nresults), line);
        emit(ZEN_ENCODE_BX(OP_HALT, 0, global_idx), line); /* word 2: Bx = global index */
    }

    /* --- Finalizar --- */

    ObjFunc *Emitter::end(int num_regs)
    {
        /* Garante HALT no fim */
        emit_abc(OP_HALT, 0, 0, 0, last_line_);

        func_->num_regs = num_regs;

        /* Shrink buffers ao tamanho exacto (opcional, poupa memória) */
        if (func_->code_count < func_->code_capacity)
        {
            func_->code = (Instruction *)zen_realloc(gc_,
                                                     func_->code,
                                                     func_->code_capacity * sizeof(Instruction),
                                                     func_->code_count * sizeof(Instruction));
            func_->lines = (int32_t *)zen_realloc(gc_,
                                                  func_->lines,
                                                  func_->code_capacity * sizeof(int32_t),
                                                  func_->code_count * sizeof(int32_t));
            func_->code_capacity = func_->code_count;
        }
        if (func_->const_count < func_->const_capacity)
        {
            func_->constants = (Value *)zen_realloc(gc_,
                                                    func_->constants,
                                                    func_->const_capacity * sizeof(Value),
                                                    func_->const_count * sizeof(Value));
            func_->const_capacity = func_->const_count;
        }

        ObjFunc *result = func_;
        func_ = nullptr;
        return result;
    }

} /* namespace zen */
