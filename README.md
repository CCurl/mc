# mc - a minimalistic compiler
```
/* Based on code from Marc Feeley's tinyc.c, with permission. */
/* http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c */
/* Heavily modified by Chris Curl, MIT license (2026) */

/*
 * This is a compiler for min-c, a stripped down version of C.
 * The compiler reads the program from the file named on the command line,
 * then runs it in the built-in VM. Output comes only from emit().
 * The grammar of min-c in EBNF is:
 *
 *  <program> ::= { <def> }   (must define main)
 *  <include> ::= "#include" '"' <file_name> '"'  (splices the file in place; each file is read once)
 *  main is a <func_def> with no parameters and return type "int" or "void"; it must be defined exactly once.
 *  <defs> ::= { <def> }
 *  <def> ::= <func_def> | <func_proto> | <var_def>
 *  <func_def> ::= ("void" | "int" | "byte") <id> "(" [ <parameter> { "," <parameter> } ] ")" "{" { <statement> } "}"
 *  <func_proto> ::= ("void" | "int" | "byte") <id> "(" [ <parameter> { "," <parameter> } ] ")" ";"
 *  <parameter> ::= ("int" | "byte") <id> [ "[" "]" ]
 *  <local_decl> ::= ("int" | "byte") <id> [ "=" <expr> ] { "," <id> [ "=" <expr> ] } ";"
 *  <var_def> ::= ("int" | "byte") <var_decl> { "," <var_decl> } ";"
 *  <var_decl> ::= <id> [ "[" <int> "]" ] | <id> "=" <constant_expr> | <id> "[" [ <int> ] "]" "=" <string>
 *  <statement> ::= <local_decl> |
 *                  <void_func_call> ";" |
 *                  "return" [ <expr> ] ";" |
 *                  "if" <paren_expr> <statement> |
 *                  "if" <paren_expr> <statement> "else" <statement> |
 *                  "while" <paren_expr> <statement> |
 *                  "do" <statement> "while" <paren_expr> ";" |
 *                  "{" { <statement> } "}" |
 *                  <assignment> ";" |
 *                  <expr> ";" |
 *                  ";"
 *  <paren_expr> ::= "(" <expr> ")"
 *  <assignment> ::= <lvalue> "=" <expr>   (assignment is a statement only, not an expression)
 *  <lvalue> ::= <id> | <id> "[" <expr> "]"
 *  <expr> ::= <test>
 *  <incdec> ::= ("++" | "--") <scalar_id> | <scalar_id> ("++" | "--")   (scalar locals, parameters, and globals only; postfix yields the old value, prefix the new)
 *  <test> ::= <math> | <math> "<" <math> | <math> ">" <math> | <math> "<=" <math> | <math> ">=" <math> | <math> "==" <math> | <math> "!=" <math>
 *  <math> ::= <term> | <math> <math_op> <term>   (no precedence: applied left to right)
 *  <math_op> ::= "+" | "-" | "*" | "/" | "%"
 *  <term> ::= <id> | <func_call> | <id> "[" <expr> "]" | <array_ref> | <incdec> | <int> | <char> | <string> | <paren_expr> | "-" <term>
 *  <func_call> ::= (<int_func_id> | <byte_func_id>) "(" [ <expr> { "," <expr> } ] ")"
 *  <void_func_call> ::= <void_func_id> "(" [ <expr> { "," <expr> } ] ")"
 *  <array_ref> ::= <global_array_id> | <array_parameter_id>
 *  <array_arg> ::= <array_ref> | <string>   (a string literal is valid only as an array argument or a global array initializer)
 *  <int_func_id> and <byte_func_id> are IDs of functions returning a value;
 *  <id> ::= [A-Z|a-z|_][A-Z|a-z|0-9|_]*  (at most 63 characters)
 *
 * Reserved words: do, else, if, while, void, int, byte, emit, return.
 * * "emit" is a built-in function, so it cannot name a variable, parameter, local, or function.
 * * emit(int value) is a built-in VM service that writes the low byte to stdout.
 * void calls are statements, not terms.
 * A function that reaches its end without a return returns 0.
 * A local or parameter may shadow a global or function name.
 * Comments start with "//" and run to the end of the line; /* */ comments are not supported.
 * <int> ::= <a_decimal_integer> (Overflow is not detected)
 * <string> ::= '"' { <string_character_or_escape> } '"'
 * <char> ::= "'" <single_character_or_escape> "'"
 * Character literals are int-valued and represent the numeric value of one byte.
 * Supported escapes are: '\n', '\r', '\t', '\0', '\e', '\\', '\'', and '\"'.
 *
 * The compiler does a minimal amount of error checking to help
 * highlight the structure of the compiler.
 */
```

## Building and Testing

- `make` builds `mc` with clang and `-m64`.
- `make bin` copies `mc` to `~/bin/`.
- `make test` runs `unit-tests.mc`, which holds the program-level checks, and prints `passed: N, failed: M`.
- `make invalid-tests` compiles each file in `invalid-tests/` and fails if any of them is accepted. These programs must be rejected.
- `make test-listing` writes a listing of `listing-expected.mc` to `listing.txt` and diffs it against `listing-expected.txt`.

## Command line

    mc [-l] [-o] [-r] [-?] <source>

```
usage: mc [-l] [-o] [-r] [-?] <source>
  compiles the specified source file
  -l: create 'listing.txt' file
  -o: output compiled image to 'a.img'
  -r: run after compile (default on)
  -?: show this help message
  defaults: -l: off, -o: off, -r: on
```
A source file is required. Without one, `mc` prints the usage message and exits.


## Notes

There is no operator precedence - strictly left-to-right.<br/>
Use parentheses to control order. EG:
- 2 + 3 * 4  = 20 (not 14)
- 2 +(3 * 4) = 14
- 3 * 4 + 2  = 14

Byte objects use unsigned 8-bit values (0 to 255) stored as bytes for globals and arrays, and in the<br/>
VM's int slots for locals and parameters.  Assignments, byte parameters, returns, and byte array stores<br/>
narrow to this range; byte values promote to int in expressions.  Conversions are implicit; casts are<br/>
not supported. Scalar initializers are supported. Global initializers must be constant expressions and<br/>
are folded at parse time; local initializers are assignments run at the declaration. String initializers<br/>
are global byte arrays laid out as [length low byte, length high byte, ASCII values..., 0]. The length excludes <br/>
the final zero; an omitted array size is inferred, and an explicit size must fit the length and terminator<br/>
Non-ASCII characters are rejected outside comments and character literals.<br/>
String escapes are '\n', '\r', '\t', '\0', '\e', '\\', '\'', and '\"'. String literals used as array arguments<br/>
get anonymous global storage. They must be passed to byte[] parameters; other array arguments must match their<br/>
element type. String literals are valid only as array arguments or global array initializers. Array parameters<br/>
pass global-array base addresses by reference. Array indexes are unchecked at runtime. The VM service instruction<br/>
is encoded as `SYS` followed by a one-byte service ID.
