/*---------------------------------------------------------------------------*/
/* Virtual machine for min-c. It runs the 32-bit x86 subset that the code generator emits. */

#include "mc.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

// -------------------------------------------------
// The memory image. Addresses run from LOAD_BASE: the globals, then the code at CODE_BASE, then the stack,
// which grows down from STACK_TOP.
byte mem[MEM_BYTES];

static void checkAddress(int addr, int size) {
    qFail(addr < LOAD_BASE || size > MEM_BYTES || addr > STACK_TOP - size, "-bad memory address %d-", addr);
}

int loadInt(int addr) {
    checkAddress(addr, 4);
    int value;
    memcpy(&value, MEM_AT(addr), 4);
    return value;
}

void storeInt(int addr, int value) {
    checkAddress(addr, 4);
    memcpy(MEM_AT(addr), &value, 4);
}

static int loadByte(int addr) { checkAddress(addr, 1); return *MEM_AT(addr); }
static void storeByte(int addr, int value) { checkAddress(addr, 1); *MEM_AT(addr) = (byte)value; }

// The processor state. Registers are indexed by their x86 encoding (EAX=0 ... EDI=7).
// The flags are kept as the two operands of the last cmp or test. Only the signed
// conditions are modelled, which is all the code generator uses.
struct cpu_s {
    int reg[8];
    int pc;
    int flagA, flagB;
};

// A decoded ModRM operand. When isReg is set the operand is register rm. Otherwise it is the memory at addr.
struct modrm_s { int isReg, reg, rm, addr; };

static int fetch8(struct cpu_s *c) { checkAddress(c->pc, 1); return *MEM_AT(c->pc++); }

static int fetch32(struct cpu_s *c) {
    int value = loadInt(c->pc);
    c->pc += 4;
    return value;
}

static void push(struct cpu_s *c, int value) {
    qFail(c->reg[ESP] - 4 < CODE_TOP, "-stack overflow-");
    c->reg[ESP] -= 4;
    storeInt(c->reg[ESP], value);
}

static int pop(struct cpu_s *c) {
    int value = loadInt(c->reg[ESP]);
    c->reg[ESP] += 4;
    return value;
}

// Decodes a ModRM byte and any displacement that follows it. Only register operands,
// [reg], [disp32], and [reg+disp32] are supported. A SIB byte (rm = ESP) or disp8 is rejected.
static struct modrm_s decodeModRM(struct cpu_s *c) {
    int b = fetch8(c);
    int mod = b >> 6;
    struct modrm_s m = { 0, (b >> 3) & 7, b & 7, 0 };
    m.isReg = mod == 3;
    if (m.isReg) { return m; }
    qFail(m.rm == ESP, "-unsupported x86 addressing: SIB-");
    qFail(mod == 1, "-unsupported x86 addressing: disp8-");
    if (mod == 0 && m.rm == EBP) { m.addr = fetch32(c); }
    else if (mod == 0) { m.addr = c->reg[m.rm]; }
    else { m.addr = c->reg[m.rm] + fetch32(c); }
    return m;
}

static int getRM(struct cpu_s *c, struct modrm_s *m) { return m->isReg ? c->reg[m->rm] : loadInt(m->addr); }
static void setRM(struct cpu_s *c, struct modrm_s *m, int value) {
    if (m->isReg) { c->reg[m->rm] = value; } else { storeInt(m->addr, value); }
}

// The 8-bit registers are the low bytes of EAX, ECX, EDX, and EBX.
static int reg8(struct cpu_s *c, int r) {
    qFail(r > EBX, "-unsupported x86 8-bit register-");
    return c->reg[r] & 0xff;
}
static void setReg8(struct cpu_s *c, int r, int value) {
    qFail(r > EBX, "-unsupported x86 8-bit register-");
    c->reg[r] = (c->reg[r] & ~0xff) | (value & 0xff);
}
static int getRM8(struct cpu_s *c, struct modrm_s *m) { return m->isReg ? reg8(c, m->rm) : loadByte(m->addr); }
static void setRM8(struct cpu_s *c, struct modrm_s *m, int value) {
    if (m->isReg) { setReg8(c, m->rm, value); } else { storeByte(m->addr, value); }
}

