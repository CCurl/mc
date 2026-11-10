/* Contains code from Marc Feeley's tinyc.c, with permission. */
/* http://www.iro.umontreal.ca/~felipe/IFT2030-Automne2002/Complements/tinyc.c */
/* Heavily modified by Chris Curl to add more C-like support */
/* See the README for the EBNF grammar, and test1.tc for an example */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
/* Lexer. */

#define BTWI(n,l,h) ((l<=n)&&(n<=h))
enum { DO_SYM, ELSE_SYM, IF_SYM, WHILE_SYM, VOID_SYM, INT_SYM, //  0->5
       LBRA, RBRA, LPAR, RPAR,                                 //  6->9
       PLUS, MINUS, STAR, SLASH, MODULO, LESS, GRT, SEMI, EQUAL, EQ_SYM, NE_SYM, // 10->20
       INT, ID, EOI, LBRACKET, RBRACKET, COMMA, RETURN_SYM,
       CHAR_LITERAL, STRING_LITERAL };

char *words[] = { "do", "else", "if", "while", "void", "int", NULL };

int ch = ' ', sym, int_val;
char id_name[64];
FILE *input_fp = NULL;

#define MAX_NAMES 1024
#define MAX_STRING_LENGTH 1024
#define MAX_GLOBAL_SLOTS 256000
char variable_names[MAX_NAMES][sizeof(id_name)];
int variable_bases[MAX_NAMES], variable_sizes[MAX_NAMES];
int variable_is_array[MAX_NAMES];
int string_values[MAX_STRING_LENGTH], string_length;
int globals[MAX_GLOBAL_SLOTS];
int num_variables;
int num_global_slots;

void message(char *msg) { fprintf(stdout, "%s\n", msg); }
void error(char *err) { message(err); exit(1); }
void syntax_error() { error("-syntax error-"); }

int find_variable(const char *name) {
    for (int i = 0; i < num_variables; i++) {
        if (strcmp(variable_names[i], name) == 0) { return i; }
    }
    return -1;
}

int declare_variable(const char *name, int size, int is_array) {
    int index = find_variable(name);
    if (index >= 0) {
        printf("-duplicate variable %s-\n", name);
        exit(1);
    }
    if (size <= 0) { error("-array size must be positive-"); }
    if (num_variables >= MAX_NAMES || size > MAX_GLOBAL_SLOTS - num_global_slots) {
        error("-too many variables-");
    }
    strcpy(variable_names[num_variables], name);
    variable_bases[num_variables] = num_global_slots;
    variable_sizes[num_variables] = size;
    variable_is_array[num_variables] = is_array;
    num_global_slots += size;
    return num_variables++;
}

int allocate_string_literal() {
    int size = string_length + 2;
    if (size > MAX_GLOBAL_SLOTS - num_global_slots) {
        error("-too much global storage-");
    }
    int base = num_global_slots;
    num_global_slots += size;
    globals[base] = string_length;
    for (int i = 0; i < string_length; i++) {
        globals[base + i + 1] = string_values[i];
    }
    return base;
}

int parameter_index(const char *name);
int local_index(const char *name);
int current_parameter_is_array(int parameter);

int variable_index(const char *name) {
    int parameter = parameter_index(name);
    if (parameter >= 0) {
        if (current_parameter_is_array(parameter)) {
            printf("-array parameter %s requires an index-\n", name);
            exit(1);
        }
        return -parameter - 1;
    }
    int index = find_variable(name);
    if (index < 0) {
        printf("-undeclared variable %s-\n", name);
        exit(1);
    }
    if (variable_is_array[index]) {
        printf("-array %s requires an index-\n", name);
        exit(1);
    }
    return variable_bases[index];
}

int array_index(const char *name) {
    if (local_index(name) >= 0) {
        printf("-local variable %s is not an array-\n", name);
        exit(1);
    }
    int parameter = parameter_index(name);
    if (parameter >= 0) {
        if (!current_parameter_is_array(parameter)) {
            printf("-parameter %s is not an array-\n", name);
            exit(1);
        }
        return -parameter - 1;
    }
    int index = find_variable(name);
    if (index < 0) {
        printf("-undeclared variable %s-\n", name);
        exit(1);
    }
    if (!variable_is_array[index]) {
        printf("-variable %s is not an array-\n", name);
        exit(1);
    }
    return variable_bases[index];
}

