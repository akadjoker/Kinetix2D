#include "debug.h"
#include "opcodes.h"
#include "vm.h"

#include <cstdio>
#include <cstring>

namespace zen
{

    /* --- Nomes dos opcodes --- */
    static const char *s_opnames[] = {
        "LOADNIL",
        "LOADBOOL",
        "LOADK",
        "LOADI",
        "MOVE",
        "GETGLOBAL",
        "SETGLOBAL",
        "ADD",
        "SUB",
        "MUL",
        "DIV",
        "IDIV",
        "MOD",
        "POW",
        "NEG",
        "ADD_OBJ",
        "SUB_OBJ",
        "MUL_OBJ",
        "DIV_OBJ",
        "MOD_OBJ",
        "NEG_OBJ",
        "EQ_OBJ",
        "LT_OBJ",
        "LE_OBJ",
        "ADDI",
        "SUBI",
        "BAND",
        "BOR",
        "BXOR",
        "BNOT",
        "SHL",
        "SHR",
        "EQ",
        "LT",
        "LE",
        "NOT",
        "CONTAINS",
        "JMP",
        "JMPIF",
        "JMPIFNOT",
        "CALL",
        "CALLGLOBAL",
        "RETURN",
        "CLOSURE",
        "GETUPVAL",
        "SETUPVAL",
        "CLOSE",
        "NEWFIBER",
        "RESUME",
        "YIELD",
        "FOR_ITER",
        "NEWARRAY",
        "NEWMAP",
        "NEWSET",
        "NEWBUFFER",
        "APPEND",
        "SETADD",
        "GETFIELD",
        "SETFIELD",
        "GETFIELD_IDX",
        "SETFIELD_IDX",
        "GETINDEX",
        "SETINDEX",
        "DELINDEX",
        "GETSLICE",
        "INVOKE",
        "INVOKE_VT",
        "SUPER_INVOKE",
        "NEWCLASS",
        "NEWINSTANCE",
        "GETMETHOD",
        "CLASSFIELD",
        "CONCAT",
        "STRADD",
        "TOSTRING",
        "TOSTRING_OBJ",
        "LEN",
        "PRINT",
        "LTJMPIFNOT",
        "LEJMPIFNOT",
        "FORPREP",
        "FORLOOP",
        "GETFIELD_MUL",
        "GETFIELD_SUB",
        "ITER_ELEM",
        "EVAL",
        "ASSERT",
        "HALT",
        "IMPORT",
    };

    const char *opcode_name(OpCode op)
    {
        int idx = (int)op;
        int count = (int)(sizeof(s_opnames) / sizeof(s_opnames[0]));
        if (idx >= 0 && idx < count)
            return s_opnames[idx];
        return "???";
    }

