// ---------------------------------------------------------------------------
// Contains code from Marc Feeley's tinyc.c, with permission.
// http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c
// Heavily modified by Chris Curl to add more C-like support.
// See the README for the EBNF grammar, and unit-tests.tc for an example.
// ---------------------------------------------------------------------------

#include "tiny-c.h"

// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc > 1) { openSource(argv[1]); }
    else { openSource("unit-tests.tc"); }
    c(program()); // The compiler
    saveImage("out.img");
    return run();
}
