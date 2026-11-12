/*---------------------------------------------------------------------------*/
/* Virtual machine for tiny-c. */

#include "tiny-c.h"

struct return_s { code *pc; int frameBase; };

short f2(code *a) { return *(short *)a; }
int f4(code *a) { return *(int *)a; }

void run() {
    /* Each frame's parameters and locals sit on st[] starting at frameBase. */
    int st[1000], sp = 0, frameBase = 1, returnSp = 0;
    code *pc = vm;
    struct return_s returnStack[1000];

    again:
    switch (*pc++) {
        case  IFETCH: st[++sp] = globals[f4(pc)]; pc += 4;
        ACASE ISTORE: globals[f4(pc)] = st[sp]; pc += 4;
        ACASE PLOAD : st[++sp] = st[frameBase + f4(pc)]; pc += 4;
        ACASE PSTORE: st[frameBase + f4(pc)] = st[sp]; pc += 4;
        ACASE LLOAD : st[++sp] = st[frameBase + f4(pc)]; pc += 4;
        ACASE LSTORE: st[frameBase + f4(pc)] = st[sp]; pc += 4;
        ACASE PAFETCH: st[sp] = globals[st[frameBase + f4(pc)] + st[sp]]; pc += 4;
        ACASE PASTORE: globals[st[frameBase + f4(pc)] + st[sp-1]] = st[sp];
                        st[sp-1] = st[sp]; --sp; pc += 4;
        ACASE IAFETCH: st[sp] = globals[f4(pc) + st[sp]]; pc += 4;
        ACASE IASTORE: globals[f4(pc) + st[sp-1]] = st[sp];
                        st[sp-1] = st[sp]; --sp; pc += 4;
        ACASE IP1   : st[++sp] = *(pc++);
        ACASE IP2   : st[++sp] = f2(pc); pc += 2;
        ACASE IP4   : st[++sp] = *(int *)pc; pc += 4;
        ACASE IDROP : --sp;
        ACASE IADD  : NOS += TOS; --sp;
        ACASE ISUB  : NOS -= TOS; --sp;
        ACASE IMUL  : NOS *= TOS; --sp;
        ACASE IDIV  : if (TOS == 0) { error("-divide by zero-"); }
                      NOS /= TOS; --sp;
        ACASE IMOD  : if (TOS == 0) { error("-modulo by zero-"); }
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
        ACASE BGET  : st[++sp] = globalBytes[f4(pc)]; pc += 4;
        ACASE BSET  : globalBytes[f4(pc)] = st[sp]; pc += 4;
        ACASE BAFETCH: st[sp] = globalBytes[f4(pc) + st[sp]]; pc += 4;
        ACASE BASTORE: globalBytes[f4(pc) + st[sp-1]] = st[sp];
                        st[sp-1] = st[sp]; --sp; pc += 4;
        ACASE PBAFETCH: st[sp] = globalBytes[st[frameBase + f4(pc)] + st[sp]]; pc += 4;
        ACASE PBASTORE: globalBytes[st[frameBase + f4(pc)] + st[sp-1]] = st[sp];
                        st[sp-1] = st[sp]; --sp; pc += 4;
        ACASE JMP   : pc += f4(pc);
        ACASE JZ    : if (st[sp--] == 0) pc += f4(pc); else pc += 4;
        ACASE JNZ   : if (st[sp--] != 0) pc += f4(pc); else pc += 4;
        ACASE LINC  : st[frameBase + f4(pc)]++; pc += 4;
        ACASE LDEC  : st[frameBase + f4(pc)]--; pc += 4;
        ACASE GINC  : globals[f4(pc)]++; pc += 4;
        ACASE GDEC  : globals[f4(pc)]--; pc += 4;
        ACASE ICALL : { int argc = f4(pc + 4);
                        int locals = f4(pc + 8);
                        if (returnSp >= 1000) { error("-call stack overflow-"); }
                        if (sp < argc || sp + locals >= 1000) { error("-call frame overflow-"); }
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
