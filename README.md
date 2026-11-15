# mc - a minimalistic compiler
```
/* Contains code from Marc Feeley's tinyc.c, with permission. */
/* http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c */
/* Heavily modified by Chris Curl to add more C-like support */

/*
 * This is a compiler for Tiny-C, a stripped down version of C.
 * The compiler reads the program from the file named on the command line
 * (default unit-tests.tc), then runs it in the built-in VM. Output comes only from emit().
 * The grammar of Tiny-C in EBNF is:
 *
 *  <program> ::= { <def> }   (must define main)
 *  <include> ::= "include" '"' <file_name> '"' ";"   (splices the file in place; each file is read once)
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
 *  void calls are statements, not terms.
 *  emit(int value) is a built-in VM service that writes the low byte and
 *  returns the written byte.
 *  <id> ::= [A-Z|a-z|_][A-Z|a-z|0-9|_]*  (at most 63 characters)
 *  Reserved words: do, else, if, while, void, int, byte, emit, return, include. "emit" is the built-in function, so it cannot name a variable, parameter, local, or function.
 *  A local or parameter may shadow a global or function name.
 *  int and byte functions must return on every path. A while loop does not count as returning, so "while (1) { return 1; }" is rejected.
 *  <int> ::= <a_decimal_integer>  (no sign; use unary minus. Overflow is not detected)
 *  <char> ::= "'" <single_character_or_escape> "'"
 *  <string> ::= '"' { <string_character_or_escape> } '"'
 *  Comments start with "//" and run to the end of the line; /* */ comments are not supported.
 *  Character literals are int-valued and represent the numeric value of one byte; supported escapes
 * are '\n', '\r', '\t', '\0', '\e', '\\', '\'', and '\"'.
 *  Byte objects use unsigned 8-bit values (0 to 255) stored as bytes for globals and arrays, and in the VM's int slots for locals and parameters. Assignments,
 *  byte parameters, returns, and byte array stores narrow to this range; byte values
 *  promote to int in expressions. Conversions are implicit; casts are not supported.
 *  Scalar initializers are supported. Global initializers must be constant expressions
 *  and are folded at parse time; local initializers are assignments run at the declaration.
 *  String initializers are global byte arrays laid out as [length low byte, length high byte, ASCII values..., 0].
 * The length excludes the final zero; an omitted array size is inferred, and an
 * explicit size must fit the length and terminator. Non-ASCII characters are
 * rejected outside comments and character literals. String escapes are '\n', '\r', '\t', '\0', '\e', '\\', '\'', and '\"'.
 *  String literals used as array arguments get anonymous global storage. They must be
 *  passed to byte[] parameters; other array arguments must match their element type.
 * String literals are valid only as array arguments or global array initializers.
 * Array parameters pass global-array base addresses by reference.
 * Array indexes are unchecked at runtime.
 * The VM service instruction is encoded as SYS followed by a one-byte service ID.
 *
 * The compiler does a minimal amount of error checking to help
 * highlight the structure of the compiler.
 */
```

## Building and Testing

- `make` builds `tc` with clang and `-m32`.
- `make bin` copies `tc` to `~/bin/`.
- `make unit-tests` runs `unit-tests.tc`, which holds the program-level checks, and prints `passed: N, failed: M`.
- `make invalid-tests` compiles each file in `invalid-tests/` and fails if any of them is accepted. These programs must be rejected.
