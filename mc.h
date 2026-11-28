// mc.h

#ifndef _MC_H_
#define _MC_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ---------------------------------------------------------------------------
#define INT_SIZE                4  // bytes in an int on the target
#define ID_NAME_LENGTH         24
#define MAX_CODE           131072 // 128 KB
#define MAX_NODES           16384 
#define MAX_FUNCTIONS        1024
#define MAX_GLOBAL_NAMES     1024
#define MAX_GLOBAL_BYTES  4194304 // 4 MB
#define MAX_STRING_LENGTH    1024
#define MAX_PATH_LENGTH        64
#define MAX_INCLUDE_DEPTH      16
#define MAX_INCLUDED_FILES    128
#define MAX_CALL_FIXUPS MAX_NODES
#define MAX_LOCALS             32
#define MAX_PARAMETERS         32
#define STACK_BYTES        65536 // Size of the stack, which grows down from the top of memory

// One memory image holds the globals, then the code, then the stack. Addresses start at LOAD_BASE,
// the usual base of a 32-bit Linux executable, and MEM_AT() maps an address to its byte in mem[].
#define LOAD_BASE       0x08048000
#define CODE_BASE       (LOAD_BASE + MAX_GLOBAL_BYTES)   // the code starts after the global data
#define CODE_TOP        (CODE_BASE + MAX_CODE)           // the stack may not grow below this
#define STACK_TOP       (CODE_TOP + STACK_BYTES)         // esp starts here
#define MEM_BYTES       (STACK_TOP - LOAD_BASE)          // size of mem[]
#define MEM_AT(addr)    (mem + ((addr) - LOAD_BASE))
#define ELF_HEADER_BYTES 0x80                            // the ELF and program headers come first, then the globals

/* Strings are char arrays: the characters, then a 0. */
#define STRING_OVERHEAD         1

// ---------------------------------------------------------------------------
#define BTWI(n,l,h) ((l<=n)&&(n<=h))

// ---------------------------------------------------------------------------
// For the lexer and the parser ...
enum { DO_SYM, ELSE_SYM, IF_SYM, WHILE_SYM, VOID_SYM, INT_SYM,  //  0 ->  5
    CHAR_SYM, RETURN_SYM, CONST_SYM,                            //  6 ->  8
    LBRA, RBRA, LPAR, RPAR, PLUS,                               //  9 -> 13
    MINUS, STAR, SLASH, MODULO,                                 // 14 -> 17
    LESS, GRT, SEMI, EQUAL, EQ_SYM, NE_SYM,                     // 18 -> 23
    INT, ID, EOI, LBRACKET, RBRACKET, COMMA,                    // 24 -> 29
	CHAR_LITERAL, STRING_LITERAL, PLUS_PLUS, MINUS_MINUS,       // 30 -> 33
    LESS_EQ, GRT_EQ, AMP, PIPE, CARET, TILDE,                   // 34 -> 39
    LSHIFT, RSHIFT, AND_AND, OR_OR                              // 40 -> 43
};

enum { TYPE_INT, TYPE_BYTE, TYPE_CHARPTR, TYPE_INTPTR };

// ---------------------------------------------------------------------------
// For the parser and the code generator ...
enum { VAR, CST, ADD, SUB, MUL, DIV, MOD, LT, GT, LE, GE, EQ, NE, SET, FUNC_CALL,
    ARRAY_GET, ARRAY_SET, ARRAY_REF, LOCAL, RETURN_NODE, ARG_LIST,
    IF1, IF2, WHILE, DO, EMPTY, SEQ, EXPR, PROG, BYTE_CONVERT, PRE_INC, POST_INC,
    BIT_AND, BIT_OR, BIT_XOR, BIT_NOT, BIT_SHL, BIT_SHR, LOG_AND, LOG_OR
};

#define BUILTIN_SYSCALL -1

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
    int returnType;
    int numParameters;
    char parameterNames[MAX_PARAMETERS][ID_NAME_LENGTH];
    int parameterIsArray[MAX_PARAMETERS];
    int parameterTypes[MAX_PARAMETERS];
    int numLocals;
    char localNames[MAX_LOCALS][ID_NAME_LENGTH];
    int localTypes[MAX_LOCALS];
    int localIsArray[MAX_LOCALS];  // an array alias, whose frame slot holds the address of the array it names
    int localInScope[MAX_LOCALS];  // cleared when the block that declared the local ends
};

struct call_fixup_s { int operand; int functionIndex; };

