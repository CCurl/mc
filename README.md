# mc - a minimalistic compiler
```
/* Contains code from Marc Feeley's tinyc.c, with permission. */
/* http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c */
/* Heavily modified by Chris Curl to add more C-like support */

/*
 * This is a compiler for Tiny-C, a stripped down version of C.
 * The compiler reads the program from the file named on the command line
 * (default tests.tc), then runs it in the built-in VM. Output comes only from emit().
 * The grammar of Tiny-C in EBNF is:
 *
 *  <program> ::= { <def> }   (must define main)
 *  <main_def> ::= ("void" | "int") "main" "(" ")" "{" { <statement> } "}"
 *  <defs> ::= { <def> }
 *  <def> ::= <func_def> | <var_def>
 *  <func_def> ::= ("void" | "int") <id> "(" [ <parameter> { "," <parameter> } ] ")" "{" { <statement> } "}"
 *  <parameter> ::= "int" <id> [ "[" "]" ]
 *  <local_decl> ::= "int" <id> { "," <id> } ";"
 *  <var_def> ::= "int" <var_decl> { "," <var_decl> } ";"
 *  <var_decl> ::= <id> [ "[" <int> "]" ] | <id> "[" [ <int> ] "]" "=" <string>
 *  <statement> ::= <local_decl> |
 *                  <void_func_call> ";" |
 *                  "return" [ <expr> ] ";" |
 *                  "if" <paren_expr> <statement> |
 *                  "if" <paren_expr> <statement> "else" <statement> |
 *                  "while" <paren_expr> <statement> |
 *                  "do" <statement> "while" <paren_expr> ";" |
 *                  "{" { <statement> } "}" |
 *                  <expr> ";" |
 *                  ";"
 *  <paren_expr> ::= "(" <expr> ")"
 *  <expr> ::= <test> | <id> "=" <expr> | <id> "[" <expr> "]" "=" <expr>
 *  <test> ::= <math> | <math> "<" <math> | <math> ">" <math> | <math> "==" <math> | <math> "!=" <math>
 *  <math> ::= <term> | <math> <math_op> <term>   (no precedence: applied left to right)
 *  <math_op> ::= "+" | "-" | "*" | "/" | "%"
 *  <term> ::= <id> | <func_call> | <id> "[" <expr> "]" | <array_ref> | <int> | <char> | <string> | <paren_expr> | "-" <term>
 *  <func_call> ::= <int_func_id> "(" [ <expr> { "," <expr> } ] ")"
 *  <void_func_call> ::= <void_func_id> "(" [ <expr> { "," <expr> } ] ")"
 *  <array_ref> ::= <global_array_id> | <array_parameter_id>
 *  <int_func_id> and <void_func_id> are IDs of functions with the
 *  corresponding return type. Void calls are statements, not terms.
 *  emit(int value) is a built-in VM service that writes the low byte and
 *  returns the written byte.
 *  <id> ::= [A-Z|a-z|_][A-Z|a-z|0-9|_]*
 *  <int> ::= <a_decimal_integer>
 *  <char> ::= "'" <single_character_or_escape> "'"
 *  <string> ::= '"' { <string_character_or_escape> } '"'
 * Character literals represent the numeric value of one byte; supported escapes
 * are '\n', '\r', '\t', '\0', '\e', '\\', '\'', and '\"'.
 * String initializers are global integer arrays laid out as [length, ASCII values..., 0].
 * The length excludes the final zero; an omitted array size is inferred, and an
 * explicit size must fit the length and terminator. Non-ASCII characters are
 * rejected outside comments and character literals. String escapes are '\n', '\r', '\t', '\0', '\e', '\\', '\'', and '\"'.
 * String literals used as array arguments get anonymous global storage.
 * String literals are valid only as array arguments or global array initializers.
 * Array parameters pass global-array base slots by reference.
 * Array indexes are unchecked at runtime.
 * The VM service instruction is encoded as SYS followed by a one-byte service ID.
 *
 * The compiler does a minimal amount of error checking to help
 * highlight the structure of the compiler.
 */
```
