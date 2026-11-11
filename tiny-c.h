// tiny-c.h

#ifndef _TINY_C_H_
#define _TINY_C_H_

// ---------------------------------------------------------------------------
#define MAX_PATH_LENGTH        64
#define MAX_INCLUDE_DEPTH      16
#define MAX_INCLUDED_FILES    128
#define MAX_NAMES            1024
#define MAX_STRING_LENGTH    1024
#define MAX_NODES            4096
#define MAX_FUNCTIONS         256
#define MAX_PARAMETERS         64
#define MAX_LOCALS            256
#define MAX_CODE            65536
#define MAX_GLOBAL_SLOTS   256000
#define MAX_CALL_FIXUPS MAX_NODES

// ---------------------------------------------------------------------------
#define BTWI(n,l,h) ((l<=n)&&(n<=h))

// ---------------------------------------------------------------------------
// For the lexer ...
enum { DO_SYM, ELSE_SYM, IF_SYM, WHILE_SYM, VOID_SYM, INT_SYM, CHAR_SYM, // 0 -> 6
        LBRA, RBRA, LPAR, RPAR, PLUS,                                  // 7 -> 11
        MINUS, STAR, SLASH, MODULO,                             // 12 -> 15
        LESS, GRT, SEMI, EQUAL, EQ_SYM, NE_SYM,                 // 16 -> 21
        INT, ID, EOI, LBRACKET, RBRACKET, COMMA, RETURN_SYM,    // 22 -> 28
	CHAR_LITERAL, STRING_LITERAL                            // 29 -> 30
};
enum { TYPE_INT, TYPE_CHAR, TYPE_VOID };
// ---------------------------------------------------------------------------
// For the parser ...
enum { VAR, CST, ADD, SUB, MUL, DIV, MOD, LT, GT, EQ, NE, SET, FUNC_CALL,
       ARRAY_GET, ARRAY_SET, ARRAY_REF, LOCAL, RETURN_NODE, VOID_RETURN, ARG_LIST,
       IF1, IF2, WHILE, DO, EMPTY, SEQ, EXPR, PROG, CHAR_CONVERT };

struct node_s {
    int kind;
    struct node_s *o1, *o2, *o3;
    int val;
    int type;
    int is_string;
};
typedef struct node_s node;

// ---------------------------------------------------------------------------
// For the code generator ...
enum { IFETCH, ISTORE, IP1, IP2, IP4, IDROP, IADD, ISUB, IMUL, IDIV,
        IMOD, ILT, IGT, IEQ, INE, JZ, JNZ, JMP, ICALL, IRET, HALT, IAFETCH, IASTORE,
        PLOAD, PSTORE, PAFETCH, PASTORE, LLOAD, LSTORE, SYS, IC2C };

enum { BUILTIN_EMIT };
enum { SYS_EMIT };

// ---------------------------------------------------------------------------
// For the VM ...
#define ACASE    goto again; case
#define TOS      st[sp]
#define NOS      st[sp-1]

#endif // _TINY_C_H_
