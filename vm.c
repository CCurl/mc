/*---------------------------------------------------------------------------*/
/* Virtual machine for tiny-c. */

#include "tiny-c.h"

// -------------------------------------------------
static int regIndex(byte *a) { return *(unsigned short *)a; }
static int f4(byte *a) { return *(int *)a; }

// The register named by the 2-byte operand at p, in the current frame.
#define REG(p) (st[frameBase + regIndex(p)])

int run() {
    /* Each frame's registers start at st[frameBase]. Register 0 of a callee is the caller's CALL register. */
    int st[STACK_SIZE], frameBase = 0, returnSp = 0;
    byte *pc = vm;
    struct return_s returnStack[STACK_SIZE];

    again:
    switch (*pc++) {
        case LDI  : REG(pc) = f4(pc + 2); pc += 6;
        ACASE MOV : REG(pc) = REG(pc + 2); pc += 4;
        ACASE IGET: REG(pc) = loadInt(f4(pc + 2)); pc += 6;
        ACASE ISET: storeInt(f4(pc + 2), REG(pc)); pc += 6;
        ACASE BGET: REG(pc) = globalMem[f4(pc + 2)]; pc += 6;
        ACASE BSET: globalMem[f4(pc + 2)] = REG(pc); pc += 6;
        ACASE IAGET : REG(pc) = loadInt(f4(pc + 2) + 4 * REG(pc + 6)); pc += 8;
        ACASE IASET : storeInt(f4(pc + 2) + 4 * REG(pc + 6), REG(pc)); pc += 8;
        ACASE BAGET : REG(pc) = globalMem[f4(pc + 2) + REG(pc + 6)]; pc += 8;
        ACASE BASET : globalMem[f4(pc + 2) + REG(pc + 6)] = REG(pc); pc += 8;
        ACASE PIAGET: REG(pc) = loadInt(REG(pc + 2) + 4 * REG(pc + 4)); pc += 6;
        ACASE PIASET: storeInt(REG(pc + 2) + 4 * REG(pc + 4), REG(pc)); pc += 6;
        ACASE PBAGET: REG(pc) = globalMem[REG(pc + 2) + REG(pc + 4)]; pc += 6;
        ACASE PBASET: globalMem[REG(pc + 2) + REG(pc + 4)] = REG(pc); pc += 6;
        ACASE I2B   : REG(pc) = (byte)REG(pc + 2); pc += 4;
        ACASE IADDI : REG(pc) = REG(pc + 2) + f4(pc + 4); pc += 8;
        ACASE IADD  : REG(pc) = REG(pc + 2) + REG(pc + 4); pc += 6;
        ACASE ISUB  : REG(pc) = REG(pc + 2) - REG(pc + 4); pc += 6;
        ACASE IMUL  : REG(pc) = REG(pc + 2) * REG(pc + 4); pc += 6;
        ACASE IDIV  : if (REG(pc + 4) == 0) { fail("-divide by zero-"); }
                      REG(pc) = REG(pc + 2) / REG(pc + 4); pc += 6;
        ACASE IMOD  : if (REG(pc + 4) == 0) { fail("-modulo by zero-"); }
                      REG(pc) = REG(pc + 2) % REG(pc + 4); pc += 6;
        ACASE ILT   : REG(pc) = REG(pc + 2) < REG(pc + 4); pc += 6;
        ACASE IGT   : REG(pc) = REG(pc + 2) > REG(pc + 4); pc += 6;
        ACASE ILE   : REG(pc) = REG(pc + 2) <= REG(pc + 4); pc += 6;
        ACASE IGE   : REG(pc) = REG(pc + 2) >= REG(pc + 4); pc += 6;
        ACASE IEQ   : REG(pc) = REG(pc + 2) == REG(pc + 4); pc += 6;
        ACASE INE   : REG(pc) = REG(pc + 2) != REG(pc + 4); pc += 6;
        ACASE EMIT  : REG(pc) = putchar((char)REG(pc + 2)); pc += 4;
        ACASE JMP   : pc += f4(pc);
        ACASE JZ    : if (REG(pc) == 0) pc += 2 + f4(pc + 2); else pc += 6;
        ACASE JNZ   : if (REG(pc) != 0) pc += 2 + f4(pc + 2); else pc += 6;
        ACASE JLT   : if (REG(pc) <  REG(pc + 2)) pc += 4 + f4(pc + 4); else pc += 8;
        ACASE JGT   : if (REG(pc) >  REG(pc + 2)) pc += 4 + f4(pc + 4); else pc += 8;
        ACASE JEQ   : if (REG(pc) == REG(pc + 2)) pc += 4 + f4(pc + 4); else pc += 8;
        ACASE JNE   : if (REG(pc) != REG(pc + 2)) pc += 4 + f4(pc + 4); else pc += 8;
        ACASE JGE   : if (REG(pc) >= REG(pc + 2)) pc += 4 + f4(pc + 4); else pc += 8;
        ACASE JLE   : if (REG(pc) <= REG(pc + 2)) pc += 4 + f4(pc + 4); else pc += 8;
        ACASE CALL  : {
                        int newBase = frameBase + regIndex(pc + 4);
                        if (returnSp >= STACK_SIZE) { fail("-call stack overflow-"); }
                        if (newBase + f4(pc + 6) > STACK_SIZE) { fail("-call frame overflow-"); }
                        returnStack[returnSp].pc = pc + 10;
                        returnStack[returnSp].frameBase = frameBase;
                        returnSp++;
                        frameBase = newBase;
                        pc = vm + f4(pc);
                      }
        ACASE RET   : st[frameBase] = REG(pc); goto doReturn;
        ACASE RET0  : st[frameBase] = 0;
        doReturn:
                      if (returnSp) {
                          returnSp--;
                          pc = returnStack[returnSp].pc;
                          frameBase = returnStack[returnSp].frameBase;
                      } else { return st[0]; }
        ACASE HALT  : return st[0];
    }
    return st[0];
}

