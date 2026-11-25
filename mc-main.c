// ---------------------------------------------------------------------------
// Contains code from Marc Feeley's tinyc.c, with permission.
// http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c
// Heavily modified by Chris Curl to add more C-like support.
// See the README for the EBNF grammar, and unit-tests.mc for an example.
// ---------------------------------------------------------------------------

#include "mc.h"

// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    const char *source = NULL, *exeName = NULL;
    int lSpec = 0, oSpec = 0, rSpec = 1, hSpec = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-?") == 0) { hSpec = 1; }
        else if (strcmp(argv[i], "-l") == 0) { lSpec = 1; }
        else if (strcmp(argv[i], "-n") == 0) { rSpec = 0; }
        else if (strcmp(argv[i], "-o") == 0) { oSpec = 1; }
        else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) { exeName = argv[++i]; }
        else { source = argv[i]; }
    }
    if ((!source) || hSpec) {
        printf("usage: %s [-l] [-o] [-e <exe>] [-n] [-?] <source>\n", argv[0]);
        printf("  compiles the specified source file\n");
        printf("  -l: create 'listing.txt' file\n");
        printf("  -o: output compiled image to 'a.img'\n");
        printf("  -e <exe>: write a Linux 32-bit executable to <exe>\n");
        printf("  -n: do not run after compile (default run)\n");
        printf("  -?: show this help message\n");
        return 1;
    }
    if (!openSource(source)) { return 1; }
    c(program()); // The compiler
    if (lSpec) { listing(); }
    if (oSpec) { saveImage("a.img"); }
    if (exeName) { saveExecutable(exeName); }
    if (rSpec) { return run(); }
    return 0;
}