int array_reference(const char *name) {
    if (local_index(name) >= 0) {
        printf("-local variable %s is not an array-\n", name);
        exit(1);
    }
    int parameter = parameter_index(name);
    if (parameter >= 0) {
        if (current_parameter_is_array(parameter)) {
            return -parameter - 1;
        }
        printf("-parameter %s is not an array-\n", name);
        exit(1);
    }
    int index = find_variable(name);
    if (index < 0) {
        printf("-undeclared variable %s-\n", name);
        exit(1);
    }
    if (!variable_is_array[index]) {
        printf("-variable %s is not an array-\n", name);
        exit(1);
    }
    return variable_bases[index];
}

int isAlpha(int ch) { return BTWI(ch,'A','Z') || BTWI(ch,'a','z') || (ch=='_'); }
int isNum(int ch) { return BTWI(ch,'0','9'); }
int isAlphaNum(int ch) { return isAlpha(ch) || isNum(ch); }

int escaped_character(int ch) {
    switch (ch) {
      case 'n': return '\n';
      case 'r': return '\r';
      case 't': return '\t';
      case '0': return '\0';
      case 'e': return 27;
      case '\\': return '\\';
      case '\'': return '\'';
      case '"': return '"';
      default: error("-invalid character escape-");
    }
    return 0;
}

void next_ch() {
    if (input_fp) { ch = fgetc(input_fp); }
    else { ch = getchar(); }
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
      case '/':
        next_ch();
        if (ch == '/') {
            while (ch != EOF && ch != '\n' && ch != '\r') { next_ch(); }
            goto again;
        }
        sym = SLASH;
        break;
      case '%': next_ch(); sym = MODULO; break;
      case '<': next_ch(); sym = LESS;  break;
      case '>': next_ch(); sym = GRT;   break;
      case '!':
        next_ch();
        if (ch != '=') { syntax_error(); }
        next_ch();
        sym = NE_SYM;
        break;
      case ';': next_ch(); sym = SEMI;  break;
      case '=':
        next_ch();
        if (ch == '=') {
            next_ch();
            sym = EQ_SYM;
        } else {
            sym = EQUAL;
        }
        break;
      case '[': next_ch(); sym = LBRACKET; break;
      case ']': next_ch(); sym = RBRACKET; break;
      case ',': next_ch(); sym = COMMA; break;
      case '\'':
        next_ch();
        if (ch == EOF || ch == '\n' || ch == '\r' || ch == '\'') {
            error("-invalid character literal-");
        }
        if (ch == '\\') {
            next_ch();
            int_val = escaped_character(ch);
        } else {
            int_val = ch;
        }
        next_ch();
        if (ch != '\'') { error("-character literal must contain one character-"); }
        next_ch();
        sym = CHAR_LITERAL;
        break;
      case '"':
        string_length = 0;
        next_ch();
        while (ch != '"') {
            if (ch == EOF || ch == '\n' || ch == '\r') {
                error("-unterminated string literal-");
            }
            int value;
            if (ch == '\\') {
                next_ch();
                value = escaped_character(ch);
            } else {
                value = ch;
            }
            if (value > 127) { error("-string literals must contain ASCII characters-"); }
            if (string_length >= MAX_STRING_LENGTH) {
                error("-string literal too long-");
            }
            string_values[string_length++] = value;
            next_ch();
        }
        next_ch();
        sym = STRING_LITERAL;
        break;
      default:
        if (isNum(ch)) {
            int_val = 0; /* missing overflow check */
            while (isNum(ch)) { int_val = int_val*10 + (ch - '0'); next_ch(); }
            sym = INT;
        } else if (isAlpha(ch)) {
            int i = 0;
            while (isAlphaNum(ch)) {
                if (i >= (int)sizeof(id_name) - 1) { error("-identifier too long-"); }
                id_name[i++]=ch;
                next_ch();
            }
            id_name[i] = '\0';
            if (strcmp(id_name, "return") == 0) {
                sym = RETURN_SYM;
            } else {
              sym = 0;
              while (words[sym] != NULL && strcmp(words[sym], id_name) != 0) { sym++; }
              if (words[sym] == NULL) {
                  sym = ID;
              }
            }
        } else { message("-ch-"); syntax_error(); }
        break;
    }
}

/*---------------------------------------------------------------------------*/
/* Parser. */

enum { VAR, CST, ADD, SUB, MUL, DIV, MOD, LT, GT, EQ, NE, SET, FUNC_CALL,
       ARRAY_GET, ARRAY_SET, ARRAY_REF, LOCAL, RETURN_NODE, VOID_RETURN, ARG_LIST,
       IF1, IF2, WHILE, DO, EMPTY, SEQ, EXPR, PROG };

