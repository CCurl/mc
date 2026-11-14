//--------------------------------------------------------------------------
// Code generator for tiny-c

#include "tiny-c.h"

// -------------------------------------------------
struct call_fixup_s callFixups[MAX_CALL_FIXUPS];
byte vm[MAX_CODE];
int here;
int numCallFixups;

// Register allocation for the function being generated: parameters, then locals, then temporaries.
int nextReg, maxReg, firstTemp;

#define OPCODE_NAME(name, kinds) #name,
#define OPCODE_KINDS(name, kinds) kinds,
const char *const opcodeNames[] = { OPCODES(OPCODE_NAME) };
const char *const opcodeKinds[] = { OPCODES(OPCODE_KINDS) };

// The length of an instruction: the opcode byte plus its operands ('r' is 2 bytes; 'a' and 't' are 4).
int opcodeLength(int op) {
    int len = 1;
    for (const char *k = opcodeKinds[op]; *k; k++) { len += (*k == 'r') ? 2 : 4; }
    return len;
}

static void checkSize(int sz) { if (here + sz > MAX_CODE) { fail("-program too large-"); } }
static void g(byte c)   { checkSize(1); vm[here++] = c; }
static void g2(short c) { checkSize(2); *(short *)(vm + here) = c; here += 2; }
static void g4(int c)   { checkSize(4); *(int *)(vm + here) = c; here += 4; }

static int hole() {
    checkSize(4);
    int p = here;
    here += 4;
    return p;
}
static void fix(int src, int dst) {
    *(int *)(vm + src) = dst - src;
}

// The callee's frame starts at register base of the caller's frame. The operand holds the address,
// the base register, and the callee's frame size, which patchCalls() fills in.
static void emitCall(int index, int base) {
    if (numCallFixups >= MAX_CALL_FIXUPS) { fail("-too many function calls-"); }
    g(CALL);
    callFixups[numCallFixups].operand = here;
    callFixups[numCallFixups].functionIndex = index;
    numCallFixups++;
    g4(0);
    g2(base);
    g4(0);
}

static void patchCalls() {
    for (int i = 0; i < numCallFixups; i++) {
        struct call_fixup_s *f = &callFixups[i];
        *(int *)(vm + f->operand) = functions[f->functionIndex].address;
        *(int *)(vm + f->operand + 6) = functions[f->functionIndex].frameSize;
    }
}

/*
// These 2 functions are for a later stage when we want to save and 
// load the generated code and global data.

// Writes the code, the global data, and the sizes needed to read them back.
int saveImage(const char *fileName) {
    FILE *fp = fopen(fileName, "wb");
    if (!fp) { fail("-cannot write image %s-\n", fileName); }
    fwrite(&here, sizeof(int), 1, fp);
    fwrite(&numGlobalBytes, sizeof(int), 1, fp);
    fwrite(vm, 1, here, fp);
    fwrite(globalMem, 1, numGlobalBytes, fp);
    fclose(fp);
    return numGlobalBytes + here + (2*sizeof(int));
}

void badImage(const char *fileName) { fail("-bad image %s-\n", fileName); }

// Reads an image written by saveImage() and returns the bytes read
int loadImage(const char *fileName) {
    FILE *fp = fopen(fileName, "rb");
    if (!fp) { fail("-cannot read image %s-\n", fileName); }
    int codeSize, globalSize;
    if (fread(&codeSize, sizeof(int), 1, fp) != 1) { badImage(fileName); }
    if (fread(&globalSize, sizeof(int), 1, fp) != 1) { badImage(fileName); }
    if (codeSize < 0 || codeSize > MAX_CODE) { badImage(fileName); }
    if (globalSize < 0 || globalSize > MAX_GLOBAL_BYTES) { badImage(fileName); }
    if (fread(vm, 1, codeSize, fp) != (size_t)codeSize) { badImage(fileName); }
    if (fread(globalMem, 1, globalSize, fp) != (size_t)globalSize) { badImage(fileName); }
    fclose(fp);
    here = codeSize;
    numGlobalBytes = globalSize;
    return numGlobalBytes + here + (2*sizeof(int));
}
*/

