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
    int declared;  // set by a prototype or a definition
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
// For the code generator and the VM. Each opcode is listed once in OPCODES, with the size of its operands.
// Operands follow the opcode in the order shown in the comments.
// "r" is a 2-byte register in the current frame, "a" a 4-byte address or immediate,
// and "t" a 4-byte jump offset counted from the offset field itself.
// Array indexes are element indexes held in registers; the VM scales int elements by 4.
#define OPCODES(X) \
    X(LDI,    "ra")                      /* r, a: r = a */ \
    X(MOV,    "rr")                      /* r, r: r = r */ \
    X(IGET,   "ra")                      /* r, a: r = int global at a */ \
    X(ISET,   "ra")                      /* r, a: int global at a = r */ \
    X(BGET,   "ra")                      /* r, a: r = byte global at a */ \
    X(BSET,   "ra")                      /* r, a: byte global at a = r */ \
    X(IAGET,  "rar")                     /* r, a, r: r = int global at a + index*4 */ \
    X(IASET,  "rar")                     /* r, a, r: int global at a + index*4 = r */ \
    X(BAGET,  "rar")                     /* r, a, r: r = byte global at a + index */ \
    X(BASET,  "rar")                     /* r, a, r: byte global at a + index = r */ \
    X(PIAGET, "rrr")                     /* r, r, r: r = int at base + index*4 (base is a register holding an address) */ \
    X(PIASET, "rrr")                     /* r, r, r: int at base + index*4 = r */ \
    X(PBAGET, "rrr")                     /* r, r, r: r = byte at base + index */ \
    X(PBASET, "rrr")                     /* r, r, r: byte at base + index = r */ \
    X(I2B,    "rr")                      /* r, r: r = (byte) r */ \
    X(IADDI,  "rra")                     /* r, r, a: r = r + a */ \
    X(IINC,   "raa")                     /* r, a, a: int global at a += delta; r = the new value */ \
    X(IINCP,  "raa")                     /* r, a, a: as IINC, but r = the old value */ \
    X(BINC,   "raa")                     /* r, a, a: byte global at a += delta; r = the new value */ \
    X(BINCP,  "raa")                     /* r, a, a: as BINC, but r = the old value */ \
    X(IADD,   "rrr")                     /* r, r, r: r = r op r for the operators below */ \
    X(ISUB,   "rrr") \
    X(IMUL,   "rrr") \
    X(IDIV,   "rrr") \
    X(IMOD,   "rrr") \
    X(ILT,    "rrr") \
    X(IGT,    "rrr") \
    X(IEQ,    "rrr") \
    X(INE,    "rrr") \
    X(ILE,    "rrr") \
    X(IGE,    "rrr") \
    X(EMIT,   "rr")                      /* r, r: r = putchar(r) */ \
    X(JMP,    "t")                       /* t: pc += t */ \
    X(JZ,     "rt")                      /* r, t: if r is zero pc += t */ \
    X(JNZ,    "rt")                      /* r, t: if r is non-zero pc += t */ \
    X(JLT,    "rrt")                     /* r, r, t: if the comparison of the two registers holds, pc += t */ \
    X(JGT,    "rrt") \
    X(JEQ,    "rrt") \
    X(JNE,    "rrt") \
    X(JGE,    "rrt") \
    X(JLE,    "rrt") \
    X(CALL,   "ara")                     /* a, r, a: call the function at a with its frame starting at register r of the caller; the frame size is the final 4-byte operand. On return the callee's result is stored in the caller's register r. */ \
    X(RET,    "r")                       /* r: return the value of register r */ \
    X(RET0,   "")                        /* return 0 (void returns and falling off the end of a function) */ \
    X(HALT,   "")

#define OPCODE_ENUM(name, kinds) name,
enum { OPCODES(OPCODE_ENUM) NUM_OPCODES };

// ---------------------------------------------------------------------------
// Declarations shared between files. Each global and function is defined in exactly one .c file.
extern int sym, intVal;
extern char idName[ID_NAME_LENGTH];
extern int variableTypes[MAX_GLOBAL_NAMES], variableBases[MAX_GLOBAL_NAMES], variableIsArray[MAX_GLOBAL_NAMES];
extern int stringLength;
extern byte globalMem[MAX_GLOBAL_BYTES];
extern int numGlobalBytes;
extern int here;
extern byte vm[MAX_CODE];
extern struct function_s functions[MAX_FUNCTIONS];
extern int numFunctions, mainFunction;

void fail(const char *fmt, ...);
void syntaxError(void);
void nextSym(void);
void openSource(const char *path);
int loadInt(int addr);
void storeInt(int addr, int value);
int findVariable(const char *name);
int declareVariable(const char *name, int size, int isArray, int type);
int variableIndex(const char *name);
int arrayIndex(const char *name);
int arrayReference(const char *name);
int allocateStringLiteral(void);
void storeStringBytes(int base);
int fold(node *x, int *out);
int parameterIndex(const char *name);
int localIndex(const char *name);
int currentParameterIsArray(int parameter);
node *expr(void);
node *parenExpr(void);
node *program(void);
void c(node *x);
int run(void);
int saveImage(const char *fileName);
extern const char *const opcodeNames[NUM_OPCODES];
extern const char *const opcodeKinds[NUM_OPCODES];
int opcodeLength(int op);
void listing(void);

// ---------------------------------------------------------------------------
// For the VM ...
#define ACASE    goto again; case

#endif // _TINY_C_H_
