#ifndef ZEN_OPCODES_H
#define ZEN_OPCODES_H

/*
** opcodes.h — Todas as instruções da VM.
**
** Formato: 32 bits  [ opcode(8) | A(8) | B(8) | C(8) ]
** ou:      32 bits  [ opcode(8) | A(8) | Bx(16) ]       para constantes
** ou:      32 bits  [ opcode(8) | A(8) | sBx(16) ]      para jumps
**
** A  = registo destino
** B  = registo fonte ou índice curto
** C  = registo fonte 2
** Bx = índice 16-bit (constantes, globais)
** sBx = signed 16-bit offset (jumps)
*/

namespace zen
{

    enum OpCode : uint8_t
    {
        /* --- Load/Store --- */
        OP_LOADNIL,  /* R[A] = nil                              */
        OP_LOADBOOL, /* R[A] = (bool)B; se C: pc++              */
        OP_LOADK,    /* R[A] = constants[Bx]                    */
        OP_LOADI,    /* R[A] = (int)sBx                         */
        OP_MOVE,     /* R[A] = R[B]                             */

        /* --- Globals (indexados) --- */
        OP_GETGLOBAL, /* R[A] = globals[Bx]                      */
        OP_SETGLOBAL, /* globals[Bx] = R[A]                      */

        /* --- Aritmética --- */
        OP_ADD, /* R[A] = R[B] + R[C]                      */
        OP_SUB, /* R[A] = R[B] - R[C]                      */
        OP_MUL, /* R[A] = R[B] * R[C]                      */
        OP_DIV, /* R[A] = R[B] / R[C]                      */
        OP_IDIV, /* R[A] = R[B] // R[C]  (floor division)   */
        OP_MOD, /* R[A] = R[B] % R[C]                      */
        OP_POW, /* R[A] = R[B] ** R[C]                     */
        OP_NEG, /* R[A] = -R[B]                            */

        /* --- Object operator overloads (compiler-selected cold path) --- */
        OP_ADD_OBJ, /* R[A] = R[B].__add__(R[C])              */
        OP_SUB_OBJ, /* R[A] = R[B].__sub__(R[C])              */
        OP_MUL_OBJ, /* R[A] = R[B].__mul__(R[C])              */
        OP_DIV_OBJ, /* R[A] = R[B].__div__(R[C])              */
        OP_MOD_OBJ, /* R[A] = R[B].__mod__(R[C])              */
        OP_NEG_OBJ, /* R[A] = R[B].__neg__()                  */
        OP_EQ_OBJ,  /* R[A] = R[B].__eq__(R[C])               */
        OP_LT_OBJ,  /* R[A] = R[B].__lt__(R[C])               */
        OP_LE_OBJ,  /* R[A] = R[B].__le__(R[C])               */

        /* --- Aritmética imediata (superinstruções) --- */
        OP_ADDI, /* R[A] = R[B] + (signed)C                 */
        OP_SUBI, /* R[A] = R[B] - (signed)C                 */

        /* --- Bitwise --- */
        OP_BAND, /* R[A] = R[B] & R[C]                      */
        OP_BOR,  /* R[A] = R[B] | R[C]                      */
        OP_BXOR, /* R[A] = R[B] ^ R[C]                      */
        OP_BNOT, /* R[A] = ~R[B]                            */
        OP_SHL,  /* R[A] = R[B] << R[C]                     */
        OP_SHR,  /* R[A] = R[B] >> R[C]                     */

        /* --- Comparação (result em R[A]) --- */
        OP_EQ,  /* R[A] = (R[B] == R[C])                   */
        OP_LT,  /* R[A] = (R[B] <  R[C])                   */
        OP_LE,  /* R[A] = (R[B] <= R[C])                   */
        OP_NOT, /* R[A] = !R[B]                            */
        OP_CONTAINS, /* R[A] = (R[B] in R[C])                  */
        OP_IS,  /* R[A] = (R[B] is R[C])  (identity check)    */

        /* --- Jumps --- */
        OP_JMP,      /* pc += sBx                               */
        OP_JMPIF,    /* se truthy(R[A]): pc += sBx              */
        OP_JMPIFNOT, /* se !truthy(R[A]): pc += sBx             */

        /* --- Funções --- */
        OP_CALL,       /* R[A](R[A+1]..R[A+B]) → R[A]..R[A+C-1]  */
        OP_CALLGLOBAL, /* globals[Bx(word2)](R[A+1]..+B) → R[A]..+C — 2 words */
        OP_RETURN,     /* return R[A]..R[A+B-1] (B=nresults)       */

        /* --- Closures / Upvalues --- */
        OP_CLOSURE,  /* R[A] = closure(constants[Bx])            */
        OP_GETUPVAL, /* R[A] = upvalues[B]                       */
        OP_SETUPVAL, /* upvalues[B] = R[A]                       */
        OP_CLOSE,    /* close upvalues >= R[A] (ao sair de scope)*/

        /* --- Fibers --- */
        OP_NEWFIBER, /* R[A] = Fiber.new(R[B])  (B=closure)     */
        OP_RESUME,   /* R[A] = R[B].resume(R[C])                 */
        OP_YIELD,    /* R[A] = yield R[B] (yield B, receive into A) */
        OP_AWAIT,    /* R[A] = await R[B] (resume B until done)  */

        /* --- Iteration --- */
        OP_FOR_ITER, /* R[A] = next(R[B]); if done pc += sBx (2-word) */