// ---------------------------------------------------------------------------
// The x86 subset. The code generator emits these encodings and the VM decodes them. They are 32-bit x86 in
// Intel encoding, so the bytes can later go into an executable unchanged. Register numbers are the x86 encodings.
enum { EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI };

// Condition codes: the low nibble of Jcc (0x80+cc) and SETcc (0x90+cc).
enum { CC_E = 4, CC_NE = 5, CC_L = 0xC, CC_GE = 0xD, CC_LE = 0xE, CC_G = 0xF };

// One-byte opcodes. Those with a ModRM byte give the operands as "r/m, reg" or "reg, r/m".
// Base opcodes that take a register in their low bits are listed with that register added.
enum {
    X_ADD_RM_R = 0x01,      // add r/m, reg
    X_OR_RM_R = 0x09,       // or r/m, reg
    X_AND_RM_R = 0x21,      // and r/m, reg
    X_SUB_RM_R = 0x29,      // sub r/m, reg
    X_XOR_RM_R = 0x31,      // xor r/m, reg
    X_CMP_RM_R = 0x39,      // cmp r/m, reg
    X_TEST_RM_R = 0x85,     // test r/m, reg
    X_MOV_RM8_R8 = 0x88,    // mov r/m8, reg8
    X_MOV_RM_R = 0x89,      // mov r/m, reg
    X_MOV_R_RM = 0x8B,      // mov reg, r/m
    X_CDQ = 0x99,           // edx = sign of eax
    X_GRP1_RM_IMM32 = 0x81, // ModRM reg field /0 = add, /5 = sub; imm32 follows
    X_GRP2_RM_IMM8 = 0xC1,  // ModRM reg field /4 = shl; imm8 follows
    X_GRP2_RM_CL = 0xD3,    // ModRM reg field /4 = shl, /7 = sar; count in cl
    X_GRP3_RM = 0xF7,       // ModRM reg field /2 = not r/m, /7 = idiv edx:eax by r/m
    X_GRP4_RM8 = 0xFE,      // ModRM reg field /0 = inc r/m8, /1 = dec r/m8
    X_GRP5_RM = 0xFF,       // ModRM reg field /0 = inc r/m32, /1 = dec r/m32
    X_RET = 0xC3,
    X_LEAVE = 0xC9,
    X_INT = 0xCD,           // imm8 follows
    X_CALL = 0xE8,          // rel32 follows
    X_JMP = 0xE9,           // rel32 follows
    X_PUSH = 0x50,          // + register
    X_PUSH_IMM32 = 0x68,    // imm32 follows
    X_POP = 0x58,           // + register
    X_MOV_R_IMM = 0xB8,     // + register; imm32 follows
    X_ESCAPE = 0x0F,        // the next byte is a two-byte opcode
    // Two-byte opcodes, after X_ESCAPE:
    X_JCC = 0x80,           // + condition code; rel32 follows
    X_SETCC = 0x90,         // + condition code; sets r/m8
    X_IMUL_R_RM = 0xAF,     // imul reg, r/m
    X_MOVZX_R_RM8 = 0xB6    // movzx reg, r/m8
};


// ---------------------------------------------------------------------------
// Declarations shared between files. Each global and function is defined in exactly one .c file.
extern int sym, intVal;
extern char idName[ID_NAME_LENGTH];
extern int variableTypes[MAX_GLOBAL_NAMES], variableBases[MAX_GLOBAL_NAMES], variableIsArray[MAX_GLOBAL_NAMES];
extern int stringValues[MAX_STRING_LENGTH];
extern int stringLength;
extern byte mem[MEM_BYTES];
extern int numGlobalBytes, numZeroedBytes;
extern int here;
extern struct function_s functions[MAX_FUNCTIONS];
extern int numFunctions, mainFunction;

int openSource(const char *path);
void qFail(int condition, const char *fmt, ...);
void syntaxError(void);
void nextSym(void);
int loadInt(int addr);
void storeInt(int addr, int value);
int findVariable(const char *name);
int declareVariable(const char *name, int size, int isArray, int type, int zeroed);
int variableIndex(const char *name);
int arrayIndex(const char *name);
int allocateStringLiteral(void);
void storeStringBytes(int base);
int parameterIndex(const char *name);
int localIndex(const char *name);
int currentParameterIsArray(int parameter);
node *expr(void);
node *parenExpr(void);
node *program(void);
void c(node *x);
int run(void);
int saveExecutable(const char *fileName);
void listing(void);

#endif // _MC_H_
