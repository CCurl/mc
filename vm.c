/*---------------------------------------------------------------------------*/
/* Virtual machine for tiny-c. */

#include "tiny-c.h"

// -------------------------------------------------
// external references
extern byte vm[MAX_CODE];
extern byte globalMem[MAX_GLOBAL_BYTES];
extern int loadInt(int addr);
extern void storeInt(int addr, int value);
extern void fail(char *err);

// -------------------------------------------------
short f2(byte *a) { return *(short *)a; }
int f4(byte *a) { return *(int *)a; }

void run() {
    /* Each frame's parameters and locals sit on st[] starting at frameBase. */
    int st[STACK_SIZE], sp = 0, frameBase = 1, returnSp = 0;
    byte *pc = vm;
    struct return_s returnStack[STACK_SIZE];

    again:
    switch (*pc++) {
        case  IFETCH: st[++sp] = loadInt(f4(pc)); pc += 4;
        ACASE ISTORE: storeInt(f4(pc), st[sp]); pc += 4;
        ACASE PLOAD : st[++sp] = st[frameBase + f4(pc)]; pc += 4;
        ACASE PSTORE: st[frameBase + f4(pc)] = st[sp]; pc += 4;
        ACASE LLOAD : st[++sp] = st[frameBase + f4(pc)]; pc += 4;
        ACASE LSTORE: st[frameBase + f4(pc)] = st[sp]; pc += 4;
        ACASE PAFETCH: st[sp] = loadInt(st[frameBase + f4(pc)] + st[sp]); pc += 4;
        ACASE PASTORE: storeInt(st[frameBase + f4(pc)] + st[sp-1], st[sp]);
                        st[sp-1] = st[sp]; --sp; pc += 4;
        ACASE IAFETCH: st[sp] = loadInt(f4(pc) + st[sp]); pc += 4;
        ACASE IASTORE: storeInt(f4(pc) + st[sp-1], st[sp]);
                        st[sp-1] = st[sp]; --sp; pc += 4;
        ACASE IP1   : st[++sp] = *(pc++);
        ACASE IP2   : st[++sp] = f2(pc); pc += 2;
        ACASE IP4   : st[++sp] = *(int *)pc; pc += 4;
        ACASE IDROP : --sp;
        ACASE IADD  : NOS += TOS; --sp;
        ACASE ISUB  : NOS -= TOS; --sp;
        ACASE IMUL  : NOS *= TOS; --sp;
        ACASE IDIV  : if (TOS == 0) { fail("-divide by zero-"); }
                      NOS /= TOS; --sp;
        ACASE IMOD  : if (TOS == 0) { fail("-modulo by zero-"); }
                      NOS %= TOS; --sp;
        ACASE ILT   : NOS =  (NOS<TOS)?1:0; --sp;
        ACASE IGT   : NOS =  (NOS>TOS)?1:0; --sp;
        ACASE IEQ   : NOS =  (NOS==TOS)?1:0; --sp;
        ACASE INE   : NOS =  (NOS!=TOS)?1:0; --sp;
        ACASE SYS   : { int op = *(pc++);
                        if (op == SYS_EMIT) { TOS = putchar((char)TOS); }
                        // more here later
                      }
        ACASE I2B   : TOS = (byte)TOS;
        ACASE BGET  : st[++sp] = globalMem[f4(pc)]; pc += 4;
        ACASE BSET  : globalMem[f4(pc)] = st[sp]; pc += 4;
        ACASE BAFETCH: st[sp] = globalMem[f4(pc) + st[sp]]; pc += 4;
        ACASE BASTORE: globalMem[f4(pc) + st[sp-1]] = st[sp];
                        st[sp-1] = st[sp]; --sp; pc += 4;
        ACASE PBAFETCH: st[sp] = globalMem[st[frameBase + f4(pc)] + st[sp]]; pc += 4;
        ACASE PBASTORE: globalMem[st[frameBase + f4(pc)] + st[sp-1]] = st[sp];
                        st[sp-1] = st[sp]; --sp; pc += 4;
        ACASE JMP   : pc += f4(pc);
        ACASE JZ    : if (st[sp--] == 0) pc += f4(pc); else pc += 4;
        ACASE JNZ   : if (st[sp--] != 0) pc += f4(pc); else pc += 4;
        ACASE LINC  : st[frameBase + f4(pc)]++; pc += 4;
        ACASE LDEC  : st[frameBase + f4(pc)]--; pc += 4;
        ACASE GINC  : storeInt(f4(pc), loadInt(f4(pc)) + 1); pc += 4;
        ACASE GDEC  : storeInt(f4(pc), loadInt(f4(pc)) - 1); pc += 4;
        ACASE ICALL : { int argc = f4(pc + 4);
                        int locals = f4(pc + 8);
                        if (returnSp >= STACK_SIZE) { fail("-call stack overflow-"); }
                        if (sp < argc || sp + locals >= STACK_SIZE) { fail("-call frame overflow-"); }
                        returnStack[returnSp].pc = pc + 12;
                        returnStack[returnSp].frameBase = frameBase;
                        returnSp++;
                        frameBase = sp - argc + 1;
                        sp += locals;
                        pc = vm + f4(pc);
                      }
        ACASE IRET : if (returnSp) {
                          st[frameBase] = TOS;
                          sp = frameBase;
                          returnSp--;
                          pc = returnStack[returnSp].pc;
                          frameBase = returnStack[returnSp].frameBase;
                      } else { return; }
        ACASE HALT  : return;
    }
}
