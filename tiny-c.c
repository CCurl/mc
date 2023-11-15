/* file: "tiny-c.c" */
/* originally from http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c */
/* Copyright (C) 2001 by Marc Feeley, All Rights Reserved. */
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
 *  <var_def> ::= "int" <id> ";"
 *  <statement> ::= "if" <paren_expr> <statement> |
 *                  "if" <paren_expr> <statement> "else" <statement> |
 *                  "while" <paren_expr> <statement> |
 *                  "do" <statement> "while" <paren_expr> ";" |
 *                  "{" { <statement> } "}" |
 *                  <expr> ";" |
 *                  <func_def> |
 *                  <func> |
 *                  ";"
 *  <paren_expr> ::= "(" <expr> ")"
 *  <expr> ::= <test> | <id> "=" <expr>
 *  <test> ::= <math> | <math> "<" <math> | <math> ">" <math>
 *  <math> ::= <term> | <math> <math_op> <term>
 *  <math_op> ::= "+" | "-" | "*" | "/"
 *  <term> ::= <id> | <int> | <paren_expr>
 *  <id> ::= "a" | "b" | "c" | "d" | ... | "z" -- FOR NOW
 *  <id> ::= [A-Z|a-z][A-Z|a-z|0-9|_]*
 *  <int> ::= <an_unsigned_decimal_integer>
 *  <func_def> ::= "void" <id> "(" ")" "{" <statement> "}" |
 *  <func> ::= <id> "(" ")" ";"
 *
 * The compiler does a minimal amount of error checking to help
 * highlight the structure of the compiler.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

 /*---------------------------------------------------------------------------*/
/* Lexer. */

#define BTWI(n,l,h) ((l<=n)&&(n<=h))
#define HASH_MASK 0x07FF

typedef unsigned int uint;
typedef unsigned char byte;

enum { DO_SYM, ELSE_SYM, IF_SYM, WHILE_SYM, VOID_SYM, INT_SYM, //  0->5
       LBRA, RBRA, LPAR, RPAR, LCOM,                           //  6->10
       PLUS, MINUS, STAR, SLASH, LESS, GRT, SEMI, EQUAL,       // 11->18
       INT, ID, EOI, FUNC_SYM };                               // 19->22

char *words[] = { "do", "else", "if", "while", "void", "int", NULL };

int ch = ' ', sym, int_val, id_hash;
char id_name[64];
FILE *input_fp = NULL;

void message(char *msg) { fprintf(stdout, "%s\n", msg); }
void error(char *err) { message(err); exit(1); }
void syntax_error() { error("-syntax error-"); }

void next_ch() {
    if (input_fp) { ch = fgetc(input_fp); }
    else { ch = getchar(); }
    // if (BTWI(ch,32,126)) { printf("%c", ch); } else { printf("(%d)", ch); }
}

int hash(char *s) {
    int h = 5381;
    while (*s) { h = (h*33)^(*s++); }
    return h & 0x07FF;
}

int isAlpha(int ch) {
    return BTWI(ch,'A','Z') || BTWI(ch,'a','z') || (ch=='_');
}

int isNum(int ch) {
    return BTWI(ch,'0','9');
}

int isAlphaNum(int ch) {
    return isAlpha(ch) || isNum(ch);
}

void lcomment() {
    while (ch !=EOF && ch!='\n') { next_ch(); }
}

void next_sym() {
  again:
    switch (ch) {
      case ' ': case 9: case 10: case 13: next_ch(); goto again;
      case EOF: sym = EOI; break;
      case '{': next_ch(); sym = LBRA;  break;
      case '}': next_ch(); sym = RBRA;  break;
      case '(': next_ch(); sym = LPAR;  break;
      case ')': next_ch(); sym = RPAR;  break;
      case '+': next_ch(); sym = PLUS;  break;
      case '-': next_ch(); sym = MINUS; break;
      case '*': next_ch(); sym = STAR;  break;
      case '/': next_ch(); if (ch=='/') { lcomment(); goto again; }
                            else { sym=SLASH; } break;
      case '<': next_ch(); sym = LESS;  break;
      case '>': next_ch(); sym = GRT;   break;
      case ';': next_ch(); sym = SEMI;  break;
      case '=': next_ch(); sym = EQUAL; break;
      default:
        if (isNum(ch)) {
            int_val = 0; /* missing overflow check */
            while (isNum(ch)) { int_val = int_val*10 + (ch - '0'); next_ch(); }
            sym = INT;
        } else if (isAlpha(ch)) {
            int i = 0; /* missing overflow check */
            while (isAlphaNum(ch)) { id_name[i++]=ch; next_ch(); }
            id_name[i] = '\0';
            sym = 0;
            while (words[sym] != NULL && strcmp(words[sym], id_name) != 0) { sym++; }
            if (words[sym] == NULL) {
              sym = ID;
              id_hash = hash(id_name);
              if (ch=='(') {
                  next_ch();
                  if (ch==')') { sym = FUNC_SYM; next_ch(); }
                  else { syntax_error(); }
              }
            }
        } else { message("-ch-"); syntax_error(); }
        break;
    }
}

