/* Contains code from Marc Feeley's tinyc.c, with permission. */
/* http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c */
/* Heavily modified by Chris Curl to add more C-like support */
/* See the README for the EBNF grammar, and unit-tests.tc for an example */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ---------------------------------------------------------------------------
#include "tiny-c.h"

// ---------------------------------------------------------------------------
#include "lexer.c"
#include "parser.c"
#include "gen.c"
#include "vm.c"

void compile() { c(program()); }

int main(int argc, char* argv[]) {
    if (argc > 1) { openSource(argv[1]); }
    else { openSource("unit-tests.tc"); }
    compile();
    run();
    return 0;
}
