//---------------------------------------------------------------------------
// Listing of the generated code. Compiled only when LISTING is defined
// (make LISTING=1). Writes a listing of the code to listing.txt.

#include "tiny-c.h"

#ifdef LISTING

static int readReg(int p) { return *(unsigned short *)(vm + p); }
static int readInt(int p) { return *(int *)(vm + p); }

static void printLabels(FILE *fp, int p) {
    for (int i = 0; i < numFunctions; i++) {
        if (functions[i].defined && functions[i].address == p) { fprintf(fp, "\n%s:\n", functions[i].name); }
    }
}

static const char *functionAt(int address) {
    for (int i = 0; i < numFunctions; i++) {
        if (functions[i].defined && functions[i].address == address) { return functions[i].name; }
    }
    return NULL;
}

void listing() {
    FILE *fp = fopen("listing.txt", "wt");
    if (!fp) { fail("-cannot write listing.txt-"); }
    int p = 0;
    while (p < here) {
        printLabels(fp, p);
        int op = vm[p];
        fprintf(fp, "%6d  %-6s", p, opcodeNames[op]);
        int q = p + 1;
        for (const char *k = opcodeKinds[op]; *k; k++) {
            if (*k == 'r') {
                fprintf(fp, " r%d", readReg(q));
                q += 2;
            } else if (*k == 't') {
                fprintf(fp, " ->%d", q + readInt(q));
                q += 4;
            } else {
                int v = readInt(q);
                const char *name = (op == CALL && k == opcodeKinds[op]) ? functionAt(v) : NULL;
                if (name) { fprintf(fp, " %s", name); } else { fprintf(fp, " %d", v); }
                q += 4;
            }
        }
        fprintf(fp, "\n");
        p += opcodeLength(op);
    }
    fclose(fp);
}

#endif