/*---------------------------------------------------------------------------*/
/* Parser. */

enum { VAR, CST, ADD, SUB, MUL, DIV, LT, GT, SET, FUNC_DEF, FUNC_CALL,
       IF1, IF2, WHILE, DO, EMPTY, SEQ, EXPR, PROG };

#define MAX_NODES 1000
struct node_s { int kind; struct node_s *o1, *o2, *o3; int val; };
typedef struct node_s node;
int num_nodes = 0;
node nodes[MAX_NODES];

node *new_node(int k) {
    if (MAX_NODES <= num_nodes) { error(""); }
    node *x = &nodes[num_nodes++];
    x->kind = k;
    return x;
}

node *gen(int k, node *o1, node *o2) {
    node *x=new_node(k);
    x->o1=o1; x->o2=o2;
    return x;
}

void expect_sym(int exp) {
    if (sym != exp) {
        printf("-expected symbol[%d],not[%d]-", exp, sym);
        syntax_error();
    }
    next_sym();
}

node *paren_expr(); /* forward declaration */

/* <term> ::= <id> | <int> | <paren_expr> */
node *term() {
  node *x;
  if (sym == ID) {
      x=new_node(VAR);
      // x->val=id_name[0]-'a'; // Update this for longer names
      x->val=id_hash;
      next_sym();
  } else if (sym == INT) {
      x=new_node(CST);
      x->val=int_val;
      next_sym();
  } else x = paren_expr();
  return x;
}

int mathop() {
    if (sym==PLUS) { return ADD; }
    else if (sym==MINUS) { return SUB; }
    else if (sym==STAR)  { return MUL; }
    else if (sym==SLASH) { return DIV; }
    return 0;
}

/* <math> ::= <term> | <math> <math_op> <term> */
/* <math_op> ::= "+" | "-" | "*" | "/" */
node *sum() {
  node *x = term();
  while (mathop()) {
    x=gen(mathop(), x, 0);
    next_sym();
    x->o2=term();
  }
  return x;
}

/* <test> ::= <math> | <math> "<" <math> | <math> ">" <math> */
node *test() {
  node *x = sum();
  if (sym == LESS) { next_sym(); return gen(LT, x, sum()); }
  if (sym == GRT)  { next_sym(); return gen(GT, x, sum()); }
  return x;
}

/* <expr> ::= <test> | <id> "=" <expr> */
node *expr() {
  node *x;
  if (sym != ID) { return test(); }
  x = test();
  if ((x->kind==VAR) && (sym==EQUAL)) {
      next_sym();
      return gen(SET, x, expr());
  }
  return x;
}

/* <paren_expr> ::= "(" <expr> ")" */
node *paren_expr() {
  node *x;
  expect_sym(LPAR);
  x = expr();
  expect_sym(RPAR);
  return x;
}

node *statement() {
  node *x;
  if (sym == IF_SYM) { /* "if" <paren_expr> <statement> */
      x = new_node(IF1);
      next_sym();
      x->o1 = paren_expr();
      x->o2 = statement();
      if (sym == ELSE_SYM) { /* ... "else" <statement> */
          x->kind = IF2;
          next_sym();
          x->o3 = statement();
      }
  } else if (sym == WHILE_SYM) { /* "while" <paren_expr> <statement> */
     x=new_node(WHILE);
     next_sym();
     x->o1=paren_expr();
     x->o2=statement();
  } else if (sym == FUNC_SYM) { /* <id> "();" */
      x=new_node(FUNC_CALL);
      x->val = id_hash;
      // printf("-call %s(%d)-", id_name, x->val);
      next_sym();
      expect_sym(SEMI);
  } else if (sym == DO_SYM) { /* "do" <statement> "while" <paren_expr> ";" */
      x = new_node(DO);
      next_sym();
      x->o1 = statement();
      expect_sym(WHILE_SYM);
      x->o2 = paren_expr();
      expect_sym(SEMI);
  } else if (sym == SEMI) { /* ";" */
      x = new_node(EMPTY);
      next_sym();
  } else if (sym == LBRA) { /* "{" <statement> "}" */
      x = new_node(EMPTY);
      next_sym();
      while (sym != RBRA) {
        x =gen(SEQ, x, NULL);
        x->o2=statement();
      }
      next_sym();
  } else if (sym==VOID_SYM) {
        next_sym(); expect_sym(FUNC_SYM);
        // printf("-def %s()-", id_name);
        x=new_node(FUNC_DEF);
        x->val=id_hash;
        if (sym != LBRA) { expect_sym(LBRA); }
        x->o1=statement();
  } else { /* <expr> ";" */
      x = gen(EXPR, expr(), NULL);
      expect_sym(SEMI);
  }
  return x;
}

