# mc - a minimalistic compiler
```
/* Based on code from Marc Feeley's tinyc.c, with permission. */
/* http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c */
/* Heavily modified by Chris Curl, MIT license (2026) */

/*
 * This is a compiler for min-c, a stripped down version of C.
 * The compiler reads the program from the file named on the command line and generates 32-bit x86 machine code.
 * It can then run that code in the built-in VM (-r) or write it as a Linux executable (-o). Output comes only from emit().
 * The grammar of min-c in EBNF is:
 *
 *  <program> ::= { <def> }   (must define main)
 *  <include> ::= "#include" '"' <file_name> '"'  (splices the file in place; each file is read once)
 *  main is a <func_def> with no parameters and return type "int"; it must be defined exactly once.
 *  <defs> ::= { <def> }
 *  <def> ::= <func_def> | <func_proto> | <var_def> | <const_def>
 *  <func_def> ::= ("int" | "char") <id> "(" [ <parameter> { "," <parameter> } ] ")" "{" { <statement> } "}"
 *  <func_proto> ::= ("int" | "char") <id> "(" [ <parameter> { "," <parameter> } ] ")" ";"
 *  <parameter> ::= "int" <id> [ "[" "]" ] | "char" [ "*" ] <id> [ "[" "]" ]    (a "char *" parameter takes no "[]")
 *  <local_decl> ::= ("int" | "char") <local_item> { "," <local_item> } ";"    (each "char *" declarator needs its own "*", as in C)
 *  <local_item> ::= <id> [ "=" <expr> ] | <id> "[" "]" "=" <array_name>    (an alias must name an array of the same element type)
 *  <var_def> ::= ("int" | "char") <var_decl> { "," <var_decl> } ";"
 *  <var_decl> ::= <id> [ "[" <int> "]" ] | <id> "=" <constant_expr> | <id> "[" [ <int> ] "]" "=" <string>
 *  <const_def> ::= "const" ("int" | "char") <id> "=" <constant_expr> ";"    (scalar only; a constant name is replaced by its value, and cannot be assigned, incremented, or indexed)
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
 *  <expr> ::= <test> | <expr> ("&&" | "||") <test>   (short-circuit, result is 0 or 1)
 *  <incdec> ::= ("++" | "--") <scalar_id> | <scalar_id> ("++" | "--")   (scalar locals, parameters, and globals only; postfix yields the old value, prefix the new)
 *  <test> ::= <math> | <math> "<" <math> | <math> ">" <math> | <math> "<=" <math> | <math> ">=" <math> | <math> "==" <math> | <math> "!=" <math>
 *  <math> ::= <term> | <math> <math_op> <term>   (no precedence: applied left to right)
 *  <math_op> ::= "+" | "-" | "*" | "/" | "%" | "&" | "|" | "^" | "<<" | ">>"   (">>" is an arithmetic shift)
 *  <term> ::= <id> | <func_call> | <id> "[" <expr> "]" | <array_ref> | <incdec> | <int> | <char> | <string> | <paren_expr> | "-" <term> | "~" <term>
 * A "char *" variable is a pointer to bytes.
 * * Assignment: a "char *" variable may be assigned a "char *" value, a string literal, a char array name, or 0.
 * * Arguments: a "char *" value may be passed to a "char *" parameter or a syscall argument.
 * * Access: p[i] and *p read or write a byte, and *p = e is valid. ++p, --p, p++, and p-- move one byte per step.
 * * Arithmetic: a "char *" value is a "char *" variable, or a char * or char array with "+" or "-" applied left to right. So p+1, 1+p, buf+n, and p+1+2 are valid.
 * * Comparison: ==, !=, <, >, <=, and >= compare two "char *" values, a "char *" and a char array, or a "char *" and 0 (for == and != only).
 * * Not allowed: storing a "char *" value in an int, a byte, or a byte array element; using it in int arithmetic; or returning it.
 * * Still rejected: &x, p+q, p-q, p+1*2 (left to right means (p+1)*2), int *, char **, arrays of char *, and char * function results.
 *  <func_call> ::= (<int_func_id> | <byte_func_id>) "(" [ <expr> { "," <expr> } ] ")"
 *  <void_func_call> ::= <void_func_id> "(" [ <expr> { "," <expr> } ] ")"
 *  <array_ref> ::= <global_array_id> | <array_parameter_id>
 *  <array_arg> ::= <array_ref> | <string>   (a string literal is valid only as an array argument or a global array initializer)
 *  <int_func_id> and <byte_func_id> are IDs of functions returning a value;
 *  <id> ::= [A-Z|a-z|_][A-Z|a-z|0-9|_]*  (at most 63 characters)
 *
 * Reserved words: do, else, if, while, void, int, char, return, const. Using void is a compile error.
 * * "syscall" is a built-in intrinsic, so it cannot name a function. syscall(n, a1, ..., a6) takes 1 to 7 arguments, loads them into eax, ebx, ecx, edx, esi, edi, ebp, and runs int 0x80. ebp is saved and restored.
 * * emit(int value) is a library function in lib/util.mc. It calls syscall to write the low byte to stdout.
 * every function returns int, so calls can be terms or statements.
 * A function that reaches its end without a return returns 0.
 * A local or parameter may shadow a global or function name. A local is visible only in the block that declares it, so a name declared in a block can be reused by a later block.
 * Comments start with "//" and run to the end of the line; /* */ comments are not supported.
 * <int> ::= <a_decimal_integer> (Overflow is not detected)
 * <string> ::= '"' { <string_character_or_escape> } '"'
 * <char> ::= "'" <single_character_or_escape> "'"
 * Character literals are int-valued and represent the numeric value of one byte.
 * Supported escapes are: '\a', '\b', '\f', '\n', '\r', '\t', '\v', '\0', '\e', '\\', '\'', and '\"'. Any other escaped character stands for itself (for example, '\q' is 'q').
 *
 * The compiler does a minimal amount of error checking to help
 * highlight the structure of the compiler.
 */
```

