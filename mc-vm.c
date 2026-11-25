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

// Runs the int 0x80 system calls that the code generator uses: exit (1), read (3), write (4), open (5), close (6).
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
                if (op >= X_PUSH && op < X_PUSH + 8)           { push(&c, c.reg[op - X_PUSH]); break; }
                if (op >= X_POP && op < X_POP + 8)             { c.reg[op - X_POP] = pop(&c); break; }
                if (op >= X_MOV_R_IMM && op < X_MOV_R_IMM + 8) { c.reg[op - X_MOV_R_IMM] = fetch32(&c); break; }
                qFail(1, "-unsupported x86 opcode %02X at %d-", op, c.pc - 1);
        }
    }
}