#define MAX_NODES 4096
struct node_s { int kind; struct node_s *o1, *o2, *o3; int val; };
typedef struct node_s node;
int num_nodes = 0;
node nodes[MAX_NODES];

#define MAX_FUNCTIONS 256
#define MAX_PARAMETERS 64
#define MAX_LOCALS 256
struct function_s {
    char name[sizeof(id_name)];
    node *body;
    int address;
    int defined;
    int returns_value;
    int num_parameters;
    char parameter_names[MAX_PARAMETERS][sizeof(id_name)];
    int parameter_is_array[MAX_PARAMETERS];
    int num_locals;
    char local_names[MAX_LOCALS][sizeof(id_name)];
};
struct function_s functions[MAX_FUNCTIONS];
int num_functions;
int current_function = -1;
int main_function = -1;
node *function_calls[MAX_NODES];
int num_function_calls;

int parameter_index(const char *name) {
    if (current_function >= 0) {
        struct function_s *function = &functions[current_function];
        for (int i = 0; i < function->num_parameters; i++) {
            if (strcmp(function->parameter_names[i], name) == 0) { return i; }
        }
    }
    return -1;
}

int local_index(const char *name) {
    if (current_function >= 0) {
        struct function_s *function = &functions[current_function];
        for (int i = 0; i < function->num_locals; i++) {
            if (strcmp(function->local_names[i], name) == 0) { return i; }
        }
    }
    return -1;
}

int current_parameter_is_array(int parameter) {
    return current_function >= 0 &&
           functions[current_function].parameter_is_array[parameter];
}

int function_index(const char *name) {
    for (int i = 0; i < num_functions; i++) {
        if (strcmp(functions[i].name, name) == 0) { return i; }
    }
    if (num_functions >= MAX_FUNCTIONS) { error("-too many functions-"); }
    strcpy(functions[num_functions].name, name);
    functions[num_functions].body = NULL;
    functions[num_functions].address = 0;
    functions[num_functions].defined = 0;
    functions[num_functions].returns_value = 0;
    functions[num_functions].num_parameters = 0;
    functions[num_functions].num_locals = 0;
    return num_functions++;
}

#define BUILTIN_EMIT (-1)

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
node *expr();

node *call_arguments(const char *name) {
    int index = strcmp(name, "emit") == 0 ? BUILTIN_EMIT : function_index(name);
    node *args = NULL;
    node **tail = &args;
    expect_sym(LPAR);
    if (sym != RPAR) {
        do {
            node *arg = expr();
            *tail = gen(ARG_LIST, arg, NULL);
            tail = &(*tail)->o2;
            if (sym != COMMA) { break; }
            next_sym();
        } while (1);
    }
    expect_sym(RPAR);
    node *call = new_node(FUNC_CALL);
    call->val = index;
    call->o1 = args;
    if (num_function_calls >= MAX_NODES) { error("-too many function calls-"); }
    function_calls[num_function_calls++] = call;
    return call;
}

/* <term> ::= <id> | <id> "[" <expr> "]" | <int> | <char> | <string> | <paren_expr> | "-" <term> */
node *term() {
  node *x;
  if (sym == MINUS) {
      next_sym();
      x = new_node(CST);
      x->val = 0;
      return gen(SUB, x, term());
  } else if (sym == ID) {
      char name[sizeof(id_name)];
      strcpy(name, id_name);
      next_sym();
      if (sym == LPAR) {
          x = call_arguments(name);
      } else if (sym == LBRACKET) {
          int base = array_index(name);
          next_sym();
          node *index = expr();
          expect_sym(RBRACKET);
          x = gen(ARRAY_GET, index, NULL);
          x->val = base;
      } else {
          int variable = find_variable(name);
          int parameter = parameter_index(name);
          int local = local_index(name);
          if (local >= 0) {
              x = new_node(LOCAL);
              x->val = functions[current_function].num_parameters + local;
          } else if (parameter >= 0 &&
                     functions[current_function].parameter_is_array[parameter]) {
              x = new_node(ARRAY_REF);
              x->val = array_reference(name);
          } else if (parameter >= 0) {
              x = new_node(VAR);
              x->val = -parameter - 1;
          } else if (variable >= 0 && variable_is_array[variable]) {
              x = new_node(ARRAY_REF);
              x->val = variable_bases[variable];
          } else {
              x=new_node(VAR);
              x->val=variable_index(name);
          }
      }
  } else if (sym == INT || sym == CHAR_LITERAL) {
      x=new_node(CST);
      x->val=int_val;
      next_sym();
  } else if (sym == STRING_LITERAL) {
      int base = allocate_string_literal();
      next_sym();
      x = new_node(ARRAY_REF);
      x->val = base;
  } else x = paren_expr();
  return x;
}