static int condition(struct cpu_s *c, int cc) {
    switch (cc) {
        case CC_E:  return c->flagA == c->flagB;
        case CC_NE: return c->flagA != c->flagB;
        case CC_L:  return c->flagA <  c->flagB;
        case CC_GE: return c->flagA >= c->flagB;
        case CC_LE: return c->flagA <= c->flagB;
        case CC_G:  return c->flagA >  c->flagB;
        default: qFail(1, "-unsupported x86 condition code %d-", cc);
    }
    return 0;
}

// Returns a NUL-terminated string in guest memory, checking every byte up to the terminator.
static const char *guestString(int addr) {
    int a = addr;
    while (*MEM_AT(a)) { checkAddress(a, 1); a = a + 1; }
    return (const char *)MEM_AT(addr);
}

// The kernel returns a failed call as -errno in eax. The C library wrappers return -1 and set errno instead.
static int kernelResult(long result) { return result < 0 ? -errno : (int)result; }

// Runs the int 0x80 system calls that the code generator uses: exit (1), read (3), write (4), open (5), close (6), unlink (10).
// Each call goes to the host with the same arguments, so the result matches the executable's.
// Returns 1 when the program exits.
static int systemCall(struct cpu_s *c) {
    int *r = c->reg;
    switch (r[EAX]) {
        case 1: return 1;
        case 3:
            checkAddress(r[ECX], r[EDX]);
            r[EAX] = kernelResult(read(r[EBX], MEM_AT(r[ECX]), (size_t)r[EDX]));
            return 0;
        case 4:
            checkAddress(r[ECX], r[EDX]);
            r[EAX] = kernelResult(write(r[EBX], MEM_AT(r[ECX]), (size_t)r[EDX]));
            return 0;
        case 5:
            r[EAX] = kernelResult(open(guestString(r[EBX]), r[ECX], r[EDX]));
            return 0;
        case 6:
            r[EAX] = kernelResult(close(r[EBX]));
            return 0;
        case 10:
            r[EAX] = kernelResult(unlink(guestString(r[EBX])));
            return 0;
        default: qFail(1, "-unsupported system call %d-", r[EAX]);
    }
    return 0;
}

