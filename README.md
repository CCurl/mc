# mc - a minimalistic compiler
```
/* Contains code from Marc Feeley's tinyc.c, with permission. */
/* http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c */
/* Heavily modified and enhanced by Chris Curl */

/*
 * This is a compiler for a considerably stripped down version of C.
 * It is meant as a starting point for a minimalistic compiler.
 * It compiles to a byte-coded VM that is stack-based.
 * The compiler reads the program from standard input or a file
 * and executes the "main()" function upon successful compilation.
 * The grammar of the language in EBNF is:
 *
 *  <program> ::= <statement>
 *  <statement> ::= "if" <paren_expr> <statement> |
 *                  "if" <paren_expr> <statement> "else" <statement> |
 *                  "while" <paren_expr> <statement> |
 *                  "do" <statement> "while" <paren_expr> ";" |
 *                  "{" <statement> "}" |
 *                  <expr> ";" |
 *                  <func_def> |
 *                  <func_call> |
 *                  "return" ";" |
 *                  ";"
 *  <paren_expr> ::= "(" <expr> ")"
 *  <expr> ::= <test> | <id> "=" <expr>
 *  <test> ::= <math> | <math> "<" <math> | <math> ">" <math>
 *  <math> ::= <term> | <math> <math_op> <term>
 *  <math_op> ::= "+" | "-" | "*" | "/"
 *  <term> ::= <id> | <int> | <paren_expr>
 *  <id> ::= [A-Z|a-z][A-Z|a-z|0-9|_]*
 *  <int> ::= <an_unsigned_decimal_integer>
 *  <func_def> ::= "void" <id> "(" ")" "{" <statement> "}" |
 *  <func_call> ::= <id> "(" ")" ";"
 *
 * The compiler does a minimal amount of error checking to help
 * highlight the structure of the compiler.
 */
 ````