    /* --- Print value (compact) --- */
    void print_value(Value val)
    {
        switch (val.type)
        {
        case VAL_NIL:
            printf("None");
            break;
        case VAL_BOOL:
            printf(val.as.boolean ? "True" : "False");
            break;
        case VAL_INT:
            printf("%lld", (long long)val.as.integer);
            break;
        case VAL_FLOAT:
            printf("%g", val.as.number);
            break;
        case VAL_SMALL_STRING:
            printf("\"");
            for (int i = 0; i < val.as.sso.len; i++)
                printf("%c", val.as.sso.chars[i]);
            printf("\"");
            break;
        case VAL_OBJ:
        {
            Obj *obj = val.as.obj;
            switch (obj->type)
            {
            case OBJ_STRING:
                printf("\"%s\"", ((ObjString *)obj)->chars);
                break;
            case OBJ_FUNC:
            {
                ObjFunc *fn = (ObjFunc *)obj;
                if (fn->name)
                    printf("<fn %s>", fn->name->chars);
                else
                    printf("<fn script>");
                break;
            }
            case OBJ_NATIVE:
                printf("<native %s>", ((ObjNative *)obj)->name->chars);
                break;
            case OBJ_CLOSURE:
            {
                ObjClosure *cl = (ObjClosure *)obj;
                if (cl->func->name)
                    printf("<closure %s>", cl->func->name->chars);
                else
                    printf("<closure>");
                break;
            }
            case OBJ_FIBER:
                printf("<fiber %p>", (void *)obj);
                break;
            case OBJ_UPVALUE:
                printf("<upvalue>");
                break;
            case OBJ_ARRAY:
                printf("<array[%d]>", arr_count((ObjArray *)obj));
                break;
            case OBJ_MAP:
                printf("<map[%d]>", ((ObjMap *)obj)->count);
                break;
            case OBJ_SET:
                printf("<set[%d]>", ((ObjSet *)obj)->count);
                break;
            case OBJ_BUFFER:
            {
                ObjBuffer *b = (ObjBuffer *)obj;
                static const char *bnames[] = {"Int8Array","Int16Array","Int32Array","Uint8Array","Uint16Array","Uint32Array","Float32Array","Float64Array"};
                printf("<%s[%d]>", bnames[b->btype], b->count);
                break;
            }
            case OBJ_CLASS:
                printf("<class %s>", ((ObjClass *)obj)->name->chars);
                break;
            case OBJ_STRUCT_DEF:
                printf("<struct_def %s>", ((ObjStructDef *)obj)->name->chars);
                break;
            case OBJ_STRUCT:
                printf("<struct %s>", ((ObjStruct *)obj)->def->name->chars);
                break;
            case OBJ_INSTANCE:
                printf("<instance %s>", ((ObjInstance *)obj)->klass->name->chars);
                break;
            case OBJ_RANGE:
            {
                ObjRange *r = (ObjRange *)obj;
                printf("<range(%lld,%lld,%lld)>", (long long)r->start, (long long)r->stop, (long long)r->step);
                break;
            }
            case OBJ_NATIVE_STRUCT_DEF:
                printf("<native_struct_def>");
                break;
            case OBJ_NATIVE_STRUCT:
                printf("<native_struct>");
                break;
            }
            break;
        }
        case VAL_PTR:
            printf("<ptr %p>", val.as.pointer);
            break;
        }
    }

    void println_value(Value val)
    {
        print_value(val);
        printf("\n");
    }

    /* --- helpers --- */

    /* Get a constant's string representation for annotations */
    static const char *const_str(ObjFunc *func, int ki)
    {
        if (ki < 0 || ki >= func->const_count)
            return nullptr;
        Value v = func->constants[ki];
        if (is_string(v))
            return as_string(v)->chars;
        return nullptr;
    }

    static const char *global_name_safe(VM *vm, int idx)
    {
        if (!vm || idx < 0 || idx >= vm->num_globals())
            return nullptr;
        return vm->global_name(idx);
    }

    static const char *selector_name_safe(VM *vm, int idx)
    {
        if (!vm || idx < 0 || idx >= vm->num_selectors())
            return nullptr;
        return vm->selector_name(idx);
    }

