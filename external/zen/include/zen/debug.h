#ifndef ZEN_DEBUG_H
#define ZEN_DEBUG_H

#include "object.h"
#include "opcodes.h"

namespace zen
{

    class VM; /* forward */

    /*
    ** debug.h — Disassembler e ferramentas de diagnóstico.
    **
    ** Usado para:
    **   1. Verificar bytecode gerado pelo Emitter/Compiler
    **   2. Debug runtime (single-step, breakpoints)
    **   3. Dump de valores e objectos
    **
    ** Activar/desactivar com:
    **   -DZEN_DEBUG_TRACE_EXEC     → imprime cada instrução executada
    **   -DZEN_DEBUG_TRACE_GC       → imprime GC mark/sweep
    **   -DZEN_DEBUG_STRESS_GC      → força GC a cada alocação (testa correctness)
    */

    /* --- Disassemble uma função inteira (with VM context for names) --- */
    void disassemble_func(ObjFunc *func, const char *label = nullptr, VM *vm = nullptr);

    /* --- Disassemble uma instrução (retorna offset da próxima) --- */
    int disassemble_instruction(ObjFunc *func, int offset, VM *vm = nullptr);

    /* --- Dump de um valor (compact, one-line) --- */
    void print_value(Value val);
    void println_value(Value val);

    /* --- Dump do stack de um fiber --- */
    void dump_stack(ObjFiber *fiber);

    /* --- Dump da constant pool --- */
    void dump_constants(ObjFunc *func);

    /* --- Nomes dos opcodes (para printing) --- */
    const char *opcode_name(OpCode op);

} /* namespace zen */

#endif /* ZEN_DEBUG_H */
