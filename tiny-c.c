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
 *  <var_def> ::= "int" <id> ";"
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
 *  <id> ::= "a" | "b" | "c" | "d" | ... | "z" -- FOR NOW
 *  <id> ::= [A-Z|a-z][A-Z|a-z|0-9|_]*
 *  <int> ::= <an_unsigned_decimal_integer>
 *  <func_def> ::= "void" <id> "(" ")" "{" <statement> "}" |
 *  <func_call> ::= <id> "(" ")" ";"
 *
 * The compiler does a minimal amount of error checking to help
 * highlight the structure of the compiler.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

 /*---------------------------------------------------------------------------*/
/* Lexer. */

#define MAX_NODES   1000
#define DICT_SZ      999
#define VM_SZ       4096

#define BTWI(n,l,h) ((l<=n)&&(n<=h))

typedef unsigned int uint;
typedef unsigned char byte;

enum {
    DO_SYM, ELSE_SYM, IF_SYM, WHILE_SYM, VOID_SYM, RET_SYM, LBRA, RBRA, LPAR, RPAR,
    PLUS, MINUS, STAR, SLASH, LESS, GRT, SEMI, EQUAL, INT, ID, FUNC, EOI
};

char *words[] = { "do", "else", "if", "while", "void", "return", NULL };

int ch = ' ', sym, int_val;
char id_name[64];
FILE *input_fp = NULL;

void next_ch() {
    if (input_fp) { ch = fgetc(input_fp); }
    else { ch = getchar(); }
    // if (BTWI(ch,32,126)) { printf("%c", ch); } else { printf("(%d)", ch); }
}

void message(char *msg) { fprintf(stdout, "%s\n", msg); }
void error(char *err) { message(err); exit(1); }
void syntax_error() { error("-syntax error-"); }
int isAlpha(int ch) { return BTWI(ch,'A','Z') || BTWI(ch,'a','z') || (ch=='_'); }
int isNum(int ch) { return BTWI(ch,'0','9'); }
int isAlphaNum(int ch) { return isAlpha(ch) || isNum(ch); }
void lcomment() { while (ch !=EOF && ch!='\n') { next_ch(); } }

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
              if (ch=='(') {
                  next_ch();
                  if (ch==')') { sym = FUNC; next_ch(); }
                  else { syntax_error(); }
              }
            }
        } else { message("-ch-"); syntax_error(); }
        break;
    }
}

/*---------------------------------------------------------------------------*/
/* Parser. */

enum { VAR, CST, ADD, SUB, MUL, DIV, LT, GT, SET, FUNC_DEF, FUNC_CALL, RET,
       IF1, IF2, WHILE, DO, EMPTY, SEQ, EXPR, PROG };

typedef struct node_s { int kind; struct node_s *o1, *o2, *o3; int val; } node_t;
int num_nodes = 0, last;
node_t nodes[MAX_NODES];

#define IsVar   0
#define IsFunc  1

typedef struct dict_s { int kind; long val; char nm[16]; } dict_t;
dict_t dict[DICT_SZ+1];

int dict_add(const char *name, int kind) {
    dict_t *p=&dict[++last];
    p->kind = kind;
    strcpy(p->nm, name);
    return last;
}

int dict_find(const char *name, int kind) {
    int i=last;
    while (i) {
        dict_t *p=&dict[i];
        if ((strcmp(p->nm, name)==0) && (p->kind==kind)) { return i; }
        i--;
    }
    return 0;
}

node_t *new_node(int k) {
    if (MAX_NODES <= num_nodes) { error(""); }
    node_t *x = &nodes[num_nodes++];
    x->kind = k;
    return x;
}