int mathop() {
    if (sym==PLUS) { return ADD; }
    else if (sym==MINUS) { return SUB; }
    else if (sym==STAR)  { return MUL; }
    else if (sym==SLASH) { return DIV; }
    else if (sym==MODULO) { return MOD; }
    return 0;
}

/* <math> ::= <term> | <math> <math_op> <term> */
/* <math_op> ::= "+" | "-" | "*" | "/" | "%" */
node *sum() {
  node *x = term();
  while (mathop()) {
    x=gen(mathop(), x, 0);
    next_sym();
    x->o2=term();
  }
  return x;
}

/* <test> ::= <math> | <math> "<" <math> | <math> ">" <math> | <math> "==" <math> | <math> "!=" <math> */
node *test() {
  node *x = sum();
  if (sym == LESS) { next_sym(); return gen(LT, x, sum()); }
  if (sym == GRT)  { next_sym(); return gen(GT, x, sum()); }
  if (sym == EQ_SYM)   { next_sym(); return gen(EQ, x, sum()); }
  if (sym == NE_SYM)   { next_sym(); return gen(NE, x, sum()); }
  return x;
}

/* <expr> ::= <test> | <id> "=" <expr> */
node *expr() {
  node *x;
  if (sym != ID) { return test(); }
  x = test();
  if (((x->kind==VAR) || (x->kind==LOCAL) || (x->kind==ARRAY_GET)) && (sym==EQUAL)) {
      next_sym();
      if (x->kind == ARRAY_GET) {
          node *set = gen(ARRAY_SET, x->o1, expr());
          set->val = x->val;
          return set;
      }
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
  if (sym == INT_SYM && current_function >= 0) {
      next_sym();
      if (sym != ID) { syntax_error(); }
      while (1) {
          char name[sizeof(id_name)];
          strcpy(name, id_name);
          if (parameter_index(name) >= 0 || local_index(name) >= 0) {
              printf("-duplicate local variable %s-\n", name);
              exit(1);
          }
          struct function_s *function = &functions[current_function];
          if (function->num_locals >= MAX_LOCALS) { error("-too many local variables-"); }
          strcpy(function->local_names[function->num_locals++], name);
          next_sym();
          if (sym != COMMA) { break; }
          next_sym();
          if (sym != ID) { syntax_error(); }
      }
      expect_sym(SEMI);
      x = new_node(EMPTY);
  } else if (sym == RETURN_SYM) {
      next_sym();
      if (sym == SEMI) {
          if (current_function < 0 || functions[current_function].returns_value) {
              error("-return without a value requires a void function-");
          }
          x = new_node(VOID_RETURN);
      } else {
          if (current_function < 0 || !functions[current_function].returns_value) {
              error("-return value requires an int function-");
          }
          x = gen(RETURN_NODE, expr(), NULL);
      }
      expect_sym(SEMI);
  } else if (sym == IF_SYM) { /* "if" <paren_expr> <statement> */
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
        x =gen(SEQ, x, 0);
        x->o2=statement();
      }
      next_sym();
  } else { /* <expr> ";" */
      x = gen(EXPR, expr(), NULL);
      expect_sym(SEMI);
  }
  return x;
}

int returns_on_all_paths(node *x) {
    switch (x->kind) {
        case RETURN_NODE: return 1;
        case SEQ: return returns_on_all_paths(x->o1) || returns_on_all_paths(x->o2);
        case IF2:
            return returns_on_all_paths(x->o2) && returns_on_all_paths(x->o3);
        case DO: return returns_on_all_paths(x->o1);
        default: return 0;
    }
}