/* <program> ::= <statement> */
node *program() {
    next_sym();
    node *prog = gen(PROG, NULL, NULL);
    prog->o1=statement();
    return prog;
}

/*---------------------------------------------------------------------------*/
/* Code generator. */

enum { HALT, FETCH, STORE, LIT1, LIT2, LIT, IDROP, IADD, ISUB, IMUL, IDIV,
        ILT, IGT, JZ, JNZ, JMP, ICALL, IRET };

typedef char code;
code vm[1000], *here = vm;
char *funcs[HASH_MASK+1];

void g(code c) { *here++ = c; } /* missing overflow check */
void g4(int n) {
    g(n & 0xff); n=(n >> 8);
    g(n & 0xff); n=(n >> 8);
    g(n & 0xff); n=(n >> 8);
    g(n & 0xff);
}
void g2(int n) {
    g(n & 0xff); n=(n >> 8);
    g(n & 0xff);
}
code *hole() { return here++; }
void fix(code *src, code *dst) { *src = dst-src; } /* missing overflow check */

void c(node *x) {
    code *p1, *p2;
    switch (x->kind) {
        case VAR  : g(FETCH); g2(x->val); break;
        case CST  : if (BTWI(x->val,0,127)) { g(LIT1); g(x->val); }
                    else if (BTWI(x->val,128,32767)) { g(LIT2); g2(x->val);  }
                    else { g(LIT); g4(x->val);  }
                    break;
        case ADD  : c(x->o1);  c(x->o2); g(IADD); break;
        case MUL  : c(x->o1);  c(x->o2); g(IMUL); break;
        case SUB  : c(x->o1);  c(x->o2); g(ISUB); break;
        case DIV  : c(x->o1);  c(x->o2); g(IDIV); break;
        case LT   : c(x->o1);  c(x->o2); g(ILT); break;
        case GT   : c(x->o1);  c(x->o2); g(IGT); break;
        case SET  : c(x->o2);  g(STORE); g2(x->o1->val); break;
        case IF1  : c(x->o1);  g(JZ); p1=hole(); c(x->o2); fix(p1,here); break;
        case IF2  : c(x->o1);  g(JZ); p1=hole(); c(x->o2); g(JMP); p2=hole();
                    fix(p1,here); c(x->o3); fix(p2,here); break;
        case WHILE: p1=here; c(x->o1); g(JZ); p2=hole(); c(x->o2);
                    g(JMP); fix(hole(),p1); fix(p2,here); break;
        case DO   : p1=here; c(x->o1); c(x->o2); g(JNZ); fix(hole(),p1); break;
        case EMPTY: break;
        case SEQ  : c(x->o1); c(x->o2); break;
        case EXPR : c(x->o1); g(IDROP); break;
        case PROG : c(x->o1); g(HALT);  break;
        case FUNC_DEF: funcs[x->val]=here; c(x->o1); g(IRET); break;
        case FUNC_CALL: g(ICALL); g2(x->val); break;
    }
}

/*---------------------------------------------------------------------------*/
/* Virtual machine. */

int sp, rsp;
long vars[HASH_MASK+1];

#define ACASE    goto again; case
#define TOS      st[sp]
#define NOS      st[sp-1]

int f4(byte *a) {
    int x=*(a+3);
    x = (x<<8)|*(a+2);
    x = (x<<8)|*(a+1);
    x = (x<<8)|*(a+0);
    return x;
}

int f2(byte *a) {
    int x=*(a+1);
    x = (x<<8)|*(a+0);
    return x;
}

