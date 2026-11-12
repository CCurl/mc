//--------------------------------------------------------------------------
// Code generator for tiny-c

#include "tiny-c.h"

byte vm[MAX_CODE], *here = vm;

struct call_fixup_s { byte *operand; int functionIndex; };
struct call_fixup_s callFixups[MAX_CALL_FIXUPS];
int numCallFixups;

void checkSize(int sz) { if (here + sz > vm + MAX_CODE) { error("-program too large-"); } }
void g(byte c)   { checkSize(1); *here++ = c; }
void g2(short c) { checkSize(2); *(short *)here = c; here += 2; }
void g4(int c)   { checkSize(4); *(int *)here = c; here += 4; }

byte *hole() {
    checkSize(4);
    byte *p = here;
    here += 4;
    return p;
}
void fix(byte *src, byte *dst) {
    *(int *)src = (int)(dst - src);
}

void emitCall(int index, int argc) {
    if (numCallFixups >= MAX_CALL_FIXUPS) { error("-too many function calls-"); }
    g(ICALL);
    callFixups[numCallFixups].operand = here;
    callFixups[numCallFixups].functionIndex = index;
    numCallFixups++;
    g4(0);
    g4(argc);
    g4(functions[index].numLocals);
}

void patchCalls() {
    for (int i = 0; i < numCallFixups; i++) {
        *(int *)callFixups[i].operand = functions[callFixups[i].functionIndex].address;
    }
}

void emitConst(int v) {
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
        *out = (byte)a;
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

/* Emits an in-place step of an int scalar: LINC/LDEC for locals and parameters, GINC/GDEC
   for globals. Returns 0 and emits nothing for byte targets. */
int emitStep(node *target, int delta) {
    if (target->type == TYPE_BYTE) { return 0; }
    if (target->kind == LOCAL) { g(delta > 0 ? LINC : LDEC); g4(target->val); }
    else if (target->val < 0) { g(delta > 0 ? LINC : LDEC); g4(-target->val - 1); }
    else { g(delta > 0 ? GINC : GDEC); g4(target->val); }
    return 1;
}

/* Stores the top of stack into a scalar target; the value stays on the stack. */
void emitStore(node *t) {
    if (t->kind == LOCAL) { g(LSTORE); g4(t->val); }
    else if (t->val < 0) { g(PSTORE); g4(-t->val - 1); }
    else if (t->type == TYPE_BYTE) { g(BSET); g4(t->val); }
    else { g(ISTORE); g4(t->val); }
}

void c(node *x) {
    byte *p1, *p2;
    int folded;
    if (!x) { return; }
    if (x->kind != CST && fold(x, &folded)) { emitConst(folded); return; }
    switch (x->kind) {
        case VAR:
            if (x->val < 0) { g(PLOAD); g4(-x->val - 1); }
            else if (x->type == TYPE_BYTE) { g(BGET); g4(x->val); }
            else { g(IFETCH); g4(x->val); }
            break;
        case LOCAL: g(LLOAD); g4(x->val); break;
        case ARRAY_REF:
            if (x->val < 0) { g(PLOAD); g4(-x->val - 1); }
            else { emitConst(x->val); }
            break;
        case ARRAY_GET:
            c(x->o1);
            if (x->type == TYPE_BYTE) {
                if (x->val < 0) { g(PBAFETCH); g4(-x->val - 1); }
                else { g(BAFETCH); g4(x->val); }
            } else if (x->val < 0) { g(PAFETCH); g4(-x->val - 1); }
            else { g(IAFETCH); g4(x->val); }
            break;
        case ARRAY_SET:
            c(x->o1); c(x->o2);
            if (x->type == TYPE_BYTE) {
                if (x->val < 0) { g(PBASTORE); g4(-x->val - 1); }
                else { g(BASTORE); g4(x->val); }
            } else if (x->val < 0) { g(PASTORE); g4(-x->val - 1); }
            else { g(IASTORE); g4(x->val); }
            break;
        case CST  : emitConst(x->val); break;
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
        case SET  : c(x->o2); emitStore(x->o1); break;
        case PRE_INC:
            if (x->type != TYPE_BYTE) { emitStep(x->o1, x->val); c(x->o1); break; }
            c(x->o1); emitConst(x->val); g(IADD); g(I2B);
            emitStore(x->o1);
            break;
        case POST_INC:
            if (x->type != TYPE_BYTE) { c(x->o1); emitStep(x->o1, x->val); break; }
            c(x->o1); c(x->o1); emitConst(x->val); g(IADD); g(I2B);
            emitStore(x->o1); g(IDROP);
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
            if ((x->o1->kind == PRE_INC || x->o1->kind == POST_INC) && emitStep(x->o1->o1, x->o1->val)) { break; }
            if (x->o1->kind == FUNC_CALL && x->o1->val != BUILTIN_EMIT) {
                c(x->o1->o1);
                emitCall(x->o1->val, argumentCount(x->o1->o1));
            } else {
                c(x->o1);
            }
            g(IDROP);
            break;
        case ARG_LIST: c(x->o1); c(x->o2); break;
        case RETURN_NODE: c(x->o1); g(IRET); break;
        case VOID_RETURN: g(IP1); g(0); g(IRET); break;
        case PROG :
            emitCall(mainFunction, 0);
            g(IDROP);
            g(HALT);
            for (int i = 0; i < numFunctions; i++) {
                functions[i].address = (int)(here - vm);
                c(functions[i].body);
                g(IP1); g(0); g(IRET);
            }
            patchCalls();
            break;
        case FUNC_CALL:
            if (x->val == BUILTIN_EMIT) {
                c(x->o1->o1);
                g(SYS); g(SYS_EMIT);
                break;
            }
            if (!functions[x->val].returnsValue) {
                fail("-void function %s used as a value-\n", functions[x->val].name);
            }
            c(x->o1);
            emitCall(x->val, argumentCount(x->o1));
            break;
    }
}