void parse_parameters(int function_index_value) {
    struct function_s *function = &functions[function_index_value];
    expect_sym(LPAR);
    if (sym == RPAR) {
        next_sym();
        return;
    }
    while (1) {
        expect_sym(INT_SYM);
        if (sym != ID) { syntax_error(); }
        for (int i = 0; i < function->num_parameters; i++) {
            if (strcmp(function->parameter_names[i], id_name) == 0) {
                printf("-duplicate parameter %s-\n", id_name);
                exit(1);
            }
        }
        if (function->num_parameters >= MAX_PARAMETERS) {
            error("-too many parameters-");
        }
        strcpy(function->parameter_names[function->num_parameters++], id_name);
        int parameter = function->num_parameters - 1;
        next_sym();
        if (sym == LBRACKET) {
            next_sym();
            expect_sym(RBRACKET);
            function->parameter_is_array[parameter] = 1;
        }
        if (sym != COMMA) { break; }
        next_sym();
    }
    expect_sym(RPAR);
}

void parse_variable_declarator(const char *name) {
    int size = 1;
    int is_array = 0;
    int inferred_size = 0;
    if (sym == LBRACKET) {
        is_array = 1;
        next_sym();
        if (sym == RBRACKET) {
            inferred_size = 1;
        } else {
            if (sym != INT) { syntax_error(); }
            size = int_val;
            next_sym();
        }
        expect_sym(RBRACKET);
    }
    int has_string_initializer = 0;
    if (sym == EQUAL) {
        next_sym();
        if (!is_array || sym != STRING_LITERAL) {
            error("-string initializer requires an array declaration-");
        }
        int required_size = string_length + 2;
        if (!inferred_size && size < required_size) {
            error("-array is too small for string initializer-");
        }
        if (inferred_size) { size = required_size; }
        has_string_initializer = 1;
    } else if (inferred_size) {
        error("-array size required unless initialized by a string-");
    }
    int variable = declare_variable(name, size, is_array);
    if (has_string_initializer) {
        int base = variable_bases[variable];
        globals[base] = string_length;
        for (int i = 0; i < string_length; i++) {
            globals[base + i + 1] = string_values[i];
        }
        next_sym();
    }
}

void variable_declarations(const char *first_name) {
    parse_variable_declarator(first_name);
    while (sym == COMMA) {
        next_sym();
        if (sym != ID) { syntax_error(); }
        char name[sizeof(id_name)];
        strcpy(name, id_name);
        next_sym();
        parse_variable_declarator(name);
    }
    expect_sym(SEMI);
}

void parse_function_definition(const char *name, int returns_value) {
    if (strcmp(name, "emit") == 0) {
        error("-emit is a built-in function-");
    }
    int index = function_index(name);
    if (functions[index].defined) {
        printf("-duplicate function %s-\n", name);
        exit(1);
    }
    functions[index].defined = 1;
    functions[index].returns_value = returns_value;
    parse_parameters(index);
    if (strcmp(name, "main") == 0) {
        if (functions[index].num_parameters != 0) { error("-main must not have parameters-"); }
        main_function = index;
    }
    expect_sym(LBRA);
    int previous_function = current_function;
    current_function = index;
    node *body = new_node(EMPTY);
    while (sym != RBRA) {
        if (sym == EOI) { syntax_error(); }
        body = gen(SEQ, body, NULL);
        body->o2 = statement();
    }
    functions[index].body = body;
    current_function = previous_function;
    if (returns_value && !returns_on_all_paths(body)) {
        printf("-int function %s does not return on all paths-\n", name);
        exit(1);
    }
    next_sym();
}

int argument_count(node *args) {
    int count = 0;
    while (args) { count++; args = args->o2; }
    return count;
}

void validate_expression(node *x, int allow_array_ref);

void validate_call(node *call) {
    if (call->val == BUILTIN_EMIT) {
        if (argument_count(call->o1) != 1) {
            error("-emit expects 1 argument-");
        }
        validate_expression(call->o1->o1, 0);
        return;
    }
    struct function_s *function = &functions[call->val];
    node *arg = call->o1;
    for (int i = 0; i < function->num_parameters; i++) {
        int is_array = arg && arg->o1 && arg->o1->kind == ARRAY_REF;
        if (is_array != function->parameter_is_array[i]) {
            printf("-argument %d to %s must be %s-\n", i + 1, function->name,
                   function->parameter_is_array[i] ? "an array reference" : "a scalar");
            exit(1);
        }
        validate_expression(arg->o1, function->parameter_is_array[i]);
        arg = arg->o2;
    }
}

void validate_expression(node *x, int allow_array_ref) {
    if (!x) { return; }
    if (x->kind == ARRAY_REF) {
        if (!allow_array_ref) { error("-array reference only valid as an array argument-"); }
        return;
    }
    if (x->kind == FUNC_CALL) {
        validate_call(x);
        return;
    }
    validate_expression(x->o1, 0);
    validate_expression(x->o2, 0);
    validate_expression(x->o3, 0);
}