void run(code *pc) {
    long st[1000];
    code *rst[1000];
    again:
    switch (*pc++) {
        case  FETCH: st[++sp] = vars[f2((byte*)pc)]; pc +=2;
        ACASE STORE: vars[f2((byte*)pc)] = st[sp]; pc += 2;
        ACASE LIT1  : st[++sp] = *(pc++);
        ACASE LIT2  : st[++sp] = f2((byte*)pc); pc += 2;
        ACASE LIT   : st[++sp] = f4((byte*)pc); pc += 4;
        ACASE IDROP : --sp;
        ACASE IADD  : NOS += TOS; --sp;
        ACASE ISUB  : NOS -= TOS; --sp;
        ACASE IMUL  : NOS *= TOS; --sp;
        ACASE IDIV  : NOS /= TOS; --sp;
        ACASE ILT   : NOS =  (NOS<TOS)?1:0; --sp;
        ACASE IGT   : NOS =  (NOS>TOS)?1:0; --sp;
        ACASE JMP   : pc += *pc;
        ACASE JZ    : if (st[sp--] == 0) pc += *pc; else pc++;
        ACASE JNZ   : if (st[sp--] != 0) pc += *pc; else pc++;
        ACASE ICALL : rst[rsp++] = pc+2; pc=funcs[f2((byte*)pc)];
                        // printf("-run:call [%p]-\n", funcs[f2((byte*)pc)]); pc += 2;
        ACASE IRET : if (rsp) { pc=rst[--rsp]; } else { return; }
        ACASE HALT  : return;
    }
}

/*---------------------------------------------------------------------------*/
/* Disassembly. */

void dis() {
    code *pc = &vm[0];
    int t;
    FILE *fp = fopen("list.txt", "wt");
    if (vm[0]==JMP) { fprintf(fp,"; main() is at %d", (int)(vm[1]+2)); }
    else {  fprintf(fp,"; there is no main() function");  }
    again:
    if (here <= pc) { return; }
    int p = (int)(pc-&vm[0]) + 1;
    fprintf(fp,"\n%04d: %02d ; ", p, *pc);
    switch (*pc++) {
        case  FETCH : t=f2((byte*)pc); fprintf(fp,"fetch [%d]",t); pc +=2;
        ACASE STORE : t=f2((byte*)pc); fprintf(fp,"store [%d]",t); pc +=2;
        ACASE LIT1  : fprintf(fp,"lit1 %d", *(pc++));
        ACASE LIT2  : fprintf(fp,"lit2 %d", f2((byte*)pc)); pc +=2;
        ACASE LIT   : fprintf(fp,"lit4 %d", f4((byte*)pc)); pc +=4;
        ACASE IDROP : fprintf(fp,"drop");
        ACASE IADD  : fprintf(fp,"add");
        ACASE ISUB  : fprintf(fp,"sub");
        ACASE IMUL  : fprintf(fp,"mul");
        ACASE IDIV  : fprintf(fp,"div");
        ACASE ILT   : fprintf(fp,"lt");
        ACASE IGT   : fprintf(fp,"gt");
        ACASE JMP   : fprintf(fp,"jmp %d", p+1+(*pc++));
        ACASE JZ    : fprintf(fp,"jz %d",  p+1+(*pc++));
        ACASE JNZ   : fprintf(fp,"jnz %d", p+1+(*pc++));
        ACASE ICALL : t=f2((byte*)pc); fprintf(fp,"call %d [hash:%d]", (int)(funcs[t]-vm), t); pc += 2;
        ACASE IRET  : fprintf(fp,"ret");
        ACASE HALT  : fprintf(fp,"halt"); goto again;
    }
    fprintf(fp, "\n");
    fclose(fp);
}

/*---------------------------------------------------------------------------*/
/* Main program. */

void compile() {
    g(JMP); g(0);
    c(program());
    char *st = funcs[hash("main")];
    if (st) { vm[1] = (char)(st-vm)-1; }
    else { vm[0] = HALT; }
}

int main(int argc, char *argv[]) {
    for (int i=0; i<=HASH_MASK; i++) { vars[i] = 0; }
    for (int i=0; i<=HASH_MASK; i++) { funcs[i] = 0; }
    if (argc>1) { input_fp = fopen(argv[1], "rt"); }
    compile();
    dis();
    if (input_fp) { fclose(input_fp); }

    printf("(nodes: %d, ", num_nodes);
    printf("code: %d bytes)\n", (int)(here-&vm[0]));
    sp=rsp=0;
    int st = hash("main");
    run(vm);
    for (int i=0; i<=HASH_MASK; i++) {
        if (vars[i] != 0) printf("[%d] = %d\n", i, vars[i]);
    }
    if (sp) { error("-stack not empty-"); }

    return 0;
}
