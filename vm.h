/*---------------------------------------------------------------------------*/
/* Virtual machine for tiny-c. */

#include "tiny-c.h"

struct return_s { code *pc; int frame_base; };

short f2(code *a) { return *(short *)a; }

void run() {
    /* Each frame's parameters and locals sit on st[] starting at frame_base. */
    int st[1000], sp = 0, frame_base = 1, return_sp = 0;
    code *pc = vm;
    struct return_s return_stack[1000];

    again:
    switch (*pc++) {
        case  IFETCH: st[++sp] = globals[f2(pc)]; pc +=2;
        ACASE ISTORE: globals[f2(pc)] = st[sp]; pc += 2;
        ACASE PLOAD : st[++sp] = st[frame_base + f2(pc)]; pc += 2;
        ACASE PSTORE: st[frame_base + f2(pc)] = st[sp]; pc += 2;
        ACASE LLOAD : st[++sp] = st[frame_base + f2(pc)]; pc += 2;
        ACASE LSTORE: st[frame_base + f2(pc)] = st[sp]; pc += 2;
        ACASE PAFETCH: st[sp] = globals[st[frame_base + f2(pc)] + st[sp]]; pc += 2;
        ACASE PASTORE: globals[st[frame_base + f2(pc)] + st[sp-1]] = st[sp];
                        st[sp-1] = st[sp]; --sp; pc += 2;
        ACASE IAFETCH: st[sp] = globals[f2(pc) + st[sp]]; pc += 2;
        ACASE IASTORE: globals[f2(pc) + st[sp-1]] = st[sp];
                        st[sp-1] = st[sp]; --sp; pc += 2;
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
        ACASE IC2C  : TOS = (signed char)TOS;
        ACASE JMP   : pc += (short)f2(pc);
        ACASE JZ    : if (st[sp--] == 0) pc += (short)f2(pc); else pc += 2;
        ACASE JNZ   : if (st[sp--] != 0) pc += (short)f2(pc); else pc += 2;
        ACASE ICALL : { int argc = f2(pc + 2);
                        int locals = f2(pc + 4);
                        if (return_sp >= 1000) { error("-call stack overflow-"); }
                        if (sp < argc || sp + locals >= 1000) { error("-call frame overflow-"); }
                        return_stack[return_sp].pc = pc + 6;
                        return_stack[return_sp].frame_base = frame_base;
                        return_sp++;
                        frame_base = sp - argc + 1;
                        sp += locals;
                        pc = vm + f2(pc);
                      }
        ACASE IRET : if (return_sp) {
                          st[frame_base] = TOS;
                          sp = frame_base;
                          return_sp--;
                          pc = return_stack[return_sp].pc;
                          frame_base = return_stack[return_sp].frame_base;
                      } else { return; }
        ACASE HALT  : return;
    }
}