void validate_node(node *x) {
    if (!x) { return; }
    if (x->kind == ARRAY_REF) {
        error("-array reference only valid as an array argument-");
    }
    if (x->kind == FUNC_CALL) {
        validate_call(x);
        return;
    }
    validate_node(x->o1);
    validate_node(x->o2);
    validate_node(x->o3);
}

/* <global and function definitions precede the required main function. */
node *defs(node *st) {
  next_sym();
  while (1) {
    if (sym==EOI) { break; }
    if (sym==LBRA) {
        if (main_function < 0) { error("-program must define main()-"); }
        syntax_error();
    }
    if (sym==VOID_SYM || sym==INT_SYM) {
      int returns_value = sym == INT_SYM;
      next_sym();
      if (sym != ID) { syntax_error(); }
      char name[sizeof(id_name)];
      strcpy(name, id_name);
      next_sym();
      if (sym == LPAR) {
          parse_function_definition(name, returns_value);
      } else if (returns_value) {
          variable_declarations(name);
      } else {
          syntax_error();
      }
      continue;
    }
    message("-def?-"); syntax_error();
  }
  return st;
}

/* <program> ::= <global_defs> <function_defs> <main_function> */
node *program() {
  node *prog = gen(PROG, NULL, NULL);
  defs(prog);
  if (main_function < 0) {
      error("-program must define main()-");
  }
  if (sym != EOI) { syntax_error(); }
  for (int i = 0; i < num_functions; i++) {
      if (!functions[i].defined) {
          printf("-undefined function %s-\n", functions[i].name);
          exit(1);
      }
  }
  for (int i = 0; i < num_function_calls; i++) {
      node *call = function_calls[i];
      if (call->val == BUILTIN_EMIT) { continue; }
      int expected = functions[call->val].num_parameters;
      int actual = argument_count(call->o1);
      if (actual != expected) {
          printf("-function %s expects %d argument(s), got %d-\n",
                 functions[call->val].name, expected, actual);
          exit(1);
      }
  }
  for (int i = 0; i < num_functions; i++) { validate_node(functions[i].body); }
  prog->o1 = functions[main_function].body;
  return prog;
}

/*---------------------------------------------------------------------------*/
/* Code generator. */
enum { IFETCH, ISTORE, IP1, IP2, IP4, IDROP, IADD, ISUB, IMUL, IDIV,
        IMOD,         ILT, IGT, IEQ, INE, JZ, JNZ, JMP, ICALL, IRET, HALT, IAFETCH, IASTORE,
        PLOAD, PSTORE, PAFETCH, PASTORE, LLOAD, LSTORE, SYS };

enum { SYS_EMIT };

typedef char code;
typedef unsigned char byte;
#define MAX_CODE 65536
code vm[MAX_CODE], *here = vm;

#define MAX_CALL_FIXUPS MAX_NODES
struct call_fixup_s { code *operand; int function_index; };
struct call_fixup_s call_fixups[MAX_CALL_FIXUPS];
int num_call_fixups;

void checkSize(int sz) { if (here + sz > vm + MAX_CODE) { error("-program too large-"); } }
void g(code c)   { checkSize(1); *here++ = c; }
void g2(short c) { checkSize(2); *(short *)here = c; here += 2; }
void g4(int c)   { checkSize(4); *(int *)here = c; here += 4; }

code *hole() {
    checkSize(2);
    code *p = here;
    here += 2;
    return p;
}
void fix(code *src, code *dst) {
    int off = (int)(dst - src);
    if (off < -32768 || off > 32767) { error("-jump too far-"); }
    *(short *)src = (short)off;
}

void emit_call(int index, int argc) {
    if (num_call_fixups >= MAX_CALL_FIXUPS) { error("-too many function calls-"); }
    g(ICALL);
    call_fixups[num_call_fixups].operand = here;
    call_fixups[num_call_fixups].function_index = index;
    num_call_fixups++;
    g2(0);
    g2(argc);
    g2(functions[index].num_locals);
}

void patch_calls() {
    for (int i = 0; i < num_call_fixups; i++) {
        unsigned int address = (unsigned int)functions[call_fixups[i].function_index].address;
        call_fixups[i].operand[0] = (code)(address & 0xff);
        call_fixups[i].operand[1] = (code)((address >> 8) & 0xff);
    }
}