// Runs the code from CODE_BASE until the program calls exit. Returns the exit status.
int run() {
    struct cpu_s c = { { 0 }, CODE_BASE, 0, 0 };
    c.reg[ESP] = STACK_TOP;
    for (;;) {
        int op = fetch8(&c);
        switch (op) {
            case X_ADD_RM_R: {
                struct modrm_s m = decodeModRM(&c);
                setRM(&c, &m, getRM(&c, &m) + c.reg[m.reg]);
                break;
            }
            case X_SUB_RM_R: {
                struct modrm_s m = decodeModRM(&c);
                setRM(&c, &m, getRM(&c, &m) - c.reg[m.reg]);
                break;
            }
            case X_AND_RM_R: {
                struct modrm_s m = decodeModRM(&c);
                setRM(&c, &m, getRM(&c, &m) & c.reg[m.reg]);
                break;
            }
            case X_OR_RM_R: {
                struct modrm_s m = decodeModRM(&c);
                setRM(&c, &m, getRM(&c, &m) | c.reg[m.reg]);
                break;
            }
            case X_XOR_RM_R: {
                struct modrm_s m = decodeModRM(&c);
                setRM(&c, &m, getRM(&c, &m) ^ c.reg[m.reg]);
                break;
            }
            case X_CMP_RM_R: {
                struct modrm_s m = decodeModRM(&c);
                c.flagA = getRM(&c, &m);
                c.flagB = c.reg[m.reg];
                break;
            }
            case X_TEST_RM_R: {
                struct modrm_s m = decodeModRM(&c);
                c.flagA = getRM(&c, &m) & c.reg[m.reg];
                c.flagB = 0;
                break;
            }
            case X_MOV_RM8_R8: {
                struct modrm_s m = decodeModRM(&c);
                setRM8(&c, &m, reg8(&c, m.reg));
                break;
            }
            case X_MOV_RM_R: {
                struct modrm_s m = decodeModRM(&c);
                setRM(&c, &m, c.reg[m.reg]);
                break;
            }
            case X_MOV_R_RM: {
                struct modrm_s m = decodeModRM(&c);
                c.reg[m.reg] = getRM(&c, &m);
                break;
            }
            case X_CDQ:
                c.reg[EDX] = c.reg[EAX] < 0 ? -1 : 0;
                break;
            case X_GRP1_RM_IMM32: {
                struct modrm_s m = decodeModRM(&c);
                int imm = fetch32(&c);
                switch (m.reg) {
                    case 0: setRM(&c, &m, getRM(&c, &m) + imm); break;  // add
                    case 5: setRM(&c, &m, getRM(&c, &m) - imm); break;  // sub
                    case 7: c.flagA = getRM(&c, &m); c.flagB = imm; break;  // cmp
                    default: qFail(1, "-unsupported x86 group 1 operation %d-", m.reg);
                }
                break;
            }
            case X_GRP5_RM: {
                struct modrm_s m = decodeModRM(&c);
                qFail(m.reg > 1, "-unsupported x86 group 5 operation %d-", m.reg);
                setRM(&c, &m, getRM(&c, &m) + (m.reg == 0 ? 1 : -1));  // inc, dec
                break;
            }
            case X_GRP4_RM8: {
                struct modrm_s m = decodeModRM(&c);
                qFail(m.reg > 1, "-unsupported x86 group 4 operation %d-", m.reg);
                setRM8(&c, &m, getRM8(&c, &m) + (m.reg == 0 ? 1 : -1));  // inc, dec
                break;
            }
            case X_GRP2_RM_IMM8: {
                struct modrm_s m = decodeModRM(&c);
                int count = fetch8(&c);
                qFail(m.reg != 4, "-unsupported x86 group 2 operation %d-", m.reg);  // shl
                setRM(&c, &m, (int)((unsigned)getRM(&c, &m) << (count & 31)));
                break;
            }
            case X_GRP2_RM_CL: {
                struct modrm_s m = decodeModRM(&c);
                int count = c.reg[ECX] & 31;
                if (m.reg == 4) {
                    setRM(&c, &m, (int)((unsigned)getRM(&c, &m) << count));  // shl
                } else {
                    qFail(m.reg != 7, "-unsupported x86 group 2 operation %d-", m.reg);
                    setRM(&c, &m, getRM(&c, &m) >> count);  // sar
                }
                break;
            }
            case X_GRP3_RM: {
                // not (/2): r/m = ~r/m. idiv (/7): edx:eax divided by r/m, quotient to eax, remainder to edx.
                struct modrm_s m = decodeModRM(&c);
                if (m.reg == 2) {
                    setRM(&c, &m, ~getRM(&c, &m));
                    break;
                }
                qFail(m.reg != 7, "-unsupported x86 group 3 operation %d-", m.reg);
                long long dividend = ((long long)(unsigned)c.reg[EDX] << 32) | (unsigned)c.reg[EAX];
                long long divisor = getRM(&c, &m);
                qFail(divisor == 0, "-divide by zero-");
                long long quotient = dividend / divisor;
                qFail(quotient != (int)quotient, "-divide overflow-");
                c.reg[EDX] = (int)(dividend % divisor);
                c.reg[EAX] = (int)quotient;
                break;
            }
            case X_RET:
                c.pc = pop(&c);
                break;
            case X_LEAVE:
                c.reg[ESP] = c.reg[EBP];
                c.reg[EBP] = pop(&c);
                break;
            case X_INT:
                qFail(fetch8(&c) != 0x80, "-unsupported interrupt-");
                if (systemCall(&c)) { return c.reg[EBX]; }
                break;
            case X_CALL: {
                int rel = fetch32(&c);
                push(&c, c.pc);
                c.pc += rel;
                break;
            }
            case X_JMP:
                c.pc += fetch32(&c);
                break;
            case X_ESCAPE: {
                int op2 = fetch8(&c);
                if (op2 >= X_JCC && op2 < X_JCC + 16) {
                    int rel = fetch32(&c);
                    if (condition(&c, op2 - X_JCC)) { c.pc += rel; }
                } else if (op2 >= X_SETCC && op2 < X_SETCC + 16) {
                    struct modrm_s m = decodeModRM(&c);
                    setRM8(&c, &m, condition(&c, op2 - X_SETCC));
                } else if (op2 == X_IMUL_R_RM) {
                    struct modrm_s m = decodeModRM(&c);
                    c.reg[m.reg] = (int)((unsigned)c.reg[m.reg] * (unsigned)getRM(&c, &m));
                } else if (op2 == X_MOVZX_R_RM8) {
                    struct modrm_s m = decodeModRM(&c);
                    c.reg[m.reg] = getRM8(&c, &m);
                } else {
                    qFail(1, "-unsupported x86 opcode 0F %02X-", op2);
                }
                break;
            }
            default:
                if (op == X_PUSH_IMM32)                        { push(&c, fetch32(&c)); break; }
                if (op >= X_PUSH && op < X_PUSH + 8)           { push(&c, c.reg[op - X_PUSH]); break; }
                if (op >= X_POP && op < X_POP + 8)             { c.reg[op - X_POP] = pop(&c); break; }
                if (op >= X_MOV_R_IMM && op < X_MOV_R_IMM + 8) { c.reg[op - X_MOV_R_IMM] = fetch32(&c); break; }
                qFail(1, "-unsupported x86 opcode %02X at %d-", op, c.pc - 1);
        }
    }
}

