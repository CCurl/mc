//---------------------------------------------------------------------------
// Disassembler for the tiny C virtual machine bytecode
// NOTE: this is unused and out of date!

/*
void dis() {
    byte *pc = &vm[0];
    FILE *fp = fopen("list.txt", "wt");
  again:
    if (here <= pc) { fclose(fp); return; }
    int p = (int)(pc-&vm[0]) + 1;
    fprintf(fp,"\n%04d: %02d ; ", p, *pc);
    switch (*pc++) {
        case  IFETCH: fprintf(fp,"fetch %d", f2((byte*)pc)); pc +=2;
        ACASE ISTORE: fprintf(fp,"store %d", f2((byte*)pc)); pc +=2;
        ACASE IP1   : fprintf(fp,"lit1 %d", *(pc++));
        ACASE IP2   : fprintf(fp,"lit2 %d", f2((byte*)pc)); pc +=2;
        ACASE IP4   : fprintf(fp,"lit4 %d", f4((byte*)pc)); pc +=4;
        ACASE IDROP : fprintf(fp,"drop");
        ACASE IADD  : fprintf(fp,"add");
        ACASE ISUB  : fprintf(fp,"sub");
        ACASE IMUL  : fprintf(fp,"mul");
        ACASE IDIV  : fprintf(fp,"div");
        ACASE ILT   : fprintf(fp,"lt");
        ACASE IGT   : fprintf(fp,"gt");
        ACASE IEQ   : fprintf(fp,"eq");
        ACASE SYS   : {
                        int operation = *(pc++);
                        if (operation == SYS_PUTCHAR) { fprintf(fp,"sys emit"); }
                        else { fprintf(fp,"sys %d", operation); }
                      }
        ACASE JMP   : fprintf(fp,"jmp %d", p+1+(*pc++));
        ACASE JZ    : fprintf(fp,"jz %d",  p+1+(*pc++));
        ACASE JNZ   : fprintf(fp,"jnz %d", p+1+(*pc++));
        ACASE ICALL : fprintf(fp,"call %d %d %d", f2((byte*)pc), f2((byte*)pc + 2),
                              f2((byte*)pc + 4)); pc += 6;
        ACASE IRET  : fprintf(fp,"ret");
        ACASE HALT  : fprintf(fp,"halt");
        ACASE IAFETCH: fprintf(fp,"array-fetch %d", f2((byte*)pc)); pc += 2;
        ACASE IASTORE: fprintf(fp,"array-store %d", f2((byte*)pc)); pc += 2;
        ACASE PLOAD : fprintf(fp,"parameter-fetch %d", f2((byte*)pc)); pc += 2;
        ACASE PSTORE: fprintf(fp,"parameter-store %d", f2((byte*)pc)); pc += 2;
        ACASE PAFETCH: fprintf(fp,"parameter-array-fetch %d", f2((byte*)pc)); pc += 2;
        ACASE PASTORE: fprintf(fp,"parameter-array-store %d", f2((byte*)pc)); pc += 2;
        ACASE LLOAD : fprintf(fp,"local-fetch %d", f2((byte*)pc)); pc += 2;
        ACASE LSTORE: fprintf(fp,"local-store %d", f2((byte*)pc)); pc += 2;
    }
    goto again;
}
*/