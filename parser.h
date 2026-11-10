//--------------------------------------------------------------------------
// Parser for tiny-c

enum { VAR, CST, ADD, SUB, MUL, DIV, MOD, LT, GT, EQ, NE, SET, FUNC_CALL,
       ARRAY_GET, ARRAY_SET, ARRAY_REF, LOCAL, RETURN_NODE, VOID_RETURN, ARG_LIST,
       IF1, IF2, WHILE, DO, EMPTY, SEQ, EXPR, PROG };

struct node_s { int kind; struct node_s *o1, *o2, *o3; int val; };
typedef struct node_s node;
int num_nodes = 0;
node nodes[MAX_NODES];

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
        fail("-expected symbol[%d],not[%d]-\n", exp, sym);
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
              fail("-duplicate local variable %s-\n", name);
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
                fail("-duplicate parameter %s-\n", id_name);
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
        fail("-duplicate function %s-\n", name);
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
        fail("-int function %s does not return on all paths-\n", name);
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
            fail("-argument %d to %s must be %s-\n", i + 1, function->name,
                 function->parameter_is_array[i] ? "an array reference" : "a scalar");
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
    fail("-def?-\n");
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
          fail("-undefined function %s-\n", functions[i].name);
      }
  }
  for (int i = 0; i < num_function_calls; i++) {
      node *call = function_calls[i];
      if (call->val == BUILTIN_EMIT) { continue; }
      int expected = functions[call->val].num_parameters;
      int actual = argument_count(call->o1);
      if (actual != expected) {
          fail("-function %s expects %d argument(s), got %d-\n",
               functions[call->val].name, expected, actual);
      }
  }
  for (int i = 0; i < num_functions; i++) { validate_node(functions[i].body); }
  prog->o1 = functions[main_function].body;
  return prog;
}
