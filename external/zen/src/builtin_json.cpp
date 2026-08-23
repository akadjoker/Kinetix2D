/* =========================================================
** builtin_json.cpp — "json" module for Zen
**
** Full JSON parser/serializer.
** Supports: parse(string) → value, stringify(value, indent?) → string
** No external deps — pure C implementation.
**
** Usage:
**   import json
**   var obj = json.parse('{"name":"zen","version":1}')
**   print(obj["name"])   // zen
**
**   var s = json.stringify(obj)       // compact
**   var s = json.stringify(obj, 2)    // pretty, 2 spaces
**   var s = json.stringify(obj, true) // pretty, 2 spaces default
** ========================================================= */

#include "module.h"
#include "vm.h"
#include "memory.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cerrno>

namespace zen
{

    /* =========================================================
    ** String buffer — growable char buffer (no STL)
    ** ========================================================= */

    struct StrBuf
    {
        char *data;
        int len;
        int cap;
    };

    static void sb_init(StrBuf *sb)
    {
        sb->data = (char *)malloc(256);
        sb->len = 0;
        sb->cap = 256;
    }

    static void sb_free(StrBuf *sb)
    {
        free(sb->data);
        sb->data = nullptr;
        sb->len = 0;
        sb->cap = 0;
    }

    static void sb_grow(StrBuf *sb, int need)
    {
        if (sb->len + need <= sb->cap)
            return;
        int new_cap = sb->cap * 2;
        while (new_cap < sb->len + need)
            new_cap *= 2;
        sb->data = (char *)realloc(sb->data, (size_t)new_cap);
        sb->cap = new_cap;
    }

    static void sb_putc(StrBuf *sb, char c)
    {
        sb_grow(sb, 1);
        sb->data[sb->len++] = c;
    }

    static void sb_puts(StrBuf *sb, const char *s, int n)
    {
        sb_grow(sb, n);
        memcpy(sb->data + sb->len, s, (size_t)n);
        sb->len += n;
    }

    static void sb_indent(StrBuf *sb, int depth, int width)
    {
        if (width <= 0 || depth <= 0)
            return;
        int n = depth * width;
        sb_grow(sb, n);
        memset(sb->data + sb->len, ' ', (size_t)n);
        sb->len += n;
    }

    /* =========================================================
    ** Stringify — value → JSON string
    ** ========================================================= */

    struct StringifyCtx
    {
        VM *vm;
        GC *gc;
        bool pretty;
        int indent_width;
        /* Cycle detection: simple stack of pointers */
        const void *stack[64];
        int stack_depth;
        char error[256];
    };

    static bool ctx_push(StringifyCtx *ctx, const void *ptr)
    {
        for (int i = 0; i < ctx->stack_depth; i++)
        {
            if (ctx->stack[i] == ptr)
            {
                snprintf(ctx->error, sizeof(ctx->error), "cyclic reference detected");
                return false;
            }
        }
        if (ctx->stack_depth >= 64)
        {
            snprintf(ctx->error, sizeof(ctx->error), "nesting too deep (>64)");
            return false;
        }
        ctx->stack[ctx->stack_depth++] = ptr;
        return true;
    }

    static void ctx_pop(StringifyCtx *ctx)
    {
        ctx->stack_depth--;
    }

    static void json_escape_string(StrBuf *sb, const char *s, int len)
    {
        sb_putc(sb, '"');
        for (int i = 0; i < len; i++)
        {
            unsigned char c = (unsigned char)s[i];
            switch (c)
            {
            case '"':
                sb_puts(sb, "\\\"", 2);
                break;
            case '\\':
                sb_puts(sb, "\\\\", 2);
                break;
            case '\b':
                sb_puts(sb, "\\b", 2);
                break;
            case '\f':
                sb_puts(sb, "\\f", 2);
                break;
            case '\n':
                sb_puts(sb, "\\n", 2);
                break;
            case '\r':
                sb_puts(sb, "\\r", 2);
                break;
            case '\t':
                sb_puts(sb, "\\t", 2);
                break;
            default:
                if (c < 0x20)
                {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                    sb_puts(sb, buf, 6);
                }
                else
                {
                    sb_putc(sb, (char)c);
                }
                break;
            }
        }
        sb_putc(sb, '"');
    }

