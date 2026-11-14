// tiny-c.h

#ifndef _TINY_C_H_
#define _TINY_C_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ---------------------------------------------------------------------------
#define MAX_PATH_LENGTH        64
#define MAX_INCLUDE_DEPTH      16
#define MAX_INCLUDED_FILES    128
#define MAX_GLOBAL_NAMES     2048
#define MAX_STRING_LENGTH    1024
#define MAX_NODES           16384 
#define MAX_FUNCTIONS        1024
#define MAX_PARAMETERS         64
#define MAX_LOCALS            256
#define MAX_CODE           131072 // 128 KB
#define MAX_GLOBAL_BYTES  4194304 // 4 MB
#define ID_NAME_LENGTH         64
#define STACK_SIZE           4096
#define MAX_REGISTERS       65535 // register operands are 2 bytes
#define MAX_CALL_FIXUPS MAX_NODES

/* Byte arrays hold a 2-byte little-endian length, the characters, then a 0. */
#define STRING_OVERHEAD         3

// ---------------------------------------------------------------------------
#define BTWI(n,l,h) ((l<=n)&&(n<=h))

// ---------------------------------------------------------------------------
// For the lexer and the parser ...
enum { DO_SYM, ELSE_SYM, IF_SYM, WHILE_SYM, VOID_SYM, INT_SYM,  //  0 ->  5
    BYTE_SYM, EMIT_SYM, RETURN_SYM, INCLUDE_SYM,                //  6 ->  9
    LBRA, RBRA, LPAR, RPAR, PLUS,                               // 10 -> 14
    MINUS, STAR, SLASH, MODULO,                                 // 15 -> 18
    LESS, GRT, SEMI, EQUAL, EQ_SYM, NE_SYM,                     // 19 -> 24
    INT, ID, EOI, LBRACKET, RBRACKET, COMMA,                    // 25 -> 30
	CHAR_LITERAL, STRING_LITERAL, PLUS_PLUS, MINUS_MINUS,       // 31 -> 34
    LESS_EQ, GRT_EQ                                             // 35 -> 36
};

enum { TYPE_INT, TYPE_BYTE, TYPE_VOID };

// ---------------------------------------------------------------------------
// For the parser and the code generator ...
enum { VAR, CST, ADD, SUB, MUL, DIV, MOD, LT, GT, LE, GE, EQ, NE, SET, FUNC_CALL,
    ARRAY_GET, ARRAY_SET, ARRAY_REF, LOCAL, RETURN_NODE, VOID_RETURN, ARG_LIST,
    IF1, IF2, WHILE, DO, EMPTY, SEQ, EXPR, PROG, BYTE_CONVERT, PRE_INC, POST_INC
};

#define BUILTIN_EMIT -1

// ---------------------------------------------------------------------------
// Shared types and structs ...
typedef unsigned char byte;

struct node_s {
    int kind;
    struct node_s *o1, *o2, *o3;
    int val;
    int type;
};
typedef struct node_s node;

struct function_s {
    char name[ID_NAME_LENGTH];
    node *body;
    int address;
    int defined;
    int returnsValue;
    int returnType;
    int numParameters;
    char parameterNames[MAX_PARAMETERS][ID_NAME_LENGTH];
    int parameterIsArray[MAX_PARAMETERS];
    int parameterTypes[MAX_PARAMETERS];
    int numLocals;
    char localNames[MAX_LOCALS][ID_NAME_LENGTH];
    int localTypes[MAX_LOCALS];
    int frameSize;  // registers: parameters, then locals, then temporaries
};

struct call_fixup_s { int operand; int functionIndex; };
struct return_s { byte *pc; int frameBase; };

// ---------------------------------------------------------------------------
// For the code generator and the VM. Operands are laid out in the order shown.
// "r" is a 2-byte register in the current frame, "a" a 4-byte address or immediate,
// and "t" a 4-byte jump offset counted from the offset field itself.
// Array indexes are element indexes held in registers; the VM scales int elements by 4.
enum { LDI,     // r, a: r = a
    MOV,        // r, r: r = r
    IGET,       // r, a: r = int global at a
    ISET,       // r, a: int global at a = r
    BGET,       // r, a: r = byte global at a
    BSET,       // r, a: byte global at a = r
    IAGET,      // r, a, r: r = int global at a + index*4
    IASET,      // r, a, r: int global at a + index*4 = r
    BAGET,      // r, a, r: r = byte global at a + index
    BASET,      // r, a, r: byte global at a + index = r
    PIAGET,     // r, r, r: r = int at base + index*4 (base is a register holding an address)
    PIASET,     // r, r, r: int at base + index*4 = r
    PBAGET,     // r, r, r: r = byte at base + index
    PBASET,     // r, r, r: byte at base + index = r
    I2B,        // r, r: r = (byte) r
    IADDI,      // r, r, a: r = r + a
    IADD, ISUB, IMUL, IDIV, IMOD, ILT, IGT, IEQ, INE, ILE, IGE,  // r, r, r: r = r op r
    EMIT,       // r, r: r = putchar(r)
    JMP,        // t: pc += t
    JZ, JNZ,    // r, t: if r is zero (non-zero) pc += t
    JLT, JGT, JEQ, JNE, JGE, JLE,  // r, r, t: if the comparison of the two registers holds, pc += t
    CALL,       // a, r, a: call the function at a with its frame starting at register r of
                //          the caller; the frame size is the final 4-byte operand. On return
                //          the callee's result is stored in the caller's register r.
    RET,        // r: return the value of register r
    RET0,       // return 0 (void returns and falling off the end of a function)
    HALT
};

// ---------------------------------------------------------------------------
// For the VM ...
#define ACASE    goto again; case

#endif // _TINY_C_H_