void emit_const(int v) {
    if (BTWI(v,0,127)) { g(IP1); g(v); }
    else if (BTWI(v,128,32767)) { g(IP2); g2(v); }
    else { g(IP4); g4(v); }
}

/* Returns 1 and sets *out if x is a constant expression that can be folded. */
int fold(node *x, int *out) {
    int a, b;
    if (!x) { return 0; }
    if (x->kind == CST) { *out = x->val; return 1; }
    if (!fold(x->o1, &a) || !fold(x->o2, &b)) { return 0; }
    switch (x->kind) {
        case ADD: *out = a + b; return 1;
        case SUB: *out = a - b; return 1;
        case MUL: *out = a * b; return 1;
        case DIV: if (b == 0) { return 0; } *out = a / b; return 1;
        case MOD: if (b == 0) { return 0; } *out = a % b; return 1;
        case LT:  *out = a < b;  return 1;
        case GT:  *out = a > b;  return 1;
        case EQ:  *out = a == b; return 1;
        case NE:  *out = a != b; return 1;
        default: return 0;
    }
}

void c(node *x) {
    code *p1, *p2;
    int folded;
    if (!x) { return; }
    if (x->kind != CST && fold(x, &folded)) { emit_const(folded); return; }
    switch (x->kind) {
        case VAR:
            if (x->val < 0) { g(PLOAD); g2(-x->val - 1); }
            else { g(IFETCH); g2(x->val); }
            break;
        case LOCAL: g(LLOAD); g2(x->val); break;
        case ARRAY_REF:
            if (x->val < 0) { g(PLOAD); g2(-x->val - 1); }
            else { g(IP2); g2(x->val); }
            break;
        case ARRAY_GET:
            c(x->o1);
            if (x->val < 0) { g(PAFETCH); g2(-x->val - 1); }
            else { g(IAFETCH); g2(x->val); }
            break;
        case ARRAY_SET:
            c(x->o1); c(x->o2);
            if (x->val < 0) { g(PASTORE); g2(-x->val - 1); }
            else { g(IASTORE); g2(x->val); }
            break;
        case CST  : emit_const(x->val); break;
        case ADD  : c(x->o1);  c(x->o2); g(IADD); break;
        case MUL  : c(x->o1);  c(x->o2); g(IMUL); break;
        case SUB  : c(x->o1);  c(x->o2); g(ISUB); break;
        case DIV  : c(x->o1);  c(x->o2); g(IDIV); break;
        case MOD  : c(x->o1);  c(x->o2); g(IMOD); break;
        case LT   : c(x->o1);  c(x->o2); g(ILT); break;
        case GT   : c(x->o1);  c(x->o2); g(IGT); break;
        case EQ   : c(x->o1);  c(x->o2); g(IEQ); break;
        case NE   : c(x->o1);  c(x->o2); g(INE); break;
        case SET  : c(x->o2);
            if (x->o1->kind == LOCAL) { g(LSTORE); g2(x->o1->val); }
            else if (x->o1->val < 0) { g(PSTORE); g2(-x->o1->val - 1); }
            else { g(ISTORE); g2(x->o1->val); }
            break;
        case IF1  : c(x->o1);  g(JZ); p1=hole(); c(x->o2); fix(p1,here); break;
        case IF2  : c(x->o1);  g(JZ); p1=hole(); c(x->o2); g(JMP); p2=hole();
                    fix(p1,here); c(x->o3); fix(p2,here); break;
        case WHILE: p1=here; c(x->o1); g(JZ); p2=hole(); c(x->o2);
                    g(JMP); fix(hole(),p1); fix(p2,here); break;
        case DO   : p1=here; c(x->o1); c(x->o2); g(JNZ); fix(hole(),p1); break;
        case EMPTY: break;
        case SEQ  : c(x->o1); c(x->o2); break;
        case EXPR:
            if (x->o1->kind == FUNC_CALL && x->o1->val != BUILTIN_EMIT) {
                c(x->o1->o1);
                emit_call(x->o1->val, argument_count(x->o1->o1));
            } else {
                c(x->o1);
            }
            g(IDROP);
            break;
        case ARG_LIST: c(x->o1); c(x->o2); break;
        case RETURN_NODE: c(x->o1); g(IRET); break;
        case VOID_RETURN: g(IP1); g(0); g(IRET); break;
        case PROG :
            emit_call(main_function, 0);
            g(IDROP);
            g(HALT);
            for (int i = 0; i < num_functions; i++) {
                functions[i].address = (int)(here - vm);
                c(functions[i].body);
                g(IP1); g(0); g(IRET);
            }
            patch_calls();
            break;
        case FUNC_CALL:
            if (x->val == BUILTIN_EMIT) {
                c(x->o1->o1);
                g(SYS); g(SYS_EMIT);
                break;
            }
            if (!functions[x->val].returns_value) {
                printf("-void function %s used as a value-\n", functions[x->val].name);
                exit(1);
            }
            c(x->o1);
            emit_call(x->val, argument_count(x->o1));
            break;
    }
}