        /* --- Objects --- */
        OP_NEWARRAY,  /* R[A] = []                               */
        OP_NEWMAP,    /* R[A] = {}                               */
        OP_NEWSET,    /* R[A] = set()                            */
        OP_NEWBUFFER, /* R[A] = TypedArray(R[B]); C=BufferType   */
        OP_APPEND,    /* R[A].push(R[B])                         */
        OP_SETADD,    /* R[A].add(R[B])  (set add)               */
        OP_GETFIELD, /* R[A] = R[B].constants[C]  (field name)  */
        OP_SETFIELD, /* R[A].constants[B] = R[C]                */
        OP_GETFIELD_IDX, /* R[A] = R[B].fields[C]  (direct index, O(1)) */
        OP_SETFIELD_IDX, /* R[A].fields[B] = R[C]  (direct index, O(1)) */
        OP_GETINDEX,  /* R[A] = R[B][R[C]]                       */
        OP_SETINDEX,  /* R[A][R[B]] = R[C]                       */
        OP_DELINDEX,  /* del R[A][R[B]]                           */
        OP_GETSLICE,  /* R[A] = R[B][R[C]:R[C+1]:R[C+2]]        */
        OP_INVOKE,   /* R[A].method(R[A+1]..R[A+B]) → R[A]; C=name_idx (2-word: word2=name_ki) */
        OP_INVOKE_VT, /* R[A].vtable[C](R[A+1]..R[A+B]) → R[A]; single-word, O(1) */
        OP_SUPER_INVOKE, /* like OP_INVOKE but dispatches on parent class vtable (2-word) */

        /* --- Classes --- */
        OP_NEWCLASS,    /* R[A] = new class (nome em constants[B]) */
        OP_NEWINSTANCE, /* R[A] = klass.new()                      */
        OP_GETMETHOD,   /* R[A] = R[B]:method(constants[C])        */
        OP_CLASSFIELD,  /* as_class(R[A]).field_names[B] = K[C]    */

        /* --- Misc --- */
        OP_CONCAT,   /* R[A] = R[B] .. R[C]  (string concat)   */
        OP_STRADD,   /* R[A] = R[A] + R[B]  (in-place append)  */
        OP_TOSTRING, /* R[A] = tostring(R[B])                   */
        OP_TOSTRING_OBJ, /* R[A] = R[B].__str__()                */
        OP_LEN,      /* R[A] = #R[B]                            */
        OP_PRINT,    /* print R[A]; B=1 → newline               */

        /* --- Fused comparison + jump (2-word) --- */
        OP_LTJMPIFNOT, /* if !(R[B] < R[C]): pc += sBx(next_word)  */
        OP_LEJMPIFNOT, /* if !(R[B] <= R[C]): pc += sBx(next_word) */

        /* --- Numeric for loop (superinstruction) --- */
        OP_FORPREP, /* R[A]-=R[A+2]; if R[A]>=R[A+1]: pc+=sBx (skip) */
        OP_FORLOOP, /* R[A]+=R[A+2]; if R[A]<R[A+1]: pc+=sBx (loop)  */

        /* --- Fused field+arith (2-word superinstructions) --- */
        OP_GETFIELD_MUL, /* word1: R[A]=R[B].fields[C]; word2: R[A]=R[B]*R[C] */
        OP_GETFIELD_SUB, /* word1: R[A]=R[B].fields[C]; word2: R[A]=R[B]-R[C] */

        /* --- Iteration (foreach) --- */
        OP_ITER_ELEM, /* R[A] = iter_elem(R[B], R[C]) — ordinal access for foreach */

        /* --- Eval --- */
        OP_EVAL, /* compile+run string R[B]; result → R[A]  */

        /* --- Assert --- */
        OP_ASSERT, /* if !truthy(R[A]): AssertionError(R[B])   */

        OP_HALT,

        /* --- Modules --- */
        OP_IMPORT,     /* R[A] = import(constants[Bx]); module as ObjMap */

        /* as_class(R[A]).field_defaults[B] = K[C] — the value a class body
        ** assignment gave a field, copied into every new instance. Added
        ** last on purpose: the opcode numbering is part of the bytecode
        ** format, so existing ones must keep their values. */
        OP_CLASSFIELDDEF,
    };

/* Encode/Decode — ABC format */
#define ZEN_ENCODE(op, a, b, c) ((uint32_t)((op) << 24) | ((a) << 16) | ((b) << 8) | (c))
#define ZEN_OP(i) ((uint8_t)(((i) >> 24) & 0xFF))
#define ZEN_A(i) ((uint8_t)(((i) >> 16) & 0xFF))
#define ZEN_B(i) ((uint8_t)(((i) >> 8) & 0xFF))
#define ZEN_C(i) ((uint8_t)((i) & 0xFF))

/* Encode/Decode — ABx format (16-bit unsigned operand) */
#define ZEN_ENCODE_BX(op, a, bx) ((uint32_t)((op) << 24) | ((a) << 16) | ((bx) & 0xFFFF))
#define ZEN_BX(i) ((uint16_t)((i) & 0xFFFF))

/* Encode/Decode — AsBx format (16-bit signed offset) */
#define ZEN_ENCODE_SBX(op, a, sbx) ((uint32_t)((op) << 24) | ((a) << 16) | (((sbx) + 32768) & 0xFFFF))
#define ZEN_SBX(i) ((int)((int)((i) & 0xFFFF) - 32768))

} /* namespace zen */

#endif /* ZEN_OPCODES_H */