    static bool json_stringify_value(StringifyCtx *ctx, StrBuf *sb, Value val, int depth);

    static bool json_stringify_array(StringifyCtx *ctx, StrBuf *sb, ObjArray *arr, int depth)
    {
        if (!ctx_push(ctx, arr))
            return false;

        int count = arr_count(arr);
        sb_putc(sb, '[');

        if (count > 0)
        {
            if (ctx->pretty)
                sb_putc(sb, '\n');

            for (int i = 0; i < count; i++)
            {
                if (ctx->pretty)
                    sb_indent(sb, depth + 1, ctx->indent_width);

                if (!json_stringify_value(ctx, sb, arr->data[i], depth + 1))
                {
                    ctx_pop(ctx);
                    return false;
                }

                if (i + 1 < count)
                    sb_putc(sb, ',');

                if (ctx->pretty)
                    sb_putc(sb, '\n');
            }

            if (ctx->pretty)
                sb_indent(sb, depth, ctx->indent_width);
        }

        sb_putc(sb, ']');
        ctx_pop(ctx);
        return true;
    }

    static bool json_stringify_map(StringifyCtx *ctx, StrBuf *sb, ObjMap *map, int depth)
    {
        if (!ctx_push(ctx, map))
            return false;

        sb_putc(sb, '{');

        if (map->count > 0)
        {
            if (ctx->pretty)
                sb_putc(sb, '\n');

            bool first = true;
            for (int32_t i = 0; i < map->capacity; i++)
            {
                if (map->nodes[i].hash == 0xFFFFFFFFu)
                    continue;

                MapNode *n = &map->nodes[i];

                if (!first)
                {
                    sb_putc(sb, ',');
                    if (ctx->pretty)
                        sb_putc(sb, '\n');
                }

                if (ctx->pretty)
                    sb_indent(sb, depth + 1, ctx->indent_width);

                /* Key must be string in JSON */
                if (is_string(n->key))
                {
                    ObjString *ks = as_string(n->key);
                    json_escape_string(sb, ks->chars, ks->length);
                }
                else if (is_int(n->key))
                {
                    char buf[32];
                    int kl = snprintf(buf, sizeof(buf), "\"%lld\"", (long long)n->key.as.integer);
                    sb_puts(sb, buf, kl);
                }
                else if (is_float(n->key))
                {
                    char buf[64];
                    int kl = snprintf(buf, sizeof(buf), "\"%.17g\"", n->key.as.number);
                    sb_puts(sb, buf, kl);
                }
                else
                {
                    sb_puts(sb, "\"?\"", 3);
                }

                sb_putc(sb, ':');
                if (ctx->pretty)
                    sb_putc(sb, ' ');

                if (!json_stringify_value(ctx, sb, n->value, depth + 1))
                {
                    ctx_pop(ctx);
                    return false;
                }

                first = false;
            }

            if (ctx->pretty)
            {
                sb_putc(sb, '\n');
                sb_indent(sb, depth, ctx->indent_width);
            }
        }

        sb_putc(sb, '}');
        ctx_pop(ctx);
        return true;
    }

