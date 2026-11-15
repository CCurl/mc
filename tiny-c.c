// ---------------------------------------------------------------------------
// Contains code from Marc Feeley's tinyc.c, with permission.
// http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c
// Heavily modified by Chris Curl to add more C-like support.
// See the README for the EBNF grammar, and unit-tests.tc for an example.
// ---------------------------------------------------------------------------

#include "tiny-c.h"

// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    const char *source = NULL;
    int dSpec = 0, oSpec = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) { dSpec = 1; }
        else if (strcmp(argv[i], "-o") == 0) { oSpec = 1; }
        else { source = argv[i]; }
    }
    openSource(source ? source : "unit-tests.tc");
    c(program()); // The compiler
    if (dSpec) { listing(); }
    if (oSpec) { saveImage("a.img"); }
    return run();
}
