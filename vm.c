/*---------------------------------------------------------------------------*/
/* Virtual machine for min-c. */

#include "min-c.h"

// -------------------------------------------------
static int regIndex(byte *a) { return *(unsigned short *)a; }
static int f4(byte *a) { return *(int *)a; }

// The register named by the 2-byte operand at p, in the current frame.
#define REG(p) (st[frameBase + regIndex(p)])
#define SZ16  2
#define SZ32  4
#define SZ48  6
#define SZ64  8
#define SZ80 10

int run() {
    /* Each frame's registers start at st[frameBase]. Register 0 of a callee is the caller's CALL register. */
    int st[STACK_SIZE], frameBase = 0, returnSp = 0;
    byte *pc = vm;
    struct return_s returnStack[STACK_SIZE];

    again:
    switch (*pc++) {
        case LDI  : REG(pc) = f4(pc+SZ16); pc += SZ48;
        ACASE MOV : REG(pc) = REG(pc+SZ16); pc += SZ32;
        ACASE IGET: REG(pc) = loadInt(f4(pc+SZ16)); pc += SZ48;
        ACASE ISET: storeInt(f4(pc+SZ16), REG(pc)); pc += SZ48;
        ACASE BGET: REG(pc) = globalMem[f4(pc+SZ16)]; pc += SZ48;
        ACASE BSET: globalMem[f4(pc+SZ16)] = REG(pc); pc += SZ48;
        ACASE IAGET : REG(pc) = loadInt(f4(pc+SZ16) + 4 * REG(pc+SZ48)); pc += SZ64;
        ACASE IASET : storeInt(f4(pc+SZ16) + 4 * REG(pc+SZ48), REG(pc)); pc += SZ64;
        ACASE BAGET : REG(pc) = globalMem[f4(pc+SZ16) + REG(pc+SZ48)]; pc += SZ64;
        ACASE BASET : globalMem[f4(pc+SZ16) + REG(pc+SZ48)] = REG(pc); pc += SZ64;
        ACASE PIAGET: REG(pc) = loadInt(REG(pc+SZ16) + 4 * REG(pc+SZ32)); pc += SZ48;
        ACASE PIASET: storeInt(REG(pc+SZ16) + 4 * REG(pc+SZ32), REG(pc)); pc += SZ48;
        ACASE PBAGET: REG(pc) = globalMem[REG(pc+SZ16) + REG(pc+SZ32)]; pc += SZ48;
        ACASE PBASET: globalMem[REG(pc+SZ16) + REG(pc+SZ32)] = REG(pc); pc += SZ48;
        ACASE I2B   : REG(pc) = (byte)REG(pc+SZ16); pc += SZ32;
        ACASE IADDI : REG(pc) = REG(pc+SZ16) + f4(pc+SZ32); pc += SZ64;
        ACASE IINC1 : REG(pc)++; pc += SZ16;
        ACASE IDEC1 : REG(pc)--; pc += SZ16;
        ACASE IINC  : REG(pc) = loadInt(f4(pc+SZ16)) + f4(pc+SZ48); storeInt(f4(pc+SZ16), REG(pc)); pc += SZ80;
        ACASE IINCP : REG(pc) = loadInt(f4(pc+SZ16)); storeInt(f4(pc+SZ16), REG(pc) + f4(pc+SZ48)); pc += SZ80;
        ACASE BINC  : REG(pc) = (byte)(globalMem[f4(pc+SZ16)] + f4(pc+SZ48)); globalMem[f4(pc+SZ16)] = REG(pc); pc += SZ80;
        ACASE BINCP : REG(pc) = globalMem[f4(pc+SZ16)]; globalMem[f4(pc+SZ16)] = REG(pc) + f4(pc+SZ48); pc += SZ80;
        ACASE IADD  : REG(pc) = REG(pc+SZ16) + REG(pc+SZ32); pc += SZ48;
        ACASE ISUB  : REG(pc) = REG(pc+SZ16) - REG(pc+SZ32); pc += SZ48;
        ACASE IMUL  : REG(pc) = REG(pc+SZ16) * REG(pc+SZ32); pc += SZ48;
        ACASE IDIV  : if (REG(pc+SZ32) == 0) { fail("-divide by zero-"); }
                      REG(pc) = REG(pc+SZ16) / REG(pc+SZ32); pc += SZ48;
        ACASE IMOD  : if (REG(pc+SZ32) == 0) { fail("-modulo by zero-"); }
                      REG(pc) = REG(pc+SZ16) %  REG(pc+SZ32); pc += SZ48;
        ACASE ILT   : REG(pc) = REG(pc+SZ16) <  REG(pc+SZ32); pc += SZ48;
        ACASE IGT   : REG(pc) = REG(pc+SZ16) >  REG(pc+SZ32); pc += SZ48;
        ACASE ILE   : REG(pc) = REG(pc+SZ16) <= REG(pc+SZ32); pc += SZ48;
        ACASE IGE   : REG(pc) = REG(pc+SZ16) >= REG(pc+SZ32); pc += SZ48;
        ACASE IEQ   : REG(pc) = REG(pc+SZ16) == REG(pc+SZ32); pc += SZ48;
        ACASE INE   : REG(pc) = REG(pc+SZ16) != REG(pc+SZ32); pc += SZ48;
        ACASE EMIT  : REG(pc) = putchar((char)REG(pc+SZ16)) ; pc += SZ32;
        ACASE JMP   : pc += f4(pc);
        ACASE JZ    : if (REG(pc) == 0) pc += 2 + f4(pc+SZ16); else pc += SZ48;
        ACASE JNZ   : if (REG(pc) != 0) pc += 2 + f4(pc+SZ16); else pc += SZ48;
        ACASE JLT   : if (REG(pc) <  REG(pc+SZ16)) pc += SZ32 + f4(pc+SZ32); else pc += SZ64;
        ACASE JGT   : if (REG(pc) >  REG(pc+SZ16)) pc += SZ32 + f4(pc+SZ32); else pc += SZ64;
        ACASE JEQ   : if (REG(pc) == REG(pc+SZ16)) pc += SZ32 + f4(pc+SZ32); else pc += SZ64;
        ACASE JNE   : if (REG(pc) != REG(pc+SZ16)) pc += SZ32 + f4(pc+SZ32); else pc += SZ64;
        ACASE JGE   : if (REG(pc) >= REG(pc+SZ16)) pc += SZ32 + f4(pc+SZ32); else pc += SZ64;
        ACASE JLE   : if (REG(pc) <= REG(pc+SZ16)) pc += SZ32 + f4(pc+SZ32); else pc += SZ64;
        ACASE CALL  : {
                        int newBase = frameBase + regIndex(pc+SZ32);
                        if (returnSp >= STACK_SIZE) { fail("-call stack overflow-"); }
                        if (newBase + f4(pc+SZ48) > STACK_SIZE) { fail("-call frame overflow-"); }
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