// -------------------------------------------------
// Listing of the generated code. Writes a disassembly of the code to listing.txt when mc is run with -l.
// -------------------------------------------------

const char *labelAt(int address) {
    if (address == CODE_BASE) { return "start"; }
    for (int i = 0; i < numFunctions; i++) {
        if (functions[i].defined && functions[i].address == address) { return functions[i].name; }
    }
    return NULL;
}

const char *reg32Names[8] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi" };
const char *reg8Names[8] = { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" };
const char *ccNames[16] = { "o", "no", "b", "ae", "e", "ne", "be", "a", "s", "ns", "p", "np", "l", "ge", "le", "g" };

// Reads the ModRM byte at addr and any displacement after it. Sets reg and writes the r/m operand to out.
// byteRegs selects 8-bit register names. memPrefix (such as "dword ") goes before a memory operand.
// Returns the number of bytes used.
int disassembleModRM(int addr, int *reg, char *out, int byteRegs, const char *memPrefix) {
    int b = *MEM_AT(addr);
    int mod = b >> 6, rm = b & 7;
    *reg = (b >> 3) & 7;
    if (mod == 3) {
        strcpy(out, byteRegs ? reg8Names[rm] : reg32Names[rm]);
        return 1;
    }
    qFail(rm == ESP, "-unsupported x86 addressing: SIB-");
    qFail(mod == 1, "-unsupported x86 addressing: disp8-");
    if (mod == 0 && rm == EBP) {
        sprintf(out, "%s[0x%X]", memPrefix, (unsigned)loadInt(addr + 1));
        return 5;
    }
    if (mod == 0) {
        sprintf(out, "%s[%s]", memPrefix, reg32Names[rm]);
        return 1;
    }
    int disp = loadInt(addr + 1);
    sprintf(out, "%s[%s%c%d]", memPrefix, reg32Names[rm], disp < 0 ? '-' : '+', disp < 0 ? -disp : disp);
    return 5;
}

// A jump or call target is shown as the function name if one starts there, otherwise as an address.
void targetText(char *out, int target) {
    const char *label = labelAt(target);
    if (label) { strcpy(out, label); } else { sprintf(out, "%08X", (unsigned)target); }
}

// Writes the two-byte instruction (0F xx) at addr and returns its length.
int disassembleEscape(int addr, char *text) {
    int op2 = *MEM_AT(addr + 1);
    int reg, n, rel;
    char rm[64], target[32];
    if (op2 >= X_JCC && op2 < X_JCC + 16) {
        rel = loadInt(addr + 2);
        targetText(target, addr + 6 + rel);
        sprintf(text, "j%s %s", ccNames[op2 - X_JCC], target);
        return 6;
    }
    if (op2 >= X_SETCC && op2 < X_SETCC + 16) {
        n = disassembleModRM(addr + 2, &reg, rm, 1, "byte ");
        sprintf(text, "set%s %s", ccNames[op2 - X_SETCC], rm);
        return 2 + n;
    }
    if (op2 == X_IMUL_R_RM) {
        n = disassembleModRM(addr + 2, &reg, rm, 0, "");
        sprintf(text, "imul %s, %s", reg32Names[reg], rm);
        return 2 + n;
    }
    if (op2 == X_MOVZX_R_RM8) {
        n = disassembleModRM(addr + 2, &reg, rm, 1, "byte ");
        sprintf(text, "movzx %s, %s", reg32Names[reg], rm);
        return 2 + n;
    }
    sprintf(text, "db 0x0F");
    return 1;
}

// Writes the instruction at addr to text and returns its length. Bytes outside the subset are shown as db.
int disassemble(int addr, char *text) {
    int op = *MEM_AT(addr);
    int reg, n, imm, rel;
    char rm[64], target[32];
    if (op >= X_PUSH && op < X_PUSH + 8) { sprintf(text, "push %s", reg32Names[op - X_PUSH]); return 1; }
    if (op >= X_POP && op < X_POP + 8) { sprintf(text, "pop %s", reg32Names[op - X_POP]); return 1; }
    if (op >= X_MOV_R_IMM && op < X_MOV_R_IMM + 8) {
        sprintf(text, "mov %s, %d", reg32Names[op - X_MOV_R_IMM], loadInt(addr + 1));
        return 5;
    }
    switch (op) {
        case X_ADD_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "add %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_SUB_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "sub %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_AND_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "and %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_OR_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "or %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_XOR_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "xor %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_CMP_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "cmp %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_TEST_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "test %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_MOV_RM8_R8:
            n = disassembleModRM(addr + 1, &reg, rm, 1, "");
            sprintf(text, "mov %s, %s", rm, reg8Names[reg]);
            return 1 + n;
        case X_MOV_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "mov %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_MOV_R_RM:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "mov %s, %s", reg32Names[reg], rm);
            return 1 + n;
        case X_CDQ:
            sprintf(text, "cdq");
            return 1;
        case X_GRP1_RM_IMM32:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "dword ");
            imm = loadInt(addr + 1 + n);
            if (reg == 0 || reg == 5 || reg == 7) {
                sprintf(text, "%s %s, %d", reg == 0 ? "add" : reg == 5 ? "sub" : "cmp", rm, imm);
                return 1 + n + 4;
            }
            break;
        case X_GRP5_RM:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "dword ");
            if (reg <= 1) {
                sprintf(text, "%s %s", reg == 0 ? "inc" : "dec", rm);
                return 1 + n;
            }
            break;
        case X_GRP4_RM8:
            n = disassembleModRM(addr + 1, &reg, rm, 1, "");
            if (reg <= 1) {
                sprintf(text, "%s %s", reg == 0 ? "inc" : "dec", rm);
                return 1 + n;
            }
            break;
        case X_GRP2_RM_IMM8:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "dword ");
            if (reg == 4) {
                sprintf(text, "shl %s, %d", rm, *MEM_AT(addr + 1 + n));
                return 2 + n;
            }
            break;
        case X_GRP2_RM_CL:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            if (reg == 4 || reg == 7) {
                sprintf(text, "%s %s, cl", reg == 4 ? "shl" : "sar", rm);
                return 1 + n;
            }
            break;
        case X_GRP3_RM:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "dword ");
            if (reg == 2) {
                sprintf(text, "not %s", rm);
                return 1 + n;
            }
            if (reg == 7) {
                sprintf(text, "idiv %s", rm);
                return 1 + n;
            }
            break;
        case X_RET:
            sprintf(text, "ret");
            return 1;
        case X_PUSH_IMM32:
            sprintf(text, "push %d", loadInt(addr + 1));
            return 5;
        case X_LEAVE:
            sprintf(text, "leave");
            return 1;
        case X_INT:
            sprintf(text, "int 0x%02X", *MEM_AT(addr + 1));
            return 2;
        case X_CALL:
            rel = loadInt(addr + 1);
            targetText(target, addr + 5 + rel);
            sprintf(text, "call %s", target);
            return 5;
        case X_JMP:
            rel = loadInt(addr + 1);
            targetText(target, addr + 5 + rel);
            sprintf(text, "jmp %s", target);
            return 5;
        case X_ESCAPE:
            return disassembleEscape(addr, text);
    }
    sprintf(text, "db 0x%02X", op);
    return 1;
}

// Each line shows an address, the instruction's bytes, and the instruction. A label line goes before each function.
void listing(void) {
    FILE *fp = fopen("listing.txt", "wt");
    qFail(!fp, "-cannot write listing.txt-");
    int p = CODE_BASE;
    while (p < here) {
        const char *label = labelAt(p);
        if (label) { fprintf(fp, "%s%s:\n", p > CODE_BASE ? "\n" : "", label); }
        char text[128], bytes[64] = "";
        int len = disassemble(p, text);
        for (int i = 0; i < len; i++) { sprintf(bytes + strlen(bytes), "%02X ", *MEM_AT(p + i)); }
        fprintf(fp, "%08X  %-30s %s\n", (unsigned)p, bytes, text);
        p += len;
    }
    fclose(fp);
}