    /* --- Disassemble one instruction --- */
    int disassemble_instruction(ObjFunc *func, int offset, VM *vm)
    {
        uint32_t instr = func->code[offset];
        OpCode op = (OpCode)ZEN_OP(instr);
        int a = ZEN_A(instr);
        int b = ZEN_B(instr);
        int c = ZEN_C(instr);
        int bx = ZEN_BX(instr);
        int sbx = ZEN_SBX(instr);

        /* Line number */
        if (offset > 0 && func->lines && func->lines[offset] == func->lines[offset - 1])
            printf("   |  ");
        else
            printf("%4d  ", func->lines ? func->lines[offset] : 0);

        /* Offset + opcode */
        printf("%04d  %-16s", offset, opcode_name(op));

        /* Per-opcode annotation */
        switch (op)
        {
        /* === Load/Store === */
        case OP_LOADNIL:
            printf("R[%d] = nil", a);
            break;
        case OP_LOADBOOL:
            printf("R[%d] = %s", a, b ? "true" : "false");
            if (c) printf(" ; skip next");
            break;
        case OP_LOADK:
            printf("R[%d] = K[%d]", a, bx);
            if (bx < func->const_count)
            {
                printf("  \t; ");
                print_value(func->constants[bx]);
            }
            break;
        case OP_LOADI:
            printf("R[%d] = %d", a, sbx);
            break;
        case OP_MOVE:
            printf("R[%d] = R[%d]", a, b);
            break;

        /* === Globals === */
        case OP_GETGLOBAL:
        {
            const char *gn = global_name_safe(vm, bx);
            if (gn)
                printf("R[%d] = G[%d]  \t; '%s'", a, bx, gn);
            else
                printf("R[%d] = G[%d]", a, bx);
            break;
        }
        case OP_SETGLOBAL:
        {
            const char *gn = global_name_safe(vm, bx);
            if (gn)
                printf("G[%d] = R[%d]  \t; '%s'", bx, a, gn);
            else
                printf("G[%d] = R[%d]", bx, a);
            break;
        }

        /* === Arithmetic === */
        case OP_ADD:  printf("R[%d] = R[%d] + R[%d]", a, b, c); break;
        case OP_SUB:  printf("R[%d] = R[%d] - R[%d]", a, b, c); break;
        case OP_MUL:  printf("R[%d] = R[%d] * R[%d]", a, b, c); break;
        case OP_DIV:  printf("R[%d] = R[%d] / R[%d]", a, b, c); break;
        case OP_IDIV: printf("R[%d] = R[%d] // R[%d]", a, b, c); break;
        case OP_MOD:  printf("R[%d] = R[%d] %% R[%d]", a, b, c); break;
        case OP_POW:  printf("R[%d] = R[%d] ** R[%d]", a, b, c); break;
        case OP_NEG:  printf("R[%d] = -R[%d]", a, b); break;

        /* === Object operator overloads === */
        case OP_ADD_OBJ: printf("R[%d] = R[%d].__add__(R[%d])", a, b, c); break;
        case OP_SUB_OBJ: printf("R[%d] = R[%d].__sub__(R[%d])", a, b, c); break;
        case OP_MUL_OBJ: printf("R[%d] = R[%d].__mul__(R[%d])", a, b, c); break;
        case OP_DIV_OBJ: printf("R[%d] = R[%d].__div__(R[%d])", a, b, c); break;
        case OP_MOD_OBJ: printf("R[%d] = R[%d].__mod__(R[%d])", a, b, c); break;
        case OP_NEG_OBJ: printf("R[%d] = R[%d].__neg__()", a, b); break;
        case OP_EQ_OBJ:  printf("R[%d] = R[%d].__eq__(R[%d])", a, b, c); break;
        case OP_LT_OBJ:  printf("R[%d] = R[%d].__lt__(R[%d])", a, b, c); break;
        case OP_LE_OBJ:  printf("R[%d] = R[%d].__le__(R[%d])", a, b, c); break;

        /* === Immediate arithmetic === */
        case OP_ADDI: printf("R[%d] = R[%d] + %d", a, b, (int8_t)c); break;
        case OP_SUBI: printf("R[%d] = R[%d] - %d", a, b, (int8_t)c); break;

        /* === Bitwise === */
        case OP_BAND: printf("R[%d] = R[%d] & R[%d]", a, b, c); break;
        case OP_BOR:  printf("R[%d] = R[%d] | R[%d]", a, b, c); break;
        case OP_BXOR: printf("R[%d] = R[%d] ^ R[%d]", a, b, c); break;
        case OP_BNOT: printf("R[%d] = ~R[%d]", a, b); break;
        case OP_SHL:  printf("R[%d] = R[%d] << R[%d]", a, b, c); break;
        case OP_SHR:  printf("R[%d] = R[%d] >> R[%d]", a, b, c); break;

        /* === Comparison === */
        case OP_EQ:  printf("R[%d] = (R[%d] == R[%d])", a, b, c); break;
        case OP_LT:  printf("R[%d] = (R[%d] < R[%d])", a, b, c); break;
        case OP_LE:  printf("R[%d] = (R[%d] <= R[%d])", a, b, c); break;
        case OP_NOT: printf("R[%d] = !R[%d]", a, b); break;
        case OP_CONTAINS: printf("R[%d] = (R[%d] in R[%d])", a, b, c); break;
        case OP_IS: printf("R[%d] = (R[%d] is R[%d])", a, b, c); break;

        /* === Jumps === */
        case OP_JMP:
            printf("pc += %d  \t; -> %04d", sbx, offset + 1 + sbx);
            break;
        case OP_JMPIF:
            printf("if R[%d]: pc += %d  \t; -> %04d", a, sbx, offset + 1 + sbx);
            break;
        case OP_JMPIFNOT:
            printf("if !R[%d]: pc += %d  \t; -> %04d", a, sbx, offset + 1 + sbx);
            break;

        /* === Functions === */
        case OP_CALL:
            printf("R[%d] = R[%d](%d args)  \t; %d results", a, a, b, c);
            break;
        case OP_CALLGLOBAL:
        {
            /* 2-word: word2 has Bx=global index */
            uint32_t word2 = func->code[offset + 1];
            int gidx = ZEN_BX(word2);
            const char *gn = global_name_safe(vm, gidx);
            if (gn)
                printf("R[%d] = G[%d](%d args)  \t; %s(), %d results", a, gidx, b, gn, c);
            else
                printf("R[%d] = G[%d](%d args)  \t; %d results", a, gidx, b, c);
            printf("\n");
            /* print word2 line */
            printf("   |  %04d  %-16s", offset + 1, "(gidx)");
            printf("G[%d]", gidx);
            if (gn) printf("  \t; '%s'", gn);
            return offset + 2;
        }
        case OP_RETURN:
            if (b == 0)
                printf("return (no value)");
            else if (b == 1)
                printf("return R[%d]", a);
            else
                printf("return R[%d]..R[%d]  \t; %d values", a, a + b - 1, b);
            break;

        /* === Closures/Upvalues === */
        case OP_CLOSURE:
        {
            printf("R[%d] = closure(K[%d])", a, bx);
            if (bx < func->const_count)
            {
                Value v = func->constants[bx];
                if (v.type == VAL_OBJ && v.as.obj)
                {
                    ObjFunc *fn = nullptr;
                    if (v.as.obj->type == OBJ_FUNC)
                        fn = (ObjFunc *)v.as.obj;
                    else if (v.as.obj->type == OBJ_CLOSURE)
                        fn = ((ObjClosure *)v.as.obj)->func;
                    if (fn && fn->name)
                        printf("  \t; %s()", fn->name->chars);
                }
            }
            break;
        }
        case OP_GETUPVAL:
            printf("R[%d] = upval[%d]", a, b);
            break;
        case OP_SETUPVAL:
            printf("upval[%d] = R[%d]", b, a);
            break;
        case OP_CLOSE:
            printf("close upvals >= R[%d]", a);
            break;

        /* === Fibers === */
        case OP_NEWFIBER:
            printf("R[%d] = Fiber(R[%d])", a, b);
            break;
        case OP_RESUME:
            printf("R[%d] = R[%d].resume(R[%d])", a, b, c);
            break;
        case OP_YIELD:
            printf("R[%d] = yield R[%d]", a, b);
            break;
        case OP_AWAIT:
            printf("R[%d] = await R[%d]", a, b);
            break;

        /* === Iteration === */
        case OP_FOR_ITER:
            printf("R[%d] = next(R[%d]); if done -> ???", a, b);
            break;
        case OP_ITER_ELEM:
            printf("R[%d] = iter_elem(R[%d], %d)", a, b, c);
            break;

        /* === Collections === */
        case OP_NEWARRAY:
            printf("R[%d] = []", a);
            break;
        case OP_NEWMAP:
            printf("R[%d] = {}", a);
            break;
        case OP_NEWSET:
            printf("R[%d] = set()", a);
            break;
        case OP_NEWBUFFER:
        {
            static const char *bnames[] = {"i8","i16","i32","u8","u16","u32","f32","f64"};
            const char *bt = (c >= 0 && c < 8) ? bnames[c] : "?";
            printf("R[%d] = %sArray(R[%d])", a, bt, b);
            break;
        }
        case OP_APPEND:
            printf("R[%d].push(R[%d])", a, b);
            break;
        case OP_SETADD:
            printf("R[%d].add(R[%d])", a, b);
            break;

        /* === Fields === */
        case OP_GETFIELD:
        {
            const char *fn = const_str(func, c);
            if (fn)
                printf("R[%d] = R[%d].%s  \t; K[%d]", a, b, fn, c);
            else
                printf("R[%d] = R[%d].K[%d]", a, b, c);
            break;
        }
        case OP_SETFIELD:
        {
            const char *fn = const_str(func, b);
            if (fn)
                printf("R[%d].%s = R[%d]  \t; K[%d]", a, fn, c, b);
            else
                printf("R[%d].K[%d] = R[%d]", a, b, c);
            break;
        }
        case OP_GETFIELD_IDX:
            printf("R[%d] = R[%d].fields[%d]", a, b, c);
            break;
        case OP_SETFIELD_IDX:
            printf("R[%d].fields[%d] = R[%d]", a, b, c);
            break;
        case OP_GETINDEX:
            printf("R[%d] = R[%d][R[%d]]", a, b, c);
            break;
        case OP_SETINDEX:
            printf("R[%d][R[%d]] = R[%d]", a, b, c);
            break;
        case OP_DELINDEX:
            printf("del R[%d][R[%d]]", a, b);
            break;
        case OP_GETSLICE:
            printf("R[%d] = R[%d][R[%d]:R[%d]:R[%d]]", a, b, c, c + 1, c + 2);
            break;

        /* === Method calls === */
        case OP_INVOKE:
        {
            /* 2-word: word2 = (sel_slot << 16) | name_ki */
            uint32_t word2 = func->code[offset + 1];
            int sel_slot = (int)(word2 >> 16);
            int name_ki = (int)(word2 & 0xFFFF);
            const char *mname = const_str(func, name_ki);
            const char *sname = selector_name_safe(vm, sel_slot);
            if (mname)
                printf("R[%d] = R[%d].%s(%d args)  \t; sel=%d", a, a, mname, b, sel_slot);
            else if (sname)
                printf("R[%d] = R[%d].%s(%d args)  \t; K[%d], sel=%d", a, a, sname, b, name_ki, sel_slot);
            else
                printf("R[%d] = R[%d].K[%d](%d args)  \t; sel=%d", a, a, name_ki, b, sel_slot);
            printf("\n");
            printf("   |  %04d  %-16s", offset + 1, "(invoke-data)");
            printf("sel=%d name_ki=%d", sel_slot, name_ki);
            if (mname) printf("  \t; \"%s\"", mname);
            return offset + 2;
        }
        case OP_INVOKE_VT:
        {
            const char *sname = selector_name_safe(vm, c);
            if (sname)
                printf("R[%d] = R[%d].vt[%d](%d args)  \t; .%s()", a, a, c, b, sname);
            else
                printf("R[%d] = R[%d].vt[%d](%d args)", a, a, c, b);
            break;
        }
        case OP_SUPER_INVOKE:
        {
            /* 3-word: word2=(sel<<16|name_ki), word3=parent_gidx */
            uint32_t word2 = func->code[offset + 1];
            uint32_t word3 = func->code[offset + 2];
            int sel_slot = (int)(word2 >> 16);
            int name_ki = (int)(word2 & 0xFFFF);
            int pgidx = (int)word3;
            const char *mname = const_str(func, name_ki);
            const char *pname = global_name_safe(vm, pgidx);
            printf("R[%d] = super.%s(%d args)", a, mname ? mname : "?", b);
            printf("  \t; parent=%s sel=%d", pname ? pname : "?", sel_slot);
            printf("\n");
            printf("   |  %04d  %-16s", offset + 1, "(super-data)");
            printf("sel=%d name_ki=%d", sel_slot, name_ki);
            if (mname) printf(" \"%s\"", mname);
            printf("\n");
            printf("   |  %04d  %-16s", offset + 2, "(parent-gidx)");
            printf("G[%d]", pgidx);
            if (pname) printf("  \t; '%s'", pname);
            return offset + 3;
        }

        /* === Classes === */
        case OP_NEWCLASS:
        {
            /* ABx: A=dest, B(hi)=name_ki (in Bx lower bits), C from original = parent reg or 255 */
            /* Actually encoded as: emit_abc(OP_NEWCLASS, class_reg, name_ki, c_operand) but
               name_ki is put via emit_abx... let me check. The encoding: Bx=name_ki in constants.
               Actually, looking at compiler: emit_abx(OP_NEWCLASS, ...) — no, it's emit_abc.
               Looking at vm_dispatch: name from K[ZEN_B(i)], parent from R[C] or 255.
               Wait — from opcodes.h it says ABx for NEWCLASS. Let me check instr_format...
               Actually in the new code I removed the format switch. The vm_dispatch uses:
               ObjString *name = as_string(K[ZEN_B(i)]) and c = ZEN_C(i).
               So it's ABC format where B indexes constants and C is parent reg or 255. */
            const char *cname = const_str(func, b);
            if (c != 255)
            {
                if (cname)
                    printf("R[%d] = class '%s'  \t; parent=R[%d]", a, cname, c);
                else
                    printf("R[%d] = class K[%d]  \t; parent=R[%d]", a, b, c);
            }
            else
            {
                if (cname)
                    printf("R[%d] = class '%s'", a, cname);
                else
                    printf("R[%d] = class K[%d]", a, b);
            }
            break;
        }
        case OP_NEWINSTANCE:
            printf("R[%d] = R[%d]()  \t; new instance", a, b);
            break;
        case OP_GETMETHOD:
        {
            const char *mn = const_str(func, c);
            if (mn)
                printf("R[%d] = R[%d].%s  \t; bound method", a, b, mn);
            else
                printf("R[%d] = R[%d].K[%d]  \t; bound method", a, b, c);
            break;
        }
        case OP_CLASSFIELD:
        {
            const char *fn = const_str(func, c);
            if (fn)
                printf("R[%d].field[%d] = '%s'", a, b, fn);
            else
                printf("R[%d].field[%d] = K[%d]", a, b, c);
            break;
        }
        case OP_CLASSFIELDDEF:
        {
            printf("R[%d].field_default[%d] = K[%d]", a, b, c);
            break;
        }

        /* === String/Misc === */
        case OP_CONCAT:
            printf("R[%d] = R[%d] + R[%d]  \t; str concat", a, b, c);
            break;
        case OP_STRADD:
            printf("R[%d] += R[%d]  \t; str append", a, b);
            break;
        case OP_TOSTRING:
            printf("R[%d] = str(R[%d])", a, b);
            break;
        case OP_TOSTRING_OBJ:
            printf("R[%d] = R[%d].__str__()", a, b);
            break;
        case OP_LEN:
            printf("R[%d] = len(R[%d])", a, b);
            break;
        case OP_PRINT:
            printf("print R[%d]  \t; %s", a, b ? "newline" : "no-nl");
            break;

        /* === Fused comparison+jump (2-word) === */
        case OP_LTJMPIFNOT:
        {
            uint32_t word2 = func->code[offset + 1];
            int jsbx = ZEN_SBX(word2);
            printf("if !(R[%d] < R[%d]): -> %04d", b, c, offset + 2 + jsbx);
            printf("\n");
            printf("   |  %04d  %-16s", offset + 1, "(jump-offset)");
            printf("sBx=%d  \t; -> %04d", jsbx, offset + 2 + jsbx);
            return offset + 2;
        }
        case OP_LEJMPIFNOT:
        {
            uint32_t word2 = func->code[offset + 1];
            int jsbx = ZEN_SBX(word2);
            printf("if !(R[%d] <= R[%d]): -> %04d", b, c, offset + 2 + jsbx);
            printf("\n");
            printf("   |  %04d  %-16s", offset + 1, "(jump-offset)");
            printf("sBx=%d  \t; -> %04d", jsbx, offset + 2 + jsbx);
            return offset + 2;
        }

        /* === For loop === */
        case OP_FORPREP:
            printf("R[%d] -= R[%d+2]; -> %04d  \t; for prep", a, a, offset + 1 + sbx);
            break;
        case OP_FORLOOP:
            printf("R[%d] += R[%d+2]; if < R[%d+1]: -> %04d", a, a, a, offset + 1 + sbx);
            break;

        /* === Fused field+arith (2-word) === */
        case OP_GETFIELD_MUL:
        {
            uint32_t word2 = func->code[offset + 1];
            int a2 = ZEN_A(word2), b2 = ZEN_B(word2), c2 = ZEN_C(word2);
            printf("R[%d] = R[%d].fields[%d]; R[%d] = R[%d] * R[%d]", a, b, c, a2, b2, c2);
            printf("\n");
            printf("   |  %04d  %-16s", offset + 1, "(MUL)");
            printf("R[%d] = R[%d] * R[%d]", a2, b2, c2);
            return offset + 2;
        }
        case OP_GETFIELD_SUB:
        {
            uint32_t word2 = func->code[offset + 1];
            int a2 = ZEN_A(word2), b2 = ZEN_B(word2), c2 = ZEN_C(word2);
            printf("R[%d] = R[%d].fields[%d]; R[%d] = R[%d] - R[%d]", a, b, c, a2, b2, c2);
            printf("\n");
            printf("   |  %04d  %-16s", offset + 1, "(SUB)");
            printf("R[%d] = R[%d] - R[%d]", a2, b2, c2);
            return offset + 2;
        }

        /* === Eval === */
        case OP_EVAL:
            printf("R[%d] = eval(R[%d])", a, b);
            break;

        /* === Assert === */
        case OP_ASSERT:
            printf("assert R[%d], R[%d]", a, b);
            break;

        /* === Halt === */
        case OP_HALT:
            printf("--- halt ---");
            break;

        /* === Import === */
        case OP_IMPORT:
        {
            const char *mn = const_str(func, bx);
            if (mn)
                printf("R[%d] = import '%s'  \t; K[%d]", a, mn, bx);
            else
                printf("R[%d] = import K[%d]", a, bx);
            break;
        }

        default:
            printf("A=%d B=%d C=%d", a, b, c);
            break;
        }

        printf("\n");
        return offset + 1;
    }

    /* --- Disassemble full function --- */
    void disassemble_func(ObjFunc *func, const char *label, VM *vm)
    {
        const char *name = label ? label : (func->name ? func->name->chars : "<script>");
        printf("== %s  (arity=%d  regs=%d  code=%d  consts=%d) ==\n",
               name, func->arity, func->num_regs, func->code_count, func->const_count);

        int offset = 0;
        while (offset < func->code_count)
            offset = disassemble_instruction(func, offset, vm);

        printf("\n");
    }

    /* --- Dump constants --- */
    void dump_constants(ObjFunc *func)
    {
        printf("  constants (%d):\n", func->const_count);
        for (int i = 0; i < func->const_count; i++)
        {
            printf("    K[%3d] = ", i);
            println_value(func->constants[i]);
        }
    }

    /* --- Dump fiber stack --- */
    void dump_stack(ObjFiber *fiber)
    {
        printf("  stack: [ ");
        for (Value *slot = fiber->stack; slot < fiber->stack_top; slot++)
        {
            print_value(*slot);
            printf(" | ");
        }
        printf("]\n");
    }

} /* namespace zen */
