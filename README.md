# mc - a minimalistic compiler
```
/* Contains code from Marc Feeley's tinyc.c, with permission. */
/* http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c */
/* Heavily modified by Chris Curl to add more C-like support */

/*
 * This is a compiler for Tiny-C, a stripped down version of C.
 * The compiler reads the program from standard input and prints out
 * the value of the variables that are not zero.
 * The grammar of Tiny-C in EBNF is:
 *
 *  <program> ::= <global_defs> <function_defs> <main_def>
 *  <main_def> ::= ("void" | "int") "main" "(" ")" "{" <statement> "}"
 *  <defs> ::= <def> | <def> <def>
 *  <def> ::= <func_def> | <var_def>
 *  <func_def> ::= ("void" | "int") <id> "(" [ <parameter> { "," <parameter> } ] ")" "{" { <statement> } "}"
 *  <parameter> ::= "int" <id> [ "[" "]" ]
 *  <local_decl> ::= "int" <id> { "," <id> } ";"
 *  <var_def> ::= "int" <var_decl> { "," <var_decl> } ";"
 *  <var_decl> ::= <id> [ "[" <int> "]" ]
 *  <statement> ::= <local_decl> |
 *                  <void_func_call> ";" |
 *                  "return" <expr> ";" |
 *                  "if" <paren_expr> <statement> |
 *                  "if" <paren_expr> <statement> "else" <statement> |
 *                  "while" <paren_expr> <statement> |
 *                  "do" <statement> "while" <paren_expr> ";" |
 *                  "{" { <statement> } "}" |
 *                  <expr> ";" |
 *                  ";"
 *  <paren_expr> ::= "(" <expr> ")"
 *  <expr> ::= <test> | <id> "=" <expr> | <id> "[" <expr> "]" "=" <expr>
 *  <test> ::= <math> | <math> "<" <math> | <math> ">" <math>
 *  <math> ::= <term> | <math> <math_op> <term>
 *  <math_op> ::= "+" | "-" | "*" | "/"
 *  <term> ::= <id> | <func_call> | <id> "[" <expr> "]" | <array_ref> | <int> | <paren_expr> | "-" <term>
 *  <func_call> ::= <int_func_id> "(" [ <expr> { "," <expr> } ] ")"
 *  <void_func_call> ::= <void_func_id> "(" [ <expr> { "," <expr> } ] ")"
 *  <array_ref> ::= <global_array_id> | <array_parameter_id>
 *  <int_func_id> and <void_func_id> are IDs of functions with the
 *  corresponding return type. Void calls are statements, not terms.
 *  <id> ::= [A-Z|a-z|_][A-Z|a-z|0-9|_]*
 *  <int> ::= <a_decimal_integer>
 * Array parameters pass global-array base slots by reference.
 * Array indexes are unchecked at runtime.
 *
 * The compiler does a minimal amount of error checking to help
 * highlight the structure of the compiler.
 */
```