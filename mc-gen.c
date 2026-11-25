//--------------------------------------------------------------------------
// Code generator for min-c

#include "mc.h"
#include <sys/stat.h>

// -------------------------------------------------
// Code generation. Expressions leave their value in eax. A binary operation evaluates its left operand and
// pushes it, evaluates the right operand into eax, moves that to ecx, pops the left operand into eax, and then
// applies the operator. A statement leaves the stack as it found it.
//
// Calling convention: arguments are evaluated left to right and pushed, then the call pushes the return address.
// The callee pushes ebp, sets ebp to esp, and pushes a zero for each local. Parameter i of n is at
// [ebp + 8 + 4*(n-1-i)] and local j is at [ebp - 4*(j+1)]. The caller removes the arguments after the call.
// The result is returned in eax.

struct call_fixup_s callFixups[MAX_CALL_FIXUPS];
int here;
int numCallFixups;
static struct function_s *curFunction;

static void checkSize(int sz) { qFail(here + sz > CODE_TOP, "-program too large-"); }
static void g(int b) { checkSize(1); *MEM_AT(here++) = (byte)b; }
static void g4(int v) { checkSize(4); memcpy(MEM_AT(here), &v, 4); here += 4; }
static void movImm(int reg, int v) { g(X_MOV_R_IMM + reg); g4(v); }

// Reserves a 4-byte field for a rel32 displacement. fix() or patchCalls() fills it in.
static int hole() {
    checkSize(4);
    int p = here;
    here += 4;
    return p;
}

// The rel32 displacement is measured from the end of the field, which is also the end of the instruction.
static void fix(int field, int dst) {
    int rel = dst - (field + 4);
    memcpy(MEM_AT(field), &rel, 4);
}

// rr() emits an opcode with a ModRM byte for two registers. reg is a register or an opcode extension (/digit).
// ri() adds an imm32 after such an operand.
static void rr(int opcode, int reg, int rm) { g(opcode); g(0xC0 | (reg << 3) | rm); }
static void ri(int opcode, int ext, int rm, int imm) { rr(opcode, ext, rm); g4(imm); }
static void movzxAl() { g(X_ESCAPE); rr(X_MOVZX_R_RM8, EAX, EAX); }
static void int80() { g(X_INT); g(0x80); }

// Memory operands: a frame slot is [ebp+disp32], a fixed address is [disp32], and [base] is a register.
static int slotDisp(int slot) {
    int numParams = curFunction->numParameters;
    return slot < numParams ? 8 + 4 * (numParams - 1 - slot) : -4 * (slot - numParams + 1);
}
static void modrmFrame(int reg, int slot) { g(0x80 | (reg << 3) | EBP); g4(slotDisp(slot)); }
static void modrmAbs(int reg, int addr) { g((reg << 3) | 5); g4(addr); }  // mod=00, rm=101 means disp32
static void modrmIndirect(int reg, int base) { g((reg << 3) | base); }

// Emits a call to a user function. patchCalls() fills in the target address.
static void emitCall(int index) {
    qFail(numCallFixups >= MAX_CALL_FIXUPS, "-too many function calls-");
    g(X_CALL);
    callFixups[numCallFixups].operand = hole();
    callFixups[numCallFixups].functionIndex = index;
    numCallFixups++;
}

static void patchCalls() {
    for (int i = 0; i < numCallFixups; i++) {
        struct call_fixup_s *f = &callFixups[i];
        fix(f->operand, functions[f->functionIndex].address);
    }
}


// Writes the code, the global data, and the sizes needed to read them back.
int saveImage(const char *fileName) {
    FILE *fp = fopen(fileName, "wb");
    qFail(!fp, "-cannot write image %s-\n", fileName);
    int codeSize = here - CODE_BASE;
    fwrite(&codeSize, sizeof(int), 1, fp);
    fwrite(&numGlobalBytes, sizeof(int), 1, fp);
    fwrite(MEM_AT(CODE_BASE), 1, codeSize, fp);
    fwrite(MEM_AT(LOAD_BASE), 1, numGlobalBytes, fp);
    fclose(fp);
    return numGlobalBytes + codeSize + (2*sizeof(int));
}

// Writes a little-endian value of the given size in bytes.
static void putLE(FILE *fp, unsigned value, int size) {
    for (int i = 0; i < size; i++) { fputc((value >> (8*i)) & 0xff, fp); }
}

// Writes a 32-bit Linux executable. The globals and code form one PT_LOAD segment at LOAD_BASE. The segment
// is read from file offset 0x1000 so the ELF headers stay outside the globals, and execution starts at CODE_BASE.
int saveExecutable(const char *fileName) {
    const int pageSize = 0x1000, headerSize = 52, programHeaderSize = 32;
    unsigned dataSize = (unsigned)numGlobalBytes;
    unsigned codeSize = (unsigned)(here - CODE_BASE);
    unsigned codeOffset = pageSize + ((dataSize + pageSize - 1) & ~(unsigned)(pageSize - 1));
    FILE *fp = fopen(fileName, "wb");
    qFail(!fp, "-cannot write executable %s-\n", fileName);
    fwrite("\x7f" "ELF", 1, 4, fp);
    putLE(fp, 1, 1);                    // 32-bit
    putLE(fp, 1, 1);                    // little-endian
    putLE(fp, 1, 1);                    // ELF version
    putLE(fp, 0, 9);                    // OS/ABI, ABI version, padding
    putLE(fp, 2, 2);                    // e_type: ET_EXEC
    putLE(fp, 3, 2);                    // e_machine: EM_386
    putLE(fp, 1, 4);                    // e_version
    putLE(fp, CODE_BASE, 4);            // e_entry
    putLE(fp, headerSize, 4);           // e_phoff
    putLE(fp, 0, 4);                    // e_shoff
    putLE(fp, 0, 4);                    // e_flags
    putLE(fp, headerSize, 2);           // e_ehsize
    putLE(fp, programHeaderSize, 2);    // e_phentsize
    putLE(fp, 2, 2);                    // e_phnum
    putLE(fp, 0, 2);                    // e_shentsize
    putLE(fp, 0, 2);                    // e_shnum
    putLE(fp, 0, 2);                    // e_shstrndx
    // Data segment: globals, read/write. The zeroed part of the global area is not stored in the file.
    putLE(fp, 1, 4);                    // p_type: PT_LOAD
    putLE(fp, pageSize, 4);             // p_offset
    putLE(fp, LOAD_BASE, 4);            // p_vaddr
    putLE(fp, LOAD_BASE, 4);            // p_paddr
    putLE(fp, dataSize, 4);             // p_filesz
    putLE(fp, MAX_GLOBAL_BYTES, 4);     // p_memsz
    putLE(fp, 6, 4);                    // p_flags: read, write
    putLE(fp, pageSize, 4);             // p_align
    // Text segment: code, read/execute, starting at CODE_BASE.
    putLE(fp, 1, 4);                    // p_type: PT_LOAD
    putLE(fp, codeOffset, 4);           // p_offset
    putLE(fp, CODE_BASE, 4);            // p_vaddr
    putLE(fp, CODE_BASE, 4);            // p_paddr
    putLE(fp, codeSize, 4);             // p_filesz
    putLE(fp, codeSize, 4);             // p_memsz
    putLE(fp, 5, 4);                    // p_flags: read, execute
    putLE(fp, pageSize, 4);             // p_align
    for (int i = headerSize + 2 * programHeaderSize; i < pageSize; i++) { fputc(0, fp); }
    fwrite(MEM_AT(LOAD_BASE), 1, dataSize, fp);
    for (unsigned i = pageSize + dataSize; i < codeOffset; i++) { fputc(0, fp); }
    fwrite(MEM_AT(CODE_BASE), 1, codeSize, fp);
    fclose(fp);
    chmod(fileName, 0755);
    return (int)(codeOffset + codeSize);
}
    