static void emitLdi(int r, int v) { g(LDI); g2(r); g4(v); }
static void emitMov(int dst, int src) { if (dst != src) { g(MOV); g2(dst); g2(src); } }
static void emitI2b(int dst, int src) { g(I2B); g2(dst); g2(src); }
static void emitIaddi(int dst, int src, int v) { g(IADDI); g2(dst); g2(src); g4(v); }
static void emit3(int op, int dst, int a, int b) { g(op); g2(dst); g2(a); g2(b); }

// Returns 1 and sets *out if x is a constant expression that can be folded.
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
        case LE:  *out = a <= b; return 1;
        case GE:  *out = a >= b; return 1;
        case EQ:  *out = a == b; return 1;
        case NE:  *out = a != b; return 1;
        default: return 0;
    }
}

static void emitRa(int op, int r, int a) { g(op); g2(r); g4(a); }
static void emitRar(int op, int r, int a, int idx) { g(op); g2(r); g4(a); g2(idx); }
static void emitRaa(int op, int r, int a, int b) { g(op); g2(r); g4(a); g4(b); }

// Emits a conditional jump on register r. Returns the offset of its target field for fix().
static int emitJumpIf(int op, int r) { g(op); g2(r); return hole(); }

// Variables held in registers: locals, and parameters (which are registers 0..numParameters-1).
static int isRegVar(node *x) { return x->kind == LOCAL || (x->kind == VAR && x->val < 0); }
static int regOf(node *x) { return x->kind == LOCAL ? x->val : -x->val - 1; }

static int allocReg() {
    if (nextReg >= MAX_REGISTERS) { fail("-too many registers-"); }
    int r = nextReg++;
    if (nextReg > maxReg) { maxReg = nextReg; }
    return r;
}

// True if evaluating x may assign a variable.
static int hasSideEffect(node *x) {
    if (!x) { return 0; }
    if (x->kind == SET || x->kind == PRE_INC || x->kind == POST_INC || x->kind == ARRAY_SET) { return 1; }
    return hasSideEffect(x->o1) || hasSideEffect(x->o2);
}

static void cInto(node *x, int dst);
static int cAny(node *x);
static void cAssign(node *x);
static void cElementStore(node *x);
static void cStmt(node *x);

// Returns a register holding the value of x. A variable's own register is returned unless it is
// copied first, which happens when later may change it.
static int cOperand(node *x, node *later) {
    if (isRegVar(x) && hasSideEffect(later)) {
        int t = allocReg();
        emitMov(t, regOf(x));
        return t;
    }
    return cAny(x);
}

// Returns a register holding the value of x, which is a variable's register or a new temporary.
static int cAny(node *x) {
    if (isRegVar(x)) { return regOf(x); }
    int t = allocReg();
    cInto(x, t);
    return t;
}

// Loads register r from, or stores r into, element x->o1 of array x. The index is in register idx,
// or is the constant k when idx is -1. The VM scales int elements by 4.
static void emitElement(node *x, int r, int idx, int k, int isStore) {
    int isByte = x->type == TYPE_BYTE;
    if (x->val >= 0) {
        if (idx < 0) {
            emitRa(isByte ? (isStore ? BSET : BGET) : (isStore ? ISET : IGET), r, x->val + (isByte ? 1 : 4) * k);
        } else {
            emitRar(isByte ? (isStore ? BASET : BAGET) : (isStore ? IASET : IAGET), r, x->val, idx);
        }
        return;
    }
    if (idx < 0) { idx = allocReg(); emitLdi(idx, k); }
    emit3(isByte ? (isStore ? PBASET : PBAGET) : (isStore ? PIASET : PIAGET), r, -x->val - 1, idx);
}

// Assigns the value of SET node x.
static void cAssign(node *x) {
    node *t = x->o1;
    if (isRegVar(t)) {
        cInto(x->o2, regOf(t));
        return;
    }
    int v = cAny(x->o2);
    emitRa(t->type == TYPE_BYTE ? BSET : ISET, v, t->val);
}

