//---------------------------------------------------------------------------
// Listing of the generated code. Writes a disassembly of the code to listing.txt when mc is run with -l.

#include "mc.h"

static const char *labelAt(int address) {
    if (address == CODE_BASE) { return "start"; }
    for (int i = 0; i < numFunctions; i++) {
        if (functions[i].defined && functions[i].address == address) { return functions[i].name; }
    }
    return NULL;
}

static const char *reg32Names[8] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi" };
static const char *reg8Names[8] = { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" };
static const char *ccNames[16] = { "o", "no", "b", "ae", "e", "ne", "be", "a", "s", "ns", "p", "np", "l", "ge", "le", "g" };

static int readInt(int addr) {
    int value;
    memcpy(&value, mem + addr, 4);
    return value;
}

// Reads the ModRM byte at addr and any displacement after it. Sets reg and writes the r/m operand to out.
// byteRegs selects 8-bit register names. memPrefix (such as "dword ") goes before a memory operand.
// Returns the number of bytes used.
static int disassembleModRM(int addr, int *reg, char *out, int byteRegs, const char *memPrefix) {
    int b = mem[addr];
    int mod = b >> 6, rm = b & 7;
    *reg = (b >> 3) & 7;
    if (mod == 3) {
        strcpy(out, byteRegs ? reg8Names[rm] : reg32Names[rm]);
        return 1;
    }
    qFail(rm == ESP, "-unsupported x86 addressing: SIB-");
    qFail(mod == 1, "-unsupported x86 addressing: disp8-");
    if (mod == 0 && rm == EBP) {
        sprintf(out, "%s[0x%X]", memPrefix, (unsigned)readInt(addr + 1));
        return 5;
    }
    if (mod == 0) {
        sprintf(out, "%s[%s]", memPrefix, reg32Names[rm]);
        return 1;
    }
    int disp = readInt(addr + 1);
    sprintf(out, "%s[%s%c%d]", memPrefix, reg32Names[rm], disp < 0 ? '-' : '+', disp < 0 ? -disp : disp);
    return 5;
}

// A jump or call target is shown as the function name if one starts there, otherwise as an address.
static void targetText(char *out, int target) {
    const char *label = labelAt(target);
    if (label) { strcpy(out, label); } else { sprintf(out, "%08X", (unsigned)target); }
}

// Writes the two-byte instruction (0F xx) at addr and returns its length.
static int disassembleEscape(int addr, char *text) {
    int op2 = mem[addr + 1];
    int reg, n, rel;
    char rm[64], target[32];
    if (op2 >= X_JCC && op2 < X_JCC + 16) {
        rel = readInt(addr + 2);
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
static int disassemble(int addr, char *text) {
    int op = mem[addr];
    int reg, n, imm, rel;
    char rm[64], target[32];
    if (op >= X_PUSH && op < X_PUSH + 8) { sprintf(text, "push %s", reg32Names[op - X_PUSH]); return 1; }
    if (op >= X_POP && op < X_POP + 8) { sprintf(text, "pop %s", reg32Names[op - X_POP]); return 1; }
    if (op >= X_MOV_R_IMM && op < X_MOV_R_IMM + 8) {
        sprintf(text, "mov %s, %d", reg32Names[op - X_MOV_R_IMM], readInt(addr + 1));
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
            imm = readInt(addr + 1 + n);
            if (reg == 0 || reg == 5 || reg == 7) {
                sprintf(text, "%s %s, %d", reg == 0 ? "add" : reg == 5 ? "sub" : "cmp", rm, imm);
                return 1 + n + 4;
            }
            break;
        case X_GRP2_RM_IMM8:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "dword ");
            if (reg == 4) {
                sprintf(text, "shl %s, %d", rm, mem[addr + 1 + n]);
                return 2 + n;
            }
            break;
        case X_GRP3_RM:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "dword ");
            if (reg == 7) {
                sprintf(text, "idiv %s", rm);
                return 1 + n;
            }
            break;
        case X_RET:
            sprintf(text, "ret");
            return 1;
        case X_LEAVE:
            sprintf(text, "leave");
            return 1;
        case X_INT:
            sprintf(text, "int 0x%02X", mem[addr + 1]);
            return 2;
        case X_CALL:
            rel = readInt(addr + 1);
            targetText(target, addr + 5 + rel);
            sprintf(text, "call %s", target);
            return 5;
        case X_JMP:
            rel = readInt(addr + 1);
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
        for (int i = 0; i < len; i++) { sprintf(bytes + strlen(bytes), "%02X ", mem[p + i]); }
        fprintf(fp, "%08X  %-30s %s\n", (unsigned)p, bytes, text);
        p += len;
    }
    fclose(fp);
}