void compile() { c(program());}

/*---------------------------------------------------------------------------*/
/* Virtual machine. */
#define ACASE    goto again; case
#define TOS      st[sp]
#define NOS      st[sp-1]
struct return_s { code *pc; int frame_base; };

short f2(code *a) { return *(short *)a; }

void run() {
    /* Each frame's parameters and locals sit on st[] starting at frame_base. */
    int st[1000], sp = 0, frame_base = 1, return_sp = 0;
    code *pc = vm;
    struct return_s return_stack[1000];

    again:
    switch (*pc++) {
        case  IFETCH: st[++sp] = globals[f2(pc)]; pc +=2;
        ACASE ISTORE: globals[f2(pc)] = st[sp]; pc += 2;
        ACASE PLOAD : st[++sp] = st[frame_base + f2(pc)]; pc += 2;
        ACASE PSTORE: st[frame_base + f2(pc)] = st[sp]; pc += 2;
        ACASE LLOAD : st[++sp] = st[frame_base + f2(pc)]; pc += 2;
        ACASE LSTORE: st[frame_base + f2(pc)] = st[sp]; pc += 2;
        ACASE PAFETCH: st[sp] = globals[st[frame_base + f2(pc)] + st[sp]]; pc += 2;
        ACASE PASTORE: globals[st[frame_base + f2(pc)] + st[sp-1]] = st[sp];
                        st[sp-1] = st[sp]; --sp; pc += 2;
        ACASE IAFETCH: st[sp] = globals[f2(pc) + st[sp]]; pc += 2;
        ACASE IASTORE: globals[f2(pc) + st[sp-1]] = st[sp];
                        st[sp-1] = st[sp]; --sp; pc += 2;
        ACASE IP1   : st[++sp] = *(pc++);
        ACASE IP2   : st[++sp] = f2(pc); pc += 2;
        ACASE IP4   : st[++sp] = *(int *)pc; pc += 4;
        ACASE IDROP : --sp;
        ACASE IADD  : NOS += TOS; --sp;
        ACASE ISUB  : NOS -= TOS; --sp;
        ACASE IMUL  : NOS *= TOS; --sp;
        ACASE IDIV  : if (TOS == 0) { error("-divide by zero-"); }
                      NOS /= TOS; --sp;
        ACASE IMOD  : if (TOS == 0) { error("-modulo by zero-"); }
                      NOS %= TOS; --sp;
        ACASE ILT   : NOS =  (NOS<TOS)?1:0; --sp;
        ACASE IGT   : NOS =  (NOS>TOS)?1:0; --sp;
        ACASE IEQ   : NOS =  (NOS==TOS)?1:0; --sp;
        ACASE INE   : NOS =  (NOS!=TOS)?1:0; --sp;
        ACASE SYS   : { int op = *(pc++);
                        if (op == SYS_EMIT) { TOS = putchar((char)TOS); }
                        // more here later
                      }
        ACASE JMP   : pc += (short)f2(pc);
        ACASE JZ    : if (st[sp--] == 0) pc += (short)f2(pc); else pc += 2;
        ACASE JNZ   : if (st[sp--] != 0) pc += (short)f2(pc); else pc += 2;
        ACASE ICALL : { int argc = f2(pc + 2);
                        int locals = f2(pc + 4);
                        if (return_sp >= 1000) { error("-call stack overflow-"); }
                        if (sp < argc || sp + locals >= 1000) { error("-call frame overflow-"); }
                        return_stack[return_sp].pc = pc + 6;
                        return_stack[return_sp].frame_base = frame_base;
                        return_sp++;
                        frame_base = sp - argc + 1;
                        sp += locals;
                        pc = vm + f2(pc);
                      }
        ACASE IRET : if (return_sp) {
                          st[frame_base] = TOS;
                          sp = frame_base;
                          return_sp--;
                          pc = return_stack[return_sp].pc;
                          frame_base = return_stack[return_sp].frame_base;
                      } else { return; }
        ACASE HALT  : return;
    }
}