// Stores the value of ARRAY_SET node x.
static void cElementStore(node *x) {
    int k, idx, v;
    if (fold(x->o1, &k)) {
        v = cAny(x->o2);
        emitElement(x, v, -1, k, 1);
        return;
    }
    idx = cOperand(x->o1, x->o2);
    v = cAny(x->o2);
    emitElement(x, v, idx, 0, 1);
}

// Steps a variable by x->val. For post-increments the old value goes to dst; dst -1 discards it.
static void cIncrement(node *x, int dst) {
    node *t = x->o1;
    int isPost = x->kind == POST_INC, isByte = x->type == TYPE_BYTE;
    if (isRegVar(t)) {
        int r = regOf(t);
        if (isPost && dst >= 0) { emitMov(dst, r); }
        emitIaddi(r, r, x->val);
        if (isByte) { emitI2b(r, r); }
        if (!isPost && dst >= 0) { emitMov(dst, r); }
        return;
    }
    if (dst < 0) { dst = allocReg(); }
    int op = isByte ? (isPost ? BINCP : BINC) : (isPost ? IINCP : IINC);
    emitRaa(op, dst, t->val, x->val);
}

// Calls the user function of node x, with the result in dst (-1 discards it). The callee's frame
// starts at the first argument register. That is dst when dst is the top temporary, so no copy is needed.
static void cCall(node *x, int dst) {
    int mark = nextReg, base, i = 0;
    base = (dst >= firstTemp && dst == nextReg - 1) ? dst : nextReg;
    nextReg = base;
    for (node *a = x->o1; a; a = a->o2) { allocReg(); }
    for (node *a = x->o1; a; a = a->o2, i++) { cInto(a->o1, base + i); }
    emitCall(x->val, base);
    nextReg = mark;
    if (dst >= 0) { emitMov(dst, base); }
}

static void cBinary(node *x, int dst, int op) {
    int lr = cOperand(x->o1, x->o2);
    int rr = cAny(x->o2);
    emit3(op, dst, lr, rr);
}

// Evaluates x and leaves its value in register dst.
static void cInto(node *x, int dst) {
    int k, a;
    if (x->kind == CST) { emitLdi(dst, x->val); return; }
    if (fold(x, &k)) { emitLdi(dst, k); return; }
    switch (x->kind) {
        case VAR:
            if (isRegVar(x)) { emitMov(dst, regOf(x)); }
            else { emitRa(x->type == TYPE_BYTE ? BGET : IGET, dst, x->val); }
            break;
        case LOCAL: emitMov(dst, x->val); break;
        case ARRAY_REF:
            if (x->val < 0) { emitMov(dst, -x->val - 1); } else { emitLdi(dst, x->val); }
            break;
        case ARRAY_GET:
            if (fold(x->o1, &k)) { emitElement(x, dst, -1, k, 0); }
            else { a = cAny(x->o1); emitElement(x, dst, a, 0, 0); }
            break;
        case BYTE_CONVERT: cInto(x->o1, dst); emitI2b(dst, dst); break;
        case ADD:
            if (fold(x->o2, &k)) { a = cAny(x->o1); emitIaddi(dst, a, k); break; }
            if (fold(x->o1, &k)) { a = cAny(x->o2); emitIaddi(dst, a, k); break; }
            cBinary(x, dst, IADD);
            break;
        case SUB:
            if (fold(x->o2, &k)) { a = cAny(x->o1); emitIaddi(dst, a, (int)(0u - (unsigned)k)); break; }
            cBinary(x, dst, ISUB);
            break;
        case MUL: cBinary(x, dst, IMUL); break;
        case DIV: cBinary(x, dst, IDIV); break;
        case MOD: cBinary(x, dst, IMOD); break;
        case LT:  cBinary(x, dst, ILT); break;
        case GT:  cBinary(x, dst, IGT); break;
        case LE:  cBinary(x, dst, ILE); break;
        case GE:  cBinary(x, dst, IGE); break;
        case EQ:  cBinary(x, dst, IEQ); break;
        case NE:  cBinary(x, dst, INE); break;
        case PRE_INC: case POST_INC: cIncrement(x, dst); break;
        case FUNC_CALL:
            if (x->val == BUILTIN_EMIT) { a = cAny(x->o1->o1); g(EMIT); g2(dst); g2(a); break; }
            if (!functions[x->val].returnsValue) {
                fail("-void function %s used as a value-\n", functions[x->val].name);
            }
            cCall(x, dst);
            break;
        default: break;
    }
}

