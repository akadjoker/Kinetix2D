/* =========================================================
** builtin_io.cpp — io module for Zen
**
** All I/O goes through ZenIO callbacks (vm->callbacks().io).
** Embedders (SDL, WASM, etc.) can replace open/read/write/close.
**
** File class (via ClassBuilder, registered in init_fn):
**   f = File("path.txt", "r")
**   data = f.read()          # read all → string
**   f.read(1024)             # read N bytes → string
**   f.write("hello")         # write string → int
**   f.seek(0)                # seek SET
**   f.seek(0, 2)             # seek END
**   pos = f.tell()           # current position
**   buf = f.readbytes()      # read all → Uint8Array
**   f.writebytes(buf)        # write buffer → int
**   f.close()
**
** Module-level helpers:
**   io.read(path)            → string
**   io.write(path, text)     → nil
**   io.append(path, text)    → nil
**   io.readbytes(path)       → Uint8Array
**   io.writebytes(path, buf) → nil
**   io.exists(path)          → bool
**   io.size(path)            → int
**   io.readlines(path)       → list of strings
** ========================================================= */

#include "module.h"
#include "vm.h"
#include "memory.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace zen
{
    /* ============================================================
    ** File class — native data wraps a ZenFile handle
    ** ============================================================ */

    struct FileData
    {
        ZenFile handle;
        bool    closed;
    };

    /* File(path, mode?) — constructor */
    static void *file_ctor(VM *vm, int argc, Value *args)
    {
        if (argc < 1 || !is_string(args[0]))
        {
            vm->runtime_error("File: expected string path");
            return nullptr;
        }
        const char *path = as_cstring(args[0]);
        const char *mode = "r";
        if (argc >= 2 && is_string(args[1]))
            mode = as_cstring(args[1]);

        const ZenCallbacks &cb = vm->callbacks();
        ZenFile h = cb.io.open(path, mode, cb.userdata);
        if (!h)
        {
            vm->runtime_error("File: cannot open '%s' (mode '%s')", path, mode);
            return nullptr;
        }
        FileData *fd = (FileData *)malloc(sizeof(FileData));
        fd->handle = h;
        fd->closed = false;
        return fd;
    }

    /* destructor — auto-close on GC */
    static void file_dtor(VM *vm, void *data)
    {
        FileData *fd = (FileData *)data;
        if (fd && !fd->closed && fd->handle)
        {
            const ZenCallbacks &cb = vm->callbacks();
            cb.io.close(fd->handle, cb.userdata);
        }
        free(fd);
    }

    /* f.read(size?) → string */
    static int file_read(VM *vm, Value *args, int nargs)
    {
        FileData *fd = zen_instance_data<FileData>(args[-1]);
        if (!fd || fd->closed) { vm->runtime_error("File.read: file is closed"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();
        long size = -1;
        if (nargs >= 1 && is_int(args[0]))
        {
            size = (long)args[0].as.integer;
        }
        else
        {
            long cur = cb.io.seek(fd->handle, 0, 1, cb.userdata);
            long end = cb.io.seek(fd->handle, 0, 2, cb.userdata);
            size = end - cur;
            cb.io.seek(fd->handle, cur, 0, cb.userdata);
        }
        if (size <= 0) { args[0] = val_obj((Obj *)vm->make_string("", 0)); return 1; }
        if (size > 100*1024*1024) { vm->runtime_error("File.read: too large"); return -1; }
        char *buf = (char *)malloc((size_t)size + 1);
        if (!buf) { vm->runtime_error("File.read: OOM"); return -1; }
        long n = cb.io.read(fd->handle, buf, size, cb.userdata);
        if (n < 0) n = 0;
        buf[n] = '\0';
        ObjString *s = vm->make_string(buf, (int)n);
        free(buf);
        args[0] = val_obj((Obj *)s);
        return 1;
    }

    /* f.readlines() → list of strings */
    static int file_readlines(VM *vm, Value *args, int)
    {
        FileData *fd = zen_instance_data<FileData>(args[-1]);
        if (!fd || fd->closed) { vm->runtime_error("File.readlines: file is closed"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();

        long cur = cb.io.seek(fd->handle, 0, 1, cb.userdata);
        long end = cb.io.seek(fd->handle, 0, 2, cb.userdata);
        cb.io.seek(fd->handle, cur, 0, cb.userdata);
        long size = end - cur;

        GC *gc = &vm->get_gc();
        ObjArray *arr = new_array(gc);
        args[0] = val_obj((Obj *)arr); /* root array against GC during string creation */
        if (size <= 0) { return 1; }
        if (size > 100 * 1024 * 1024) { vm->runtime_error("File.readlines: too large"); return -1; }

        char *raw = (char *)malloc((size_t)size + 1);
        if (!raw) { vm->runtime_error("File.readlines: OOM"); return -1; }
        long n = cb.io.read(fd->handle, raw, size, cb.userdata);
        if (n < 0) n = 0;
        raw[n] = '\0';

        const char *p = raw;
        const char *e = raw + n;
        while (p < e)
        {
            const char *nl = (const char *)memchr(p, '\n', (size_t)(e - p));
            if (!nl) nl = e;
            int len = (int)(nl - p);
            if (len > 0 && p[len - 1] == '\r') len--;
            ObjString *line = vm->make_string(p, len);
            array_push(gc, as_array(args[0]), val_obj((Obj *)line));
            p = nl + 1;
        }
        free(raw);
        return 1;
    }

    /* f.write(str) → int */
    static int file_write(VM *vm, Value *args, int nargs)
    {
        FileData *fd = zen_instance_data<FileData>(args[-1]);
        if (!fd || fd->closed) { vm->runtime_error("File.write: closed"); return -1; }
        if (nargs < 1 || !is_string(args[0]))
        { vm->runtime_error("File.write: expected string"); return -1; }
        ObjString *data = as_string(args[0]);
        const ZenCallbacks &cb = vm->callbacks();
        long n = cb.io.write(fd->handle, data->chars, (long)data->length, cb.userdata);
        args[0] = val_int((int64_t)n);
        return 1;
    }

    /* f.seek(offset, whence?) — 0=SET 1=CUR 2=END */
    static int file_seek(VM *vm, Value *args, int nargs)
    {
        FileData *fd = zen_instance_data<FileData>(args[-1]);
        if (!fd || fd->closed) { vm->runtime_error("File.seek: closed"); return -1; }
        if (nargs < 1 || !is_int(args[0]))
        { vm->runtime_error("File.seek: expected int offset"); return -1; }
        long offset = (long)args[0].as.integer;
        int whence = 0;
        if (nargs >= 2 && is_int(args[1])) whence = (int)args[1].as.integer;
        const ZenCallbacks &cb = vm->callbacks();
        long pos = cb.io.seek(fd->handle, offset, whence, cb.userdata);
        args[0] = val_int((int64_t)pos);
        return 1;
    }

    /* f.tell() → int */
    static int file_tell(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        FileData *fd = zen_instance_data<FileData>(args[-1]);
        if (!fd || fd->closed) { vm->runtime_error("File.tell: closed"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();
        long pos = cb.io.seek(fd->handle, 0, 1, cb.userdata);
        args[0] = val_int((int64_t)pos);
        return 1;
    }

    /* f.close() → nil */
    static int file_close(VM *vm, Value *args, int nargs)
    {
        (void)nargs;
        FileData *fd = zen_instance_data<FileData>(args[-1]);
        if (!fd || fd->closed) return 0;
        const ZenCallbacks &cb = vm->callbacks();
        cb.io.close(fd->handle, cb.userdata);
        fd->closed = true;
        return 0;
    }

    /* f.readbytes(size?) → Uint8Array */
    static int file_readbytes(VM *vm, Value *args, int nargs)
    {
        FileData *fd = zen_instance_data<FileData>(args[-1]);
        if (!fd || fd->closed) { vm->runtime_error("File.readbytes: closed"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();
        long size = -1;
        if (nargs >= 1 && is_int(args[0]))
        {
            size = (long)args[0].as.integer;
        }
        else
        {
            long cur = cb.io.seek(fd->handle, 0, 1, cb.userdata);
            long end = cb.io.seek(fd->handle, 0, 2, cb.userdata);
            size = end - cur;
            cb.io.seek(fd->handle, cur, 0, cb.userdata);
        }
        if (size <= 0) { args[0] = val_obj((Obj *)new_buffer(&vm->get_gc(), BUF_UINT8, 0)); return 1; }
        if (size > 100*1024*1024) { vm->runtime_error("File.readbytes: too large"); return -1; }
        GC *gc = &vm->get_gc();
        ObjBuffer *buf = new_buffer(gc, BUF_UINT8, (int32_t)size);
        cb.io.read(fd->handle, buf->data, size, cb.userdata);
        args[0] = val_obj((Obj *)buf);
        return 1;
    }

    /* f.writebytes(buffer) → int */
    static int file_writebytes(VM *vm, Value *args, int nargs)
    {
        FileData *fd = zen_instance_data<FileData>(args[-1]);
        if (!fd || fd->closed) { vm->runtime_error("File.writebytes: closed"); return -1; }
        if (nargs < 1 || !is_buffer(args[0]))
        { vm->runtime_error("File.writebytes: expected buffer"); return -1; }
        ObjBuffer *b = as_buffer(args[0]);
        int elem_sz = buffer_elem_size[b->btype];
        const ZenCallbacks &cb = vm->callbacks();
        long n = cb.io.write(fd->handle, b->data, (long)(b->count * elem_sz), cb.userdata);
        args[0] = val_int((int64_t)n);
        return 1;
    }

    /* ============================================================
    ** Module-level convenience functions (all use ZenIO callbacks)
    ** ============================================================ */

    /* io.read(path) → string */
    static int nat_read(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        { vm->runtime_error("io.read: expected string path"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();
        ZenFile f = cb.io.open(as_cstring(args[0]), "rb", cb.userdata);
        if (!f) { vm->runtime_error("io.read: cannot open '%s'", as_cstring(args[0])); return -1; }
        long end = cb.io.seek(f, 0, 2, cb.userdata);
        cb.io.seek(f, 0, 0, cb.userdata);
        if (end < 0 || end > 100*1024*1024)
        { cb.io.close(f, cb.userdata); vm->runtime_error("io.read: too large"); return -1; }
        char *buf = (char *)malloc((size_t)end + 1);
        if (!buf) { cb.io.close(f, cb.userdata); vm->runtime_error("io.read: OOM"); return -1; }
        long n = cb.io.read(f, buf, end, cb.userdata);
        cb.io.close(f, cb.userdata);
        if (n < 0) n = 0;
        buf[n] = '\0';
        ObjString *s = vm->make_string(buf, (int)n);
        free(buf);
        args[0] = val_obj((Obj *)s);
        return 1;
    }

    /* io.write(path, text) → nil */
    static int nat_write(VM *vm, Value *args, int nargs)
    {
        if (nargs != 2 || !is_string(args[0]) || !is_string(args[1]))
        { vm->runtime_error("io.write: expected (string, string)"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();
        ZenFile f = cb.io.open(as_cstring(args[0]), "wb", cb.userdata);
        if (!f) { vm->runtime_error("io.write: cannot open '%s'", as_cstring(args[0])); return -1; }
        ObjString *data = as_string(args[1]);
        cb.io.write(f, data->chars, (long)data->length, cb.userdata);
        cb.io.close(f, cb.userdata);
        return 0;
    }

    /* io.append(path, text) → nil */
    static int nat_append(VM *vm, Value *args, int nargs)
    {
        if (nargs != 2 || !is_string(args[0]) || !is_string(args[1]))
        { vm->runtime_error("io.append: expected (string, string)"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();
        ZenFile f = cb.io.open(as_cstring(args[0]), "ab", cb.userdata);
        if (!f) { vm->runtime_error("io.append: cannot open '%s'", as_cstring(args[0])); return -1; }
        ObjString *data = as_string(args[1]);
        cb.io.write(f, data->chars, (long)data->length, cb.userdata);
        cb.io.close(f, cb.userdata);
        return 0;
    }

    /* io.readbytes(path) → Uint8Array */
    static int nat_readbytes(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        { vm->runtime_error("io.readbytes: expected string path"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();
        ZenFile f = cb.io.open(as_cstring(args[0]), "rb", cb.userdata);
        if (!f) { vm->runtime_error("io.readbytes: cannot open '%s'", as_cstring(args[0])); return -1; }
        long end = cb.io.seek(f, 0, 2, cb.userdata);
        cb.io.seek(f, 0, 0, cb.userdata);
        if (end < 0 || end > 100*1024*1024)
        { cb.io.close(f, cb.userdata); vm->runtime_error("io.readbytes: too large"); return -1; }
        GC *gc = &vm->get_gc();
        ObjBuffer *buf = new_buffer(gc, BUF_UINT8, (int32_t)end);
        cb.io.read(f, buf->data, end, cb.userdata);
        cb.io.close(f, cb.userdata);
        args[0] = val_obj((Obj *)buf);
        return 1;
    }

    /* io.writebytes(path, buffer) → nil */
    static int nat_writebytes(VM *vm, Value *args, int nargs)
    {
        if (nargs != 2 || !is_string(args[0]) || !is_buffer(args[1]))
        { vm->runtime_error("io.writebytes: expected (string, buffer)"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();
        ZenFile f = cb.io.open(as_cstring(args[0]), "wb", cb.userdata);
        if (!f) { vm->runtime_error("io.writebytes: cannot open '%s'", as_cstring(args[0])); return -1; }
        ObjBuffer *buf = as_buffer(args[1]);
        int elem_sz = buffer_elem_size[buf->btype];
        cb.io.write(f, buf->data, (long)(buf->count * elem_sz), cb.userdata);
        cb.io.close(f, cb.userdata);
        return 0;
    }

    /* io.exists(path) → bool */
    static int nat_exists(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        { vm->runtime_error("io.exists: expected string path"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();
        args[0] = val_bool(cb.io.exists(as_cstring(args[0]), cb.userdata) != 0);
        return 1;
    }

    /* io.size(path) → int */
    static int nat_size(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        { vm->runtime_error("io.size: expected string path"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();
        ZenFile f = cb.io.open(as_cstring(args[0]), "rb", cb.userdata);
        if (!f) { vm->runtime_error("io.size: cannot open '%s'", as_cstring(args[0])); return -1; }
        long sz = cb.io.seek(f, 0, 2, cb.userdata);
        cb.io.close(f, cb.userdata);
        args[0] = val_int((int64_t)sz);
        return 1;
    }

    /* io.readlines(path) → list of strings */
    static int nat_readlines(VM *vm, Value *args, int nargs)
    {
        if (nargs != 1 || !is_string(args[0]))
        { vm->runtime_error("io.readlines: expected string path"); return -1; }
        const ZenCallbacks &cb = vm->callbacks();
        ZenFile f = cb.io.open(as_cstring(args[0]), "rb", cb.userdata);
        if (!f) { vm->runtime_error("io.readlines: cannot open '%s'", as_cstring(args[0])); return -1; }
        long end = cb.io.seek(f, 0, 2, cb.userdata);
        cb.io.seek(f, 0, 0, cb.userdata);
        if (end < 0 || end > 100*1024*1024)
        { cb.io.close(f, cb.userdata); vm->runtime_error("io.readlines: too large"); return -1; }
        char *raw = (char *)malloc((size_t)end + 1);
        if (!raw) { cb.io.close(f, cb.userdata); vm->runtime_error("io.readlines: OOM"); return -1; }
        long n = cb.io.read(f, raw, end, cb.userdata);
        cb.io.close(f, cb.userdata);
        if (n < 0) n = 0;
        raw[n] = '\0';

        GC *gc = &vm->get_gc();
        ObjArray *arr = new_array(gc);
        args[0] = val_obj((Obj *)arr); /* root array against GC during string creation */
        const char *p = raw;
        const char *e = raw + n;
        while (p < e)
        {
            const char *nl = (const char *)memchr(p, '\n', (size_t)(e - p));
            if (!nl) nl = e;
            int len = (int)(nl - p);
            if (len > 0 && p[len - 1] == '\r') len--;
            ObjString *line = vm->make_string(p, len);
            array_push(gc, as_array(args[0]), val_obj((Obj *)line));
            p = nl + 1;
        }
        free(raw);
        return 1;
    }

    /* ---- Module registration ---- */

    static const NativeReg io_functions[] = {
        {"read",       nat_read,       1},
        {"write",      nat_write,      2},
        {"append",     nat_append,     2},
        {"readbytes",  nat_readbytes,  1},
        {"writebytes", nat_writebytes, 2},
        {"exists",     nat_exists,     1},
        {"size",       nat_size,       1},
        {"readlines",  nat_readlines,  1},
        {nullptr, nullptr, 0}
    };

    /* init_fn: register File class via ClassBuilder */
    static void io_lib_init(VM *vm)
    {
        vm->def_class("File")
            .ctor(file_ctor)
            .dtor(file_dtor)
            .method("read",       file_read,       -1)
            .method("readlines",  file_readlines,   0)
            .method("write",      file_write,       1)
            .method("seek",       file_seek,       -1)
            .method("tell",       file_tell,        0)
            .method("close",      file_close,       0)
            .method("readbytes",  file_readbytes,  -1)
            .method("writebytes", file_writebytes,  1)
            .end();
    }

    extern const NativeLib zen_lib_io = {
        "io",
        io_functions,
        8,
        nullptr, 0,
        io_lib_init
    };

} /* namespace zen */
