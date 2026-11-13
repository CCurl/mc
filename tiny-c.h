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
#define MAX_NAMES            2048
#define MAX_STRING_LENGTH    1024
#define MAX_NODES           16384
#define MAX_FUNCTIONS        1024
#define MAX_PARAMETERS         64
#define MAX_LOCALS            256
#define MAX_CODE            65536
#define MAX_GLOBAL_SLOTS   256000
#define MAX_GLOBAL_BYTES   256000
#define ID_NAME_LENGTH         64
#define STACK_SIZE           1024
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
	CHAR_LITERAL, STRING_LITERAL, PLUS_PLUS, MINUS_MINUS        // 31 -> 34
};

enum { TYPE_INT, TYPE_BYTE, TYPE_VOID };

// ---------------------------------------------------------------------------
// For the parser and the code generator ...
enum { VAR, CST, ADD, SUB, MUL, DIV, MOD, LT, GT, EQ, NE, SET, FUNC_CALL,
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
};

struct call_fixup_s { byte *operand; int functionIndex; };
struct return_s { byte *pc; int frameBase; };

// ---------------------------------------------------------------------------
// For the code generator and the VM ...
enum { IFETCH, ISTORE, IP1, IP2, IP4, IDROP, IADD, ISUB, IMUL, IDIV,
    IMOD, ILT, IGT, IEQ, INE, JZ, JNZ, JMP, ICALL, IRET, HALT, IAFETCH, IASTORE,
    PLOAD, PSTORE, PAFETCH, PASTORE, LLOAD, LSTORE, SYS, I2B,
    BGET, BSET, BAFETCH, BASTORE, PBAFETCH, PBASTORE, LINC, LDEC, GINC, GDEC
};

#define SYS_EMIT      0

// ---------------------------------------------------------------------------
// For the VM ...
#define ACASE    goto again; case
#define TOS      st[sp]
#define NOS      st[sp-1]

#endif // _TINY_C_H_