/*
// This function is for a later stage when we want to save and
// load the generated code and global data.
void badImage(const char *fileName) { qFail(1, "-bad image %s-\n", fileName); }

// Reads an image written by saveImage() and returns the bytes read
int loadImage(const char *fileName) {
    FILE *fp = fopen(fileName, "rb");
    qFail(!fp, "-cannot read image %s-\n", fileName);
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

// A parameter or local is a frame slot. Parameters are slots 0..numParameters-1 and the locals follow them.
static int isSlot(node *x) { return x->kind == LOCAL || x->val < 0; }
static int slotOf(node *x) { return x->kind == LOCAL ? x->val : -x->val - 1; }

// Loads variable t into eax.
static void loadVar(node *t) {
    if (isSlot(t)) { g(X_MOV_R_RM); modrmFrame(EAX, slotOf(t)); }
    else if (t->type == TYPE_BYTE) { g(X_ESCAPE); g(X_MOVZX_R_RM8); modrmAbs(EAX, t->val); }
    else { g(X_MOV_R_RM); modrmAbs(EAX, t->val); }
}

// Stores eax into variable t.
static void storeVar(node *t) {
    if (isSlot(t)) { g(X_MOV_RM_R); modrmFrame(EAX, slotOf(t)); }
    else if (t->type == TYPE_BYTE) { g(X_MOV_RM8_R8); modrmAbs(EAX, t->val); }
    else { g(X_MOV_RM_R); modrmAbs(EAX, t->val); }
}

// Loads the base address of array x into eax: a global's address, or the address held by a parameter.
static void loadBase(node *x) {
    if (x->val < 0) { g(X_MOV_R_RM); modrmFrame(EAX, -x->val - 1); }
    else { movImm(EAX, x->val); }
}

static void cExpr(node *x);
static void cEffect(node *x);

// Leaves the address of element x->o1 of array x in eax.
static void cElementAddress(node *x) {
    cExpr(x->o1);
    if (x->type != TYPE_BYTE) { rr(X_GRP2_RM_IMM8, 4, EAX); g(2); }
    if (x->val >= 0) { ri(X_GRP1_RM_IMM32, 0, EAX, x->val); }
    else {
        rr(X_MOV_RM_R, EAX, ECX);
        loadBase(x);
        rr(X_ADD_RM_R, ECX, EAX);
    }
}

// Calls a user function. The arguments are pushed in order, and the caller removes them after the call.
static void cCall(node *x) {
    int n = 0;
    for (node *a = x->o1; a; a = a->o2) {
        cExpr(a->o1);
        g(X_PUSH + EAX);
        n++;
    }
    emitCall(x->val);
    if (n > 0) { ri(X_GRP1_RM_IMM32, 0, ESP, 4 * n); }
}

// Writes the low byte of arg to stdout with the write system call. The byte is left in eax.
static void cEmit(node *arg) {
    cExpr(arg);
    g(X_PUSH + EAX);            // the byte is now at [esp]
    rr(X_MOV_RM_R, ESP, ECX);
    movImm(EDX, 1);
    movImm(EBX, 1);
    movImm(EAX, 4);
    int80();
    g(X_POP + EAX);
    movzxAl();
}

// Evaluates the operands of a binary operation: the left operand ends up in eax and the right one in ecx.
static void cOperands(node *x) {
    cExpr(x->o1);
    g(X_PUSH + EAX);
    cExpr(x->o2);
    rr(X_MOV_RM_R, EAX, ECX);
    g(X_POP + EAX);
}

// Compares eax with ecx and leaves 0 or 1 in eax.
static void cCompare(int cc) {
    rr(X_CMP_RM_R, ECX, EAX);
    g(X_ESCAPE); rr(X_SETCC + cc, 0, EAX);
    movzxAl();
}

// Adds x->val to the variable of x. With keep, the expression's value is left on the stack:
// the old value for a post-increment, the new value for a pre-increment.
static void cIncrement(node *x, int keep) {
    node *t = x->o1;
    int isPost = x->kind == POST_INC;
    loadVar(t);
    if (keep && isPost) { g(X_PUSH + EAX); }
    ri(X_GRP1_RM_IMM32, 0, EAX, x->val);
    if (x->type == TYPE_BYTE) { movzxAl(); }
    storeVar(t);
    if (keep && isPost) { g(X_POP + EAX); }
}

// Loads the element at eax (as found by cElementAddress) into eax.
static void loadElement(int type) {
    if (type == TYPE_BYTE) { g(X_ESCAPE); g(X_MOVZX_R_RM8); modrmIndirect(EAX, EAX); }
    else { g(X_MOV_R_RM); modrmIndirect(EAX, EAX); }
}

// Evaluates expression x into eax. Every expression leaves its value in eax.
static void cExpr(node *x) {
    switch (x->kind) {
        case CST: movImm(EAX, x->val); break;
        case VAR: case LOCAL: loadVar(x); break;
        case ARRAY_REF: loadBase(x); break;
        case ARRAY_GET: cElementAddress(x); loadElement(x->type); break;
        case BYTE_CONVERT: cExpr(x->o1); movzxAl(); break;
        case ADD: cOperands(x); rr(X_ADD_RM_R, ECX, EAX); break;
        case SUB: cOperands(x); rr(X_SUB_RM_R, ECX, EAX); break;
        case MUL: cOperands(x); g(X_ESCAPE); rr(X_IMUL_R_RM, EAX, ECX); break;
        case DIV: case MOD:
            cOperands(x);
            g(X_CDQ);
            rr(X_GRP3_RM, 7, ECX);  // idiv ecx: the quotient is in eax and the remainder in edx
            if (x->kind == MOD) { rr(X_MOV_RM_R, EDX, EAX); }
            break;
        case LT:  cOperands(x); cCompare(CC_L); break;
        case GT:  cOperands(x); cCompare(CC_G); break;
        case LE:  cOperands(x); cCompare(CC_LE); break;
        case GE:  cOperands(x); cCompare(CC_GE); break;
        case EQ:  cOperands(x); cCompare(CC_E); break;
        case NE:  cOperands(x); cCompare(CC_NE); break;
        case PRE_INC: case POST_INC: cIncrement(x, 1); break;
        case FUNC_CALL:
            if (x->val == BUILTIN_EMIT) { cEmit(x->o1->o1); break; }
            qFail(!functions[x->val].returnsValue, "-void function %s used as a value-\n", functions[x->val].name);
            cCall(x);
            break;
        default: qFail(1, "-internal: bad expression kind %d-", x->kind);
    }
}

// Stores into element x->o1 of array x (an ARRAY_SET node). The address is computed first, then the value.
static void cElementStore(node *x) {
    cElementAddress(x);
    g(X_PUSH + EAX);
    cExpr(x->o2);
    g(X_POP + ECX);
    if (x->type == TYPE_BYTE) { g(X_MOV_RM8_R8); } else { g(X_MOV_RM_R); }
    modrmIndirect(EAX, ECX);
}

// Evaluates x for its side effects. A value that x leaves in eax is simply overwritten by the next expression.
static void cEffect(node *x) {
    switch (x->kind) {
        case SET: cExpr(x->o2); storeVar(x->o1); break;
        case PRE_INC: case POST_INC: cIncrement(x, 0); break;
        case ARRAY_SET: cElementStore(x); break;
        case FUNC_CALL:
            if (x->val == BUILTIN_EMIT) { cExpr(x); } else { cCall(x); }
            break;
        default: cExpr(x); break;
    }
}

// Emits a jump taken when x is true (whenTrue) or false. Returns the hole for fix().
static int condJump(node *x, int whenTrue) {
    cExpr(x);
    rr(X_TEST_RM_R, EAX, EAX);
    g(X_ESCAPE); g(X_JCC + (whenTrue ? CC_NE : CC_E));
    return hole();
}

// Tears down the frame and returns to the caller. The caller removes the arguments.
static void epilogue() { g(X_LEAVE); g(X_RET); }

// Emits statement x. A statement leaves the stack as it found it.
static void cStmt(node *x) {
    int p1, p2, top;
    if (!x) { return; }
    switch (x->kind) {
        case SEQ: cStmt(x->o1); cStmt(x->o2); break;
        case EMPTY: break;
        case EXPR: cEffect(x->o1); break;
        case IF1:
            p1 = condJump(x->o1, 0);
            cStmt(x->o2);
            fix(p1, here);
            break;
        case IF2:
            p1 = condJump(x->o1, 0);
            cStmt(x->o2);
            g(X_JMP); p2 = hole();
            fix(p1, here);
            cStmt(x->o3);
            fix(p2, here);
            break;
        case WHILE:
            top = here;
            p1 = condJump(x->o1, 0);
            cStmt(x->o2);
            g(X_JMP); fix(hole(), top);
            fix(p1, here);
            break;
        case DO:
            top = here;
            cStmt(x->o1);
            p1 = condJump(x->o2, 1);
            fix(p1, top);
            break;
        case RETURN_NODE: cExpr(x->o1); epilogue(); break;
        case VOID_RETURN: epilogue(); break;
        default: cEffect(x); break;
    }
}

// Generates the program. The code starts with a call to main that passes its return value to the exit system
// call, then each function. A function sets up its frame, runs its body, and returns 0 if it falls off the end.
void c(node *prog) {
    qFail(prog->kind != PROG, "-program expected-");
    here = CODE_BASE;
    emitCall(mainFunction);
    rr(X_MOV_RM_R, EAX, EBX);  // the exit status
    movImm(EAX, 1);            // the exit system call
    int80();
    for (int i = 0; i < numFunctions; i++) {
        struct function_s *f = &functions[i];
        curFunction = f;
        f->address = here;
        g(X_PUSH + EBP);
        rr(X_MOV_RM_R, ESP, EBP);
        movImm(EAX, 0);
        for (int j = 0; j < f->numLocals; j++) { g(X_PUSH + EAX); }
        cStmt(f->body);
        movImm(EAX, 0);
        epilogue();
    }
    patchCalls();
}