    static bool json_stringify_value(StringifyCtx *ctx, StrBuf *sb, Value val, int depth)
    {
        switch (val.type)
        {
        case VAL_NIL:
            sb_puts(sb, "null", 4);
            return true;
        case VAL_BOOL:
            if (val.as.boolean)
                sb_puts(sb, "true", 4);
            else
                sb_puts(sb, "false", 5);
            return true;
        case VAL_INT:
        {
            char buf[32];
            int n = snprintf(buf, sizeof(buf), "%lld", (long long)val.as.integer);
            sb_puts(sb, buf, n);
            return true;
        }
        case VAL_FLOAT:
        {
            double d = val.as.number;
            if (!std::isfinite(d))
            {
                snprintf(ctx->error, sizeof(ctx->error), "cannot serialize NaN or Infinity");
                return false;
            }
            char buf[64];
            int n = snprintf(buf, sizeof(buf), "%.17g", d);
            sb_puts(sb, buf, n);
            return true;
        }
        case VAL_OBJ:
        {
            Obj *obj = val.as.obj;
            switch (obj->type)
            {
            case OBJ_STRING:
            {
                ObjString *s = (ObjString *)obj;
                json_escape_string(sb, s->chars, s->length);
                return true;
            }
            case OBJ_ARRAY:
                return json_stringify_array(ctx, sb, (ObjArray *)obj, depth);
            case OBJ_MAP:
                return json_stringify_map(ctx, sb, (ObjMap *)obj, depth);
            default:
                snprintf(ctx->error, sizeof(ctx->error),
                         "type not JSON serializable");
                return false;
            }
        }
        default:
            snprintf(ctx->error, sizeof(ctx->error), "type not JSON serializable");
            return false;
        }
    }

    /* =========================================================
    ** Parser — JSON string → Zen value
    ** ========================================================= */

    struct JsonParser
    {
        VM *vm;
        GC *gc;
        const char *src;
        int len;
        int pos;
        char error[256];
    };