node_t *gen(int k, node_t *o1, node_t *o2) {
    node_t *x=new_node(k);
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

node_t *paren_expr(); /* forward declaration */

/* <term> ::= <id> | <int> | <paren_expr> */
node_t *term() {
  node_t *x;
  if (sym == ID) {
      x=new_node(VAR);
      x->val=dict_find(id_name,IsVar);
      if (x->val==0) { x->val=dict_add(id_name,IsVar); }
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
node_t *sum() {
  node_t *x = term();
  while (mathop()) {
    x=gen(mathop(), x, 0);
    next_sym();
    x->o2=term();
  }
  return x;
}

/* <test> ::= <math> | <math> "<" <math> | <math> ">" <math> */
node_t *test() {
  node_t *x = sum();
  if (sym == LESS) { next_sym(); return gen(LT, x, sum()); }
  if (sym == GRT)  { next_sym(); return gen(GT, x, sum()); }
  return x;
}

/* <expr> ::= <test> | <id> "=" <expr> */
node_t *expr() {
  node_t *x;
  if (sym != ID) { return test(); }
  x = test();
  if ((x->kind==VAR) && (sym==EQUAL)) {
      next_sym();
      return gen(SET, x, expr());
  }
  return x;
}

/* <paren_expr> ::= "(" <expr> ")" */
node_t *paren_expr() {
  node_t *x;
  expect_sym(LPAR);
  x = expr();
  expect_sym(RPAR);
  return x;
}

node_t *statement() {
  node_t *x;
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
  } else if (sym == FUNC) { /* <id> "();" */
      x=new_node(FUNC_CALL);
      x->val = dict_find(id_name, IsFunc);
      if (x->val == 0) { printf("-%s() not defined-", id_name); syntax_error(); }
      // printf("-call %s(%d)-", id_name, x->val);
      next_sym();
      expect_sym(SEMI);
  } else if (sym == RET_SYM) { /* "return" ";" */
      next_sym();
      expect_sym(SEMI);
      x=new_node(RET);
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
        next_sym(); expect_sym(FUNC);
        // printf("-def %s()-", id_name);
        x=new_node(FUNC_DEF);
        x->val = dict_find(id_name, IsFunc);
        if (x->val) { printf("-%s() already defined-", id_name); syntax_error(); }
        x->val = dict_add(id_name, IsFunc);
        if (sym != LBRA) { expect_sym(LBRA); }
        x->o1=statement();
  } else { /* <expr> ";" */
      x = gen(EXPR, expr(), NULL);
      expect_sym(SEMI);
  }
  return x;
}

/* <program> ::= <statement> */
node_t *program() {
    next_sym();
    node_t *prog = gen(PROG, NULL, NULL);
    prog->o1=statement();
    return prog;
}

/*---------------------------------------------------------------------------*/
/* Code generator. */

enum { HALT, FETCH, STORE, LIT1, LIT2, LIT, IDROP, IADD, ISUB, IMUL, IDIV,
        ILT, IGT, JZ, JNZ, JMP, ICALL, IRET };

typedef char code;
code vm[VM_SZ];
int here;

void g(code c) { vm[here++] = c; } /* missing overflow check */
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

int hole() { return here++; }
void fix(int src, int dst) { vm[src] = dst-src; } /* missing overflow check */

void c(node_t *x) {
    int p1, p2;
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
        case RET  : g(IRET); break;
        case FUNC_DEF : dict[x->val].val=here; c(x->o1); g(IRET); break;
        case FUNC_CALL: g(ICALL); g2(x->val); break;
    }
}

/*---------------------------------------------------------------------------*/
/* Virtual machine. */

int sp, rsp;

#define ACASE    goto again; case
#define TOS      st[sp]
#define NOS      st[sp-1]
#define f1(a)    vm[a]
#define fu(a)    (byte)vm[a]

int  f2(int a) { return (f1(a+1)<<8) | fu(a); }
long f4(int a) { return (f1(a+3)<<24) | (fu(a+2)<<16)| (fu(a+1)<<8) | fu(a); }

void run(int pc) {
    long st[1000], rst[1000];
    again:
    switch (f1(pc++)) {
        case  FETCH: st[++sp] = dict[f2(pc)].val; pc +=2;
        ACASE STORE: dict[f2(pc)].val = st[sp]; pc += 2;
        ACASE LIT1  : st[++sp] = f1(pc++);
        ACASE LIT2  : st[++sp] = f2(pc); pc += 2;
        ACASE LIT   : st[++sp] = f4(pc); pc += 4;
        ACASE IDROP : --sp;
        ACASE IADD  : NOS += TOS; --sp;
        ACASE ISUB  : NOS -= TOS; --sp;
        ACASE IMUL  : NOS *= TOS; --sp;
        ACASE IDIV  : NOS /= TOS; --sp;
        ACASE ILT   : NOS =  (NOS<TOS)?1:0; --sp;
        ACASE IGT   : NOS =  (NOS>TOS)?1:0; --sp;
        ACASE JMP   : pc += f1(pc);
        ACASE JZ    : if (st[sp--] == 0) pc += f1(pc); else pc++;
        ACASE JNZ   : if (st[sp--] != 0) pc += f1(pc); else pc++;
        ACASE ICALL : rst[rsp++] = pc+2; pc = dict[f2(pc)].val;
                        // printf("-run:call [%p]-\n", funcs[f2((byte*)pc)]); pc += 2;
        ACASE IRET : if (rsp) { pc=rst[--rsp]; } else { return; }
        ACASE HALT  : return;
    }
}

/*---------------------------------------------------------------------------*/
/* Disassembly. */

void dis() {
    int pc=0, t;
    FILE *fp = fopen("list.txt", "wt");
    if (vm[0]==JMP) { fprintf(fp,"; main() is at %d", (int)(vm[1]+1)); }
    else {  fprintf(fp,"; there is no main() function");  }
    again:
    if (here <= pc) { return; }
    int p = pc;
    fprintf(fp,"\n%04d: %02d ; ", p, f1(pc));
    switch (f1(pc++)) {
        case  FETCH : t=f2(pc); fprintf(fp,"fetch [%d] (%s)", t, dict[t].nm); pc+=2;
        ACASE STORE : t=f2(pc); fprintf(fp,"store [%d] (%s)", t, dict[t].nm); pc+=2;
        ACASE LIT1  : fprintf(fp,"lit1 %d", f1(pc)); pc+=1;
        ACASE LIT2  : fprintf(fp,"lit2 %d", f2(pc)); pc+=2;
        ACASE LIT   : fprintf(fp,"lit4 %ld",f4(pc)); pc+=4;
        ACASE IDROP : fprintf(fp,"drop");
        ACASE IADD  : fprintf(fp,"add");
        ACASE ISUB  : fprintf(fp,"sub");
        ACASE IMUL  : fprintf(fp,"mul");
        ACASE IDIV  : fprintf(fp,"div");
        ACASE ILT   : fprintf(fp,"lt");
        ACASE IGT   : fprintf(fp,"gt");
        ACASE JMP   : fprintf(fp,"jmp %d", pc+f1(pc)); pc++;
        ACASE JZ    : fprintf(fp,"jz %d",  pc+f1(pc)); pc++;
        ACASE JNZ   : fprintf(fp,"jnz %d", pc+f1(pc)); pc++;
        ACASE ICALL : t=f2(pc); fprintf(fp,"call %ld (%s)", dict[t].val, dict[t].nm); pc+=2;
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
    int st = dict_find("main", IsFunc);
    if (st) { vm[1] = (char)(dict[st].val-1); }
    else { vm[0] = HALT; }
}

int main(int argc, char *argv[]) {
    if (argc>1) { input_fp = fopen(argv[1], "rt"); }

    here=last=sp=rsp=0;
    compile();
    dis();
    if (input_fp) { fclose(input_fp); }

    printf("(nodes: %d, ", num_nodes);
    printf("code: %d bytes)\n", here);
    run(0);
    for (int i=1; i<=last; i++) {
        dict_t *p=&dict[i];
        printf("%s %s: %ld\n", (p->kind==IsVar)?"var":"func", p->nm, p->val);
    }
    if (sp) { error("-stack not empty-"); }

    return 0;
}
