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
    int lSpec = 0, rSpec = 0, hSpec = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-?") == 0) { hSpec = 1; }
        else if (strcmp(argv[i], "-l") == 0) { lSpec = 1; }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) { exeName = argv[++i]; }
        else if (strcmp(argv[i], "-r") == 0) { rSpec = 1; }
        else { source = argv[i]; }
    }

    // Do not run if an executable is specified
    if (exeName) { rSpec = 0; }

    if ((!source) || hSpec) {
        printf("usage: %s [-l] [-o <exe>] [-r] [-?] <source>\n", argv[0]);
        printf("  compiles the specified source file\n");
        printf("  -l: create 'listing.txt' file\n");
        printf("  -o <exe>: output compiled executable to <exe>\n");
        printf("  -r: run in vm after compile (default no)\n");
        printf("  -?: show this help message\n");
        return 1;
    }
    if (!openSource(source)) { return 1; }
    c(program()); // The compiler
    if (lSpec) { listing(); }
    if (exeName) { saveExecutable(exeName); }
    if (rSpec) { return run(); }
    return 0;
}