    static void jp_skip_ws(JsonParser *p)
    {
        while (p->pos < p->len)
        {
            char c = p->src[p->pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                p->pos++;
            else
                break;
        }
    }

    static bool jp_consume(JsonParser *p, char expected)
    {
        if (p->pos < p->len && p->src[p->pos] == expected)
        {
            p->pos++;
            return true;
        }
        return false;
    }

    static void jp_error(JsonParser *p, const char *msg)
    {
        if (p->error[0] != '\0')
            return;
        int line = 1, col = 1;
        for (int i = 0; i < p->pos && i < p->len; i++)
        {
            if (p->src[i] == '\n')
            {
                line++;
                col = 1;
            }
            else
            {
                col++;
            }
        }
        snprintf(p->error, sizeof(p->error), "%s at line %d, col %d", msg, line, col);
    }

    static bool jp_parse_value(JsonParser *p, Value *out);

    /* Encode a Unicode codepoint to UTF-8 into buf, returns bytes written */
    static int utf8_encode(int cp, char *buf)
    {
        if (cp < 0x80)
        {
            buf[0] = (char)cp;
            return 1;
        }
        if (cp < 0x800)
        {
            buf[0] = (char)(0xC0 | (cp >> 6));
            buf[1] = (char)(0x80 | (cp & 0x3F));
            return 2;
        }
        if (cp < 0x10000)
        {
            buf[0] = (char)(0xE0 | (cp >> 12));
            buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
            buf[2] = (char)(0x80 | (cp & 0x3F));
            return 3;
        }
        buf[0] = (char)(0xF0 | (cp >> 18));
        buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        buf[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }

    static int jp_parse_hex4(JsonParser *p)
    {
        if (p->pos + 4 > p->len)
        {
            jp_error(p, "expected 4 hex digits");
            return -1;
        }
        int val = 0;
        for (int i = 0; i < 4; i++)
        {
            char c = p->src[p->pos++];
            int nibble;
            if (c >= '0' && c <= '9')
                nibble = c - '0';
            else if (c >= 'a' && c <= 'f')
                nibble = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                nibble = c - 'A' + 10;
            else
            {
                jp_error(p, "invalid hex digit in \\u escape");
                return -1;
            }
            val = (val << 4) | nibble;
        }
        return val;
    }

    static bool jp_parse_string(JsonParser *p, char **out, int *out_len)
    {
        if (!jp_consume(p, '"'))
        {
            jp_error(p, "expected '\"'");
            return false;
        }

        /* Temporary buffer for parsed string content */
        int cap = 64;
        int len = 0;
        char *buf = (char *)malloc((size_t)cap);

        while (p->pos < p->len)
        {
            char c = p->src[p->pos++];
            if (c == '"')
            {
                *out = buf;
                *out_len = len;
                return true;
            }

            if (c == '\\')
            {
                if (p->pos >= p->len)
                {
                    jp_error(p, "incomplete escape");
                    free(buf);
                    return false;
                }
                char esc = p->src[p->pos++];
                char decoded;
                switch (esc)
                {
                case '"':
                    decoded = '"';
                    break;
                case '\\':
                    decoded = '\\';
                    break;
                case '/':
                    decoded = '/';
                    break;
                case 'b':
                    decoded = '\b';
                    break;
                case 'f':
                    decoded = '\f';
                    break;
                case 'n':
                    decoded = '\n';
                    break;
                case 'r':
                    decoded = '\r';
                    break;
                case 't':
                    decoded = '\t';
                    break;
                case 'u':
                {
                    int hi = jp_parse_hex4(p);
                    if (hi < 0)
                    {
                        free(buf);
                        return false;
                    }
                    int codepoint = hi;
                    /* Surrogate pair */
                    if (hi >= 0xD800 && hi <= 0xDBFF)
                    {
                        if (p->pos + 2 > p->len || p->src[p->pos] != '\\' || p->src[p->pos + 1] != 'u')
                        {
                            jp_error(p, "expected low surrogate");
                            free(buf);
                            return false;
                        }
                        p->pos += 2;
                        int lo = jp_parse_hex4(p);
                        if (lo < 0)
                        {
                            free(buf);
                            return false;
                        }
                        if (lo < 0xDC00 || lo > 0xDFFF)
                        {
                            jp_error(p, "invalid low surrogate");
                            free(buf);
                            return false;
                        }
                        codepoint = 0x10000 + ((hi - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    else if (hi >= 0xDC00 && hi <= 0xDFFF)
                    {
                        jp_error(p, "unexpected low surrogate");
                        free(buf);
                        return false;
                    }
                    char utf8[4];
                    int n = utf8_encode(codepoint, utf8);
                    if (len + n > cap)
                    {
                        cap = (cap + n) * 2;
                        buf = (char *)realloc(buf, (size_t)cap);
                    }
                    memcpy(buf + len, utf8, (size_t)n);
                    len += n;
                    continue; /* skip the putc below */
                }
                default:
                    jp_error(p, "invalid escape sequence");
                    free(buf);
                    return false;
                }
                c = decoded;
            }
            else if ((unsigned char)c < 0x20)
            {
                jp_error(p, "unescaped control character");
                free(buf);
                return false;
            }

            if (len + 1 > cap)
            {
                cap *= 2;
                buf = (char *)realloc(buf, (size_t)cap);
            }
            buf[len++] = c;
        }

        jp_error(p, "unterminated string");
        free(buf);
        return false;
    }

    static bool jp_parse_number(JsonParser *p, Value *out)
    {
        int start = p->pos;

        if (p->pos < p->len && p->src[p->pos] == '-')
            p->pos++;

        if (p->pos >= p->len)
        {
            jp_error(p, "invalid number");
            return false;
        }

        if (p->src[p->pos] == '0')
        {
            p->pos++;
            if (p->pos < p->len && p->src[p->pos] >= '0' && p->src[p->pos] <= '9')
            {
                jp_error(p, "leading zeroes not allowed");
                return false;
            }
        }
        else
        {
            if (p->src[p->pos] < '0' || p->src[p->pos] > '9')
            {
                jp_error(p, "invalid number");
                return false;
            }
            while (p->pos < p->len && p->src[p->pos] >= '0' && p->src[p->pos] <= '9')
                p->pos++;
        }

        bool has_frac = false, has_exp = false;

        if (p->pos < p->len && p->src[p->pos] == '.')
        {
            has_frac = true;
            p->pos++;
            if (p->pos >= p->len || p->src[p->pos] < '0' || p->src[p->pos] > '9')
            {
                jp_error(p, "invalid fraction");
                return false;
            }
            while (p->pos < p->len && p->src[p->pos] >= '0' && p->src[p->pos] <= '9')
                p->pos++;
        }

        if (p->pos < p->len && (p->src[p->pos] == 'e' || p->src[p->pos] == 'E'))
        {
            has_exp = true;
            p->pos++;
            if (p->pos < p->len && (p->src[p->pos] == '+' || p->src[p->pos] == '-'))
                p->pos++;
            if (p->pos >= p->len || p->src[p->pos] < '0' || p->src[p->pos] > '9')
            {
                jp_error(p, "invalid exponent");
                return false;
            }
            while (p->pos < p->len && p->src[p->pos] >= '0' && p->src[p->pos] <= '9')
                p->pos++;
        }

        /* Extract number text */
        int nlen = p->pos - start;
        char tmp[64];
        if (nlen >= 63)
            nlen = 63;
        memcpy(tmp, p->src + start, (size_t)nlen);
        tmp[nlen] = '\0';

        /* Integer if no fraction/exponent */
        if (!has_frac && !has_exp)
        {
            errno = 0;
            char *end = nullptr;
            long long ll = strtoll(tmp, &end, 10);
            if (errno == 0 && end && *end == '\0')
            {
                *out = val_int((int64_t)ll);
                return true;
            }
        }

        /* Float */
        errno = 0;
        char *end = nullptr;
        double d = strtod(tmp, &end);
        if (!end || *end != '\0' || errno == ERANGE || !std::isfinite(d))
        {
            jp_error(p, "number out of range");
            return false;
        }
        *out = val_float(d);
        return true;
    }

    static bool jp_parse_literal(JsonParser *p, const char *lit, int n, Value val, Value *out)
    {
        if (p->pos + n > p->len || memcmp(p->src + p->pos, lit, (size_t)n) != 0)
        {
            char msg[64];
            snprintf(msg, sizeof(msg), "expected '%s'", lit);
            jp_error(p, msg);
            return false;
        }
        p->pos += n;
        *out = val;
        return true;
    }

    static bool jp_parse_array(JsonParser *p, Value *out)
    {
        p->pos++; /* skip '[' */
        ObjArray *arr = new_array(p->gc);

        jp_skip_ws(p);
        if (jp_consume(p, ']'))
        {
            *out = val_obj((Obj *)arr);
            return true;
        }

        for (;;)
        {
            Value elem;
            if (!jp_parse_value(p, &elem))
                return false;
            array_push(p->gc, arr, elem);

            jp_skip_ws(p);
            if (jp_consume(p, ']'))
            {
                *out = val_obj((Obj *)arr);
                return true;
            }
            if (!jp_consume(p, ','))
            {
                jp_error(p, "expected ',' or ']'");
                return false;
            }
            jp_skip_ws(p);
        }
    }

    static bool jp_parse_object(JsonParser *p, Value *out)
    {
        p->pos++; /* skip '{' */
        ObjMap *map = new_map(p->gc);

        jp_skip_ws(p);
        if (jp_consume(p, '}'))
        {
            *out = val_obj((Obj *)map);
            return true;
        }

        for (;;)
        {
            /* Key must be string */
            char *key_buf = nullptr;
            int key_len = 0;
            if (!jp_parse_string(p, &key_buf, &key_len))
                return false;

            ObjString *key_str = create_string(p->gc, key_buf, key_len);
            free(key_buf);

            jp_skip_ws(p);
            if (!jp_consume(p, ':'))
            {
                jp_error(p, "expected ':'");
                return false;
            }

            Value val;
            if (!jp_parse_value(p, &val))
                return false;

            map_set(p->gc, map, val_obj((Obj *)key_str), val);

            jp_skip_ws(p);
            if (jp_consume(p, '}'))
            {
                *out = val_obj((Obj *)map);
                return true;
            }
            if (!jp_consume(p, ','))
            {
                jp_error(p, "expected ',' or '}'");
                return false;
            }
            jp_skip_ws(p);
        }
    }

    static bool jp_parse_value(JsonParser *p, Value *out)
    {
        jp_skip_ws(p);
        if (p->pos >= p->len)
        {
            jp_error(p, "unexpected end of input");
            return false;
        }

        char c = p->src[p->pos];
        switch (c)
        {
        case '"':
        {
            char *str = nullptr;
            int slen = 0;
            if (!jp_parse_string(p, &str, &slen))
                return false;
            *out = val_obj((Obj *)create_string(p->gc, str, slen));
            free(str);
            return true;
        }
        case '{':
            return jp_parse_object(p, out);
        case '[':
            return jp_parse_array(p, out);
        case 't':
            return jp_parse_literal(p, "true", 4, val_bool(true), out);
        case 'f':
            return jp_parse_literal(p, "false", 5, val_bool(false), out);
        case 'n':
            return jp_parse_literal(p, "null", 4, val_nil(), out);
        default:
            if (c == '-' || (c >= '0' && c <= '9'))
                return jp_parse_number(p, out);
            jp_error(p, "unexpected token");
            return false;
        }
    }

    /* =========================================================
    ** json.parse(string) → value
    ** ========================================================= */
    static int nat_json_parse(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1 || !is_string(args[0]))
        {
            vm->runtime_error("json.parse() expects a string.");
            return -1;
        }

        ObjString *input = as_string(args[0]);

        JsonParser p;
        p.vm = vm;
        p.gc = &vm->get_gc();
        p.src = input->chars;
        p.len = input->length;
        p.pos = 0;
        p.error[0] = '\0';

        Value result;
        if (!jp_parse_value(&p, &result))
        {
            vm->runtime_error("json.parse: %s", p.error);
            return -1;
        }

        /* Check trailing chars */
        jp_skip_ws(&p);
        if (p.pos != p.len)
        {
            vm->runtime_error("json.parse: unexpected trailing characters at pos %d", p.pos);
            return -1;
        }

        args[0] = result;
        return 1;
    }

    /* =========================================================
    ** json.stringify(value, indent?) → string
    **   indent: true (default 2), int (spaces), or omit (compact)
    ** ========================================================= */
    static int nat_json_stringify(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1)
        {
            vm->runtime_error("json.stringify() expects a value.");
            return -1;
        }

        StringifyCtx ctx;
        ctx.vm = vm;
        ctx.gc = &vm->get_gc();
        ctx.pretty = false;
        ctx.indent_width = 0;
        ctx.stack_depth = 0;
        ctx.error[0] = '\0';

        if (nargs >= 2)
        {
            if (is_bool(args[1]))
            {
                ctx.pretty = args[1].as.boolean;
                ctx.indent_width = ctx.pretty ? 2 : 0;
            }
            else if (is_int(args[1]))
            {
                int w = (int)args[1].as.integer;
                if (w < 0)
                    w = 0;
                if (w > 16)
                    w = 16;
                ctx.indent_width = w;
                ctx.pretty = w > 0;
            }
        }

        StrBuf sb;
        sb_init(&sb);

        if (!json_stringify_value(&ctx, &sb, args[0], 0))
        {
            sb_free(&sb);
            vm->runtime_error("json.stringify: %s", ctx.error);
            return -1;
        }

        args[0] = val_obj((Obj *)vm->make_string(sb.data, sb.len));
        sb_free(&sb);
        return 1;
    }

    /* =========================================================
    ** Registration
    ** ========================================================= */

    static const NativeReg json_functions[] = {
        {"parse", nat_json_parse, 1},
        {"stringify", nat_json_stringify, -1},
        {nullptr, nullptr, 0}
    };

    extern const NativeLib zen_lib_json = {
        "json",
        json_functions,
        2,
        nullptr, 0, nullptr
    };

} /* namespace zen */