// The fused opcode that jumps when comparison x is true (whenTrue) or false, or -1 if x is not a
// comparison. Negating an int comparison swaps it with its inverse, e.g. not (a < b) is a >= b.
static int condJumpOp(node *x, int whenTrue) {
    switch (x->kind) {
        case LT: return whenTrue ? JLT : JGE;
        case GT: return whenTrue ? JGT : JLE;
        case LE: return whenTrue ? JLE : JGT;
        case GE: return whenTrue ? JGE : JLT;
        case EQ: return whenTrue ? JEQ : JNE;
        case NE: return whenTrue ? JNE : JEQ;
        default: return -1;
    }
}

// Emits a jump taken when condition x is true (whenTrue) or false. Returns the hole for fix().
static int condJump(node *x, int whenTrue) {
    int k, op = condJumpOp(x, whenTrue);
    if (op >= 0 && !fold(x, &k)) {
        int lr = cOperand(x->o1, x->o2);
        int rr = cAny(x->o2);
        g(op); g2(lr); g2(rr);
        return hole();
    }
    return emitJumpIf(whenTrue ? JNZ : JZ, cAny(x));
}

// Evaluates x for its side effects only.
static void cEffect(node *x) {
    switch (x->kind) {
        case SET: cAssign(x); break;
        case PRE_INC: case POST_INC: cIncrement(x, -1); break;
        case ARRAY_SET: cElementStore(x); break;
        case FUNC_CALL:
            if (x->val != BUILTIN_EMIT) { cCall(x, -1); break; }
            cInto(x, allocReg());
            break;
        default: cInto(x, allocReg()); break;
    }
}

static void cStmt(node *x) {
    int mark = nextReg, p1, p2, r, top;
    if (!x) { return; }
    switch (x->kind) {
        case SEQ: cStmt(x->o1); cStmt(x->o2); break;
        case EMPTY: break;
        case EXPR: cEffect(x->o1); break;
        case IF1:
            p1 = condJump(x->o1, 0);
            nextReg = mark;
            cStmt(x->o2);
            fix(p1, here);
            break;
        case IF2:
            p1 = condJump(x->o1, 0);
            nextReg = mark;
            cStmt(x->o2);
            g(JMP); p2 = hole();
            fix(p1, here);
            cStmt(x->o3);
            fix(p2, here);
            break;
        case WHILE:
            top = here;
            p1 = condJump(x->o1, 0);
            nextReg = mark;
            cStmt(x->o2);
            g(JMP); fix(hole(), top);
            fix(p1, here);
            break;
        case DO:
            top = here;
            cStmt(x->o1);
            p1 = condJump(x->o2, 1);
            fix(p1, top);
            break;
        case RETURN_NODE: r = cAny(x->o1); g(RET); g2(r); break;
        case VOID_RETURN: g(RET0); break;
        default: cEffect(x); break;
    }
    nextReg = mark;
}

// Walks the generated code one instruction at a time, using the operand sizes in OPCODES.
// Fails if an opcode is unknown or the walk does not end exactly where the code ends.
static void checkCode() {
    int p = 0;
    while (p < here) {
        if (vm[p] >= NUM_OPCODES) { fail("-internal: bad opcode at %d-", p); }
        p += opcodeLength(vm[p]);
    }
    if (p != here) { fail("-internal: code does not decode cleanly-"); }
}

// Generates the program from a PROG node: a call to main, HALT, then each function body.
void c(node *prog) {
    if (prog->kind != PROG) { fail("-program expected-"); }
    emitCall(mainFunction, 0);
    g(HALT);
    for (int i = 0; i < numFunctions; i++) {
        struct function_s *f = &functions[i];
        f->address = here;
        firstTemp = f->numParameters + f->numLocals;
        nextReg = maxReg = firstTemp;
        cStmt(f->body);
        g(RET0);
        f->frameSize = maxReg;
    }
    patchCalls();
    checkCode();
}