## Building and Testing

- `make` builds `mc` with clang and `-m64`.
- `make bin` copies `mc` to `~/bin/`.
- `make test` runs `unit-tests.mc`, which holds the program-level checks, and prints `passed: N, failed: M`.
- `make test-invalid` compiles each file in `invalid-tests/` and fails if any of them is accepted. These programs must be rejected.
- `make test-listing` writes a listing of `listing-expected.mc` to `listing.txt` and diffs it against `listing-expected.txt`.
- `make test-exe` builds `unit-tests.mc` into a native executable, runs it, and diffs its output against the VM run.
- `make bench-vm` runs `bench.mc` in the VM.
- `make bench` builds `bench.mc` into an executable named `bench`, and `make bench-exe` runs it.

## Command line

    mc [-l] [-o <exe>] [-r] [-?] <source>

```
usage: mc [-l] [-o <exe>] [-r] [-?] <source>
  compiles the specified source file
  -l: create 'listing.txt' file
  -o <exe>: write a Linux 32-bit executable to <exe>
  -r: run in the VM after compile (ignored when -o is given)
  -?: show this help message
  defaults: -l: off, -o: off, -r: off (compile only)
```
A source file is required. Without one, `mc` prints the usage message and exits.


## Notes

There is no operator precedence - strictly left-to-right.<br/>
Use parentheses to control order. EG:
- 2 + 3 * 4  = 20 (not 14)
- 2 +(3 * 4) = 14
- 3 * 4 + 2  = 14

Byte objects use unsigned 8-bit values (0 to 255) stored as bytes for globals and arrays, and in the<br/>
4-byte stack slots for locals and parameters.  Assignments, char parameters, returns, and char array stores<br/>
narrow to this range; char values promote to int in expressions.  Conversions are implicit; casts are<br/>
not supported. Scalar initializers are supported. Global initializers must be constant expressions and<br/>
are folded at parse time; local initializers are assignments run at the declaration. String initializers<br/>
are global char arrays laid out as the ASCII values followed by a 0. An omitted array size is inferred, and an explicit size must fit the characters and terminator<br/>
Non-ASCII characters are rejected outside comments and character literals.<br/>
String escapes are '\a', '\b', '\f', '\n', '\r', '\t', '\v', '\0', '\e', '\\', '\'', '\"'. String literals used as array arguments<br/>
get anonymous global storage. They must be passed to char[] parameters; other array arguments must match their<br/>
element type. String literals are valid only as array arguments or global array initializers. Array parameters<br/>
pass global-array base addresses by reference. Array indexes are not bounds-checked. An access outside the memory<br/>
image fails with `-bad memory address-`, but an access that lands in the globals, code, or stack is not detected.<br/>
`syscall(n, ...)` compiles to a Linux system call (`int 0x80`, `eax` = n). `emit` is a library function in `lib/util.mc` that calls `syscall(4, 1, ...)` to write one byte to stdout.
