/* Contains code from Marc Feeley's tinyc.c, with permission. */
/* http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c */
/* Heavily modified by Chris Curl to add more C-like support */
/* See the README for the EBNF grammar, and unit-tests.tc for an example */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ---------------------------------------------------------------------------
#define BTWI(n,l,h) ((l<=n)&&(n<=h))

// ---------------------------------------------------------------------------
#define MAX_PATH_LENGTH        64
#define MAX_INCLUDE_DEPTH      16
#define MAX_INCLUDED_FILES    128
#define MAX_NAMES            1024
#define MAX_STRING_LENGTH    1024
#define MAX_NODES            4096
#define MAX_FUNCTIONS         256
#define MAX_PARAMETERS         64
#define MAX_LOCALS            256
#define MAX_CODE            65536
#define MAX_GLOBAL_SLOTS   256000
#define MAX_CALL_FIXUPS MAX_NODES

// ---------------------------------------------------------------------------
#include "lexer.h"
#include "parser.h"
#include "gen.h"
#include "vm.h"

void compile() { c(program()); }

int main(int argc, char* argv[]) {
    if (argc > 1) { open_source(argv[1]); }
    else { open_source("unit-tests.tc"); }
    compile();
    run();
    return 0;
}
