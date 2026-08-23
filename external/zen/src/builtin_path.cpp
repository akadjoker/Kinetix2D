/* =========================================================
** builtin_path.cpp — path module for Zen
**
** Pure string manipulation (no I/O). Separators default to '/'.
**
**   import path
**   path.join("a", "b", "c")   → "a/b/c"
**   path.dirname("/a/b/c.txt") → "/a/b"
**   path.basename("/a/b/c.txt")→ "c.txt"
**   path.ext("/a/b.tar.gz")    → ".gz"
**   path.stem("/a/b/c.txt")    → "c"
**   path.isabs("/foo")         → true
**   path.norm("a//b/../c")     → "a/c"
** ========================================================= */

#include "module.h"
#include "vm.h"
#include "memory.h"
#include <cstring>
#include <cstdlib>

namespace zen
{
    /* ---- helpers ---- */

    static inline bool is_sep(char c) { return c == '/' || c == '\\'; }

    /* path.join(a, b, ...) → string — variadic */
    static int nat_join(VM *vm, Value *args, int nargs)
    {
        if (nargs < 1) { args[0] = val_obj((Obj *)vm->make_string("", 0)); return 1; }
        /* build into a temp buffer */
        char buf[4096];
        int pos = 0;
        for (int i = 0; i < nargs; i++)
        {
            if (!is_string(args[i])) { vm->runtime_error("path.join: all args must be strings"); return -1; }
            const char *s = as_cstring(args[i]);
            int len = as_string(args[i])->length;
            if (len == 0) continue;
            /* if this part is absolute, reset */
            if (is_sep(s[0]) && i > 0)
                pos = 0;
            /* add separator if needed */
            if (pos > 0 && !is_sep(buf[pos - 1]))
                buf[pos++] = '/';
            if (pos + len >= (int)sizeof(buf) - 1)
            { vm->runtime_error("path.join: result too long"); return -1; }
            memcpy(buf + pos, s, (size_t)len);
            pos += len;
        }
        buf[pos] = '\0';
        args[0] = val_obj((Obj *)vm->make_string(buf, pos));
        return 1;
    }

    /* path.dirname(p) → string */
    static int nat_dirname(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        { vm->runtime_error("path.dirname: expected string"); return -1; }
        const char *p = as_cstring(args[0]);
        int len = as_string(args[0])->length;
        int i = len - 1;
        /* skip trailing separators */
        while (i > 0 && is_sep(p[i])) i--;
        /* find last separator */
        while (i >= 0 && !is_sep(p[i])) i--;
        if (i < 0) { args[0] = val_obj((Obj *)vm->make_string(".", 1)); return 1; }
        if (i == 0) { args[0] = val_obj((Obj *)vm->make_string("/", 1)); return 1; }
        args[0] = val_obj((Obj *)vm->make_string(p, i));
        return 1;
    }

    /* path.basename(p) → string */
    static int nat_basename(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        { vm->runtime_error("path.basename: expected string"); return -1; }
        const char *p = as_cstring(args[0]);
        int len = as_string(args[0])->length;
        int end = len;
        /* skip trailing separators */
        while (end > 0 && is_sep(p[end - 1])) end--;
        int start = end - 1;
        while (start >= 0 && !is_sep(p[start])) start--;
        start++;
        args[0] = val_obj((Obj *)vm->make_string(p + start, end - start));
        return 1;
    }

    /* path.ext(p) → string (includes the dot) */
    static int nat_ext(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        { vm->runtime_error("path.ext: expected string"); return -1; }
        const char *p = as_cstring(args[0]);
        int len = as_string(args[0])->length;
        int i = len - 1;
        while (i >= 0 && !is_sep(p[i]))
        {
            if (p[i] == '.')
            {
                args[0] = val_obj((Obj *)vm->make_string(p + i, len - i));
                return 1;
            }
            i--;
        }
        args[0] = val_obj((Obj *)vm->make_string("", 0));
        return 1;
    }

    /* path.stem(p) → string (basename without ext) */
    static int nat_stem(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        { vm->runtime_error("path.stem: expected string"); return -1; }
        const char *p = as_cstring(args[0]);
        int len = as_string(args[0])->length;
        /* find basename start */
        int end = len;
        while (end > 0 && is_sep(p[end - 1])) end--;
        int start = end - 1;
        while (start >= 0 && !is_sep(p[start])) start--;
        start++;
        /* find extension dot in basename */
        int dot = end - 1;
        while (dot > start && p[dot] != '.') dot--;
        if (dot <= start) dot = end; /* no dot found */
        args[0] = val_obj((Obj *)vm->make_string(p + start, dot - start));
        return 1;
    }

    /* path.isabs(p) → bool */
    static int nat_isabs(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        { vm->runtime_error("path.isabs: expected string"); return -1; }
        const char *p = as_cstring(args[0]);
        args[0] = val_bool(p[0] == '/');
        return 1;
    }

    /* path.norm(p) → string — collapse . and .. and double seps */
    static int nat_norm(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        { vm->runtime_error("path.norm: expected string"); return -1; }
        const char *src = as_cstring(args[0]);
        int slen = as_string(args[0])->length;
        if (slen == 0) { args[0] = val_obj((Obj *)vm->make_string(".", 1)); return 1; }

        bool absolute = is_sep(src[0]);

        /* split into parts */
        const char *parts[256];
        int plens[256];
        int nparts = 0;

        const char *p = src;
        const char *end = src + slen;
        while (p < end)
        {
            while (p < end && is_sep(*p)) p++;
            if (p >= end) break;
            const char *seg = p;
            while (p < end && !is_sep(*p)) p++;
            int seglen = (int)(p - seg);
            if (seglen == 1 && seg[0] == '.') continue;
            if (seglen == 2 && seg[0] == '.' && seg[1] == '.')
            {
                if (nparts > 0 && !(plens[nparts-1] == 2 && parts[nparts-1][0] == '.' && parts[nparts-1][1] == '.'))
                    nparts--;
                else if (!absolute)
                {
                    parts[nparts] = seg;
                    plens[nparts] = seglen;
                    nparts++;
                }
                continue;
            }
            if (nparts < 256)
            {
                parts[nparts] = seg;
                plens[nparts] = seglen;
                nparts++;
            }
        }

        char buf[4096];
        int pos = 0;
        if (absolute) buf[pos++] = '/';
        for (int i = 0; i < nparts; i++)
        {
            if (i > 0) buf[pos++] = '/';
            memcpy(buf + pos, parts[i], (size_t)plens[i]);
            pos += plens[i];
        }
        if (pos == 0) { buf[pos++] = '.'; }
        buf[pos] = '\0';
        args[0] = val_obj((Obj *)vm->make_string(buf, pos));
        return 1;
    }

    /* ---- registration ---- */

    static const NativeReg path_functions[] = {
        {"join",     nat_join,     -1},
        {"dirname",  nat_dirname,   1},
        {"basename", nat_basename,  1},
        {"ext",      nat_ext,       1},
        {"stem",     nat_stem,      1},
        {"isabs",    nat_isabs,     1},
        {"norm",     nat_norm,      1},
        {nullptr, nullptr, 0}
    };

    extern const NativeLib zen_lib_path = {
        "path",
        path_functions,
        7,
        nullptr, 0, nullptr
    };

} /* namespace zen */
