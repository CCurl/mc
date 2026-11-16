// ---------------------------------------------------------------------------
// Contains code from Marc Feeley's tinyc.c, with permission.
// http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c
// Heavily modified by Chris Curl to add more C-like support.
// See the README for the EBNF grammar, and unit-tests.mc for an example.
// ---------------------------------------------------------------------------

#include "min-c.h"

// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    const char *source = NULL;
    int lSpec = 0, oSpec = 0, rSpec = 1, hSpec = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) { lSpec = 1; }
        else if (strcmp(argv[i], "-o") == 0) { oSpec = 1; }
        else if (strcmp(argv[i], "-r") == 0) { rSpec = 1; }
        else if (strcmp(argv[i], "-?") == 0) { hSpec = 1; }
        else { source = argv[i]; }
    }
    if ((!source) || hSpec) {
        printf("usage: %s [-l] [-o] [-r] [-?] <source>\n", argv[0]);
        printf("  compiles the specified source file\n");
        printf("  -l: create 'listing.txt' file\n");
        printf("  -o: output compiled image to 'a.img'\n");
        printf("  -r: run after compile (default on)\n");
        printf("  -?: show this help message\n");
        printf("  defaults: -l: off, -o: off, -r: on\n");
        return 1;
    }
    if (!openSource(source)) { return 1; }
    c(program()); // The compiler
    if (lSpec) { listing(); }
    if (oSpec) { saveImage("a.img"); }
    if (rSpec) { return run(); }
    return 0;
}
