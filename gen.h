//--------------------------------------------------------------------------
// Code generator for tiny-c

#include "tiny-c.h"

typedef char code;
typedef unsigned char byte;
code vm[MAX_CODE], *here = vm;

struct call_fixup_s { code *operand; int function_index; };
struct call_fixup_s call_fixups[MAX_CALL_FIXUPS];
int num_call_fixups;

void checkSize(int sz) { if (here + sz > vm + MAX_CODE) { error("-program too large-"); } }
void g(code c)   { checkSize(1); *here++ = c; }
void g2(short c) { checkSize(2); *(short *)here = c; here += 2; }
void g4(int c)   { checkSize(4); *(int *)here = c; here += 4; }

code *hole() {
    checkSize(2);
    code *p = here;
    here += 2;
    return p;
}
void fix(code *src, code *dst) {
    int off = (int)(dst - src);
    if (off < -32768 || off > 32767) { error("-jump too far-"); }
    *(short *)src = (short)off;
}

void emit_call(int index, int argc) {
    if (num_call_fixups >= MAX_CALL_FIXUPS) { error("-too many function calls-"); }
    g(ICALL);
    call_fixups[num_call_fixups].operand = here;
    call_fixups[num_call_fixups].function_index = index;
    num_call_fixups++;
    g2(0);
    g2(argc);
    g2(functions[index].num_locals);
}

void patch_calls() {
    for (int i = 0; i < num_call_fixups; i++) {
        unsigned int address = (unsigned int)functions[call_fixups[i].function_index].address;
        call_fixups[i].operand[0] = (code)(address & 0xff);
        call_fixups[i].operand[1] = (code)((address >> 8) & 0xff);
    }
}

void emit_const(int v) {
    if (BTWI(v,0,127)) { g(IP1); g(v); }
    else if (BTWI(v,128,32767)) { g(IP2); g2(v); }
    else { g(IP4); g4(v); }
}

/* Returns 1 and sets *out if x is a constant expression that can be folded. */
int fold(node *x, int *out) {
    int a, b;
    if (!x) { return 0; }
    if (x->kind == CST) { *out = x->val; return 1; }
    if (x->kind == BYTE_CONVERT) {
        if (!fold(x->o1, &a)) { return 0; }
        *out = (unsigned char)a;
        return 1;
    }
    if (!fold(x->o1, &a) || !fold(x->o2, &b)) { return 0; }
    switch (x->kind) {
        case ADD: *out = a + b; return 1;
        case SUB: *out = a - b; return 1;
        case MUL: *out = a * b; return 1;
        case DIV: if (b == 0) { return 0; } *out = a / b; return 1;
        case MOD: if (b == 0) { return 0; } *out = a % b; return 1;
        case LT:  *out = a < b;  return 1;
        case GT:  *out = a > b;  return 1;
        case EQ:  *out = a == b; return 1;
        case NE:  *out = a != b; return 1;
        default: return 0;
    }
}

void c(node *x) {
    code *p1, *p2;
    int folded;
    if (!x) { return; }
    if (x->kind != CST && fold(x, &folded)) { emit_const(folded); return; }
    switch (x->kind) {
        case VAR:
            if (x->val < 0) { g(PLOAD); g2(-x->val - 1); }
            else if (x->type == TYPE_BYTE) { g(BGET); g2(x->val); }
            else { g(IFETCH); g2(x->val); }
            break;
        case LOCAL: g(LLOAD); g2(x->val); break;
        case ARRAY_REF:
            if (x->val < 0) { g(PLOAD); g2(-x->val - 1); }
            else { g(IP2); g2(x->val); }
            break;
        case ARRAY_GET:
            c(x->o1);
            if (x->type == TYPE_BYTE) {
                if (x->val < 0) { g(PBAFETCH); g2(-x->val - 1); }
                else { g(BAFETCH); g2(x->val); }
            } else if (x->val < 0) { g(PAFETCH); g2(-x->val - 1); }
            else { g(IAFETCH); g2(x->val); }
            break;
        case ARRAY_SET:
            c(x->o1); c(x->o2);
            if (x->type == TYPE_BYTE) {
                if (x->val < 0) { g(PBASTORE); g2(-x->val - 1); }
                else { g(BASTORE); g2(x->val); }
            } else if (x->val < 0) { g(PASTORE); g2(-x->val - 1); }
            else { g(IASTORE); g2(x->val); }
            break;
        case CST  : emit_const(x->val); break;
        case BYTE_CONVERT: c(x->o1); g(I2B); break;
        case ADD  : c(x->o1);  c(x->o2); g(IADD); break;
        case MUL  : c(x->o1);  c(x->o2); g(IMUL); break;
        case SUB  : c(x->o1);  c(x->o2); g(ISUB); break;
        case DIV  : c(x->o1);  c(x->o2); g(IDIV); break;
        case MOD  : c(x->o1);  c(x->o2); g(IMOD); break;
        case LT   : c(x->o1);  c(x->o2); g(ILT); break;
        case GT   : c(x->o1);  c(x->o2); g(IGT); break;
        case EQ   : c(x->o1);  c(x->o2); g(IEQ); break;
        case NE   : c(x->o1);  c(x->o2); g(INE); break;
        case SET  : c(x->o2);
            if (x->o1->kind == LOCAL) { g(LSTORE); g2(x->o1->val); }
            else if (x->o1->val < 0) { g(PSTORE); g2(-x->o1->val - 1); }
            else if (x->o1->type == TYPE_BYTE) { g(BSET); g2(x->o1->val); }
            else { g(ISTORE); g2(x->o1->val); }
            break;
        case IF1  : c(x->o1);  g(JZ); p1=hole(); c(x->o2); fix(p1,here); break;
        case IF2  : c(x->o1);  g(JZ); p1=hole(); c(x->o2); g(JMP); p2=hole();
                    fix(p1,here); c(x->o3); fix(p2,here); break;
        case WHILE: p1=here; c(x->o1); g(JZ); p2=hole(); c(x->o2);
                    g(JMP); fix(hole(),p1); fix(p2,here); break;
        case DO   : p1=here; c(x->o1); c(x->o2); g(JNZ); fix(hole(),p1); break;
        case EMPTY: break;
        case SEQ  : c(x->o1); c(x->o2); break;
        case EXPR:
            if (x->o1->kind == FUNC_CALL && x->o1->val != BUILTIN_EMIT) {
                c(x->o1->o1);
                emit_call(x->o1->val, argument_count(x->o1->o1));
            } else {
                c(x->o1);
            }
            g(IDROP);
            break;
        case ARG_LIST: c(x->o1); c(x->o2); break;
        case RETURN_NODE: c(x->o1); g(IRET); break;
        case VOID_RETURN: g(IP1); g(0); g(IRET); break;
        case PROG :
            emit_call(main_function, 0);
            g(IDROP);
            g(HALT);
            for (int i = 0; i < num_functions; i++) {
                functions[i].address = (int)(here - vm);
                c(functions[i].body);
                g(IP1); g(0); g(IRET);
            }
            patch_calls();
            break;
        case FUNC_CALL:
            if (x->val == BUILTIN_EMIT) {
                c(x->o1->o1);
                g(SYS); g(SYS_EMIT);
                break;
            }
            if (!functions[x->val].returns_value) {
                fail("-void function %s used as a value-\n", functions[x->val].name);
            }
            c(x->o1);
            emit_call(x->val, argument_count(x->o1));
            break;
    }
}
