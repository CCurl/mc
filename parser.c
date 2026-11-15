//--------------------------------------------------------------------------
// Parser for tiny-c

#include "tiny-c.h"

// -------------------------------------------------
int numNodes = 0;
node nodes[MAX_NODES];

struct function_s functions[MAX_FUNCTIONS];
int numFunctions;
int currentFunction = -1;
int mainFunction = -1;

int parameterIndex(const char *name) {
    if (currentFunction >= 0) {
        struct function_s *function = &functions[currentFunction];
        for (int i = 0; i < function->numParameters; i++) {
            if (strcmp(function->parameterNames[i], name) == 0) { return i; }
        }
    }
    return -1;
}

int localIndex(const char *name) {
    if (currentFunction >= 0) {
        struct function_s *function = &functions[currentFunction];
        for (int i = 0; i < function->numLocals; i++) {
            if (strcmp(function->localNames[i], name) == 0) { return i; }
        }
    }
    return -1;
}

int currentParameterIsArray(int parameter) {
    return currentFunction >= 0 &&
           functions[currentFunction].parameterIsArray[parameter];
}

static int functionIndex(const char *name) {
    for (int i = 0; i < numFunctions; i++) {
        if (strcmp(functions[i].name, name) == 0) { return i; }
    }
    if (numFunctions >= MAX_FUNCTIONS) { fail("-too many functions-"); }
    strcpy(functions[numFunctions].name, name);
    functions[numFunctions].body = NULL;
    functions[numFunctions].address = 0;
    functions[numFunctions].declared = 0;
    functions[numFunctions].defined = 0;
    functions[numFunctions].returnsValue = 0;
    functions[numFunctions].returnType = TYPE_VOID;
    functions[numFunctions].numParameters = 0;
    functions[numFunctions].numLocals = 0;
    return numFunctions++;
}

static node *newNode(int k) {
    if (MAX_NODES <= numNodes) { fail("-too many syntax nodes-"); }
    node *x = &nodes[numNodes++];
    x->kind = k;
    x->o1 = x->o2 = x->o3 = NULL;
    x->val = 0;
    x->type = TYPE_INT;
    return x;
}

static node *gen(int k, node *o1, node *o2) {
    node *x=newNode(k);
    x->o1=o1; x->o2=o2;
    return x;
}

static node *byteConversion(node *value) {
    node *conversion = gen(BYTE_CONVERT, value, NULL);
    conversion->type = TYPE_BYTE;
    return conversion;
}

// The type named by a void, int, or byte keyword.
static int keywordType(int keyword) {
    if (keyword == VOID_SYM) { return TYPE_VOID; }
    return keyword == BYTE_SYM ? TYPE_BYTE : TYPE_INT;
}

static void expectSym(int exp) {
    if (sym != exp) {
        fail("-expected symbol[%d],not[%d]-\n", exp, sym);
    }
    nextSym();
}

static node *callArguments(const char *name) {
    int index = strcmp(name, "emit") == 0 ? BUILTIN_EMIT : functionIndex(name);
    if (index != BUILTIN_EMIT && !functions[index].declared) {
        fail("-function %s used before declaration-\n", name);
    }
    node *args = NULL;
    node **tail = &args;
    int count = 0;
    expectSym(LPAR);
    if (sym != RPAR) {
        do {
            node *arg = expr();
            *tail = gen(ARG_LIST, arg, NULL);
            tail = &(*tail)->o2;
            count++;
            if (sym != COMMA) { break; }
            nextSym();
        } while (1);
    }
    expectSym(RPAR);
    if (index == BUILTIN_EMIT) {
        if (count != 1 || args->o1->kind == ARRAY_REF) {
            fail("-emit expects one scalar argument-");
        }
    } else {
        struct function_s *function = &functions[index];
        if (count != function->numParameters) {
            fail("-function %s expects %d argument(s), got %d-\n", name, function->numParameters, count);
        }
        node *arg = args;
        for (int i = 0; i < count; i++, arg = arg->o2) {
            int isArray = arg->o1->kind == ARRAY_REF;
            if (isArray != function->parameterIsArray[i]) {
                fail("-argument %d to %s must be %s-\n", i + 1, name,
                     function->parameterIsArray[i] ? "an array reference" : "a scalar");
            }
            if (isArray && arg->o1->type != function->parameterTypes[i]) {
                fail("-argument %d to %s must be %s array-\n", i + 1, name,
                     function->parameterTypes[i] == TYPE_BYTE ? "a byte" : "an int");
            }
            if (!isArray && function->parameterTypes[i] == TYPE_BYTE) {
                arg->o1 = byteConversion(arg->o1);
            }
        }
    }
    node *call = newNode(FUNC_CALL);
    call->val = index;
    call->o1 = args;
    return call;
}

static int arrayElementType(const char *name) {
    int parameter = parameterIndex(name);
    if (parameter >= 0) {
        return functions[currentFunction].parameterTypes[parameter];
    }
    return variableTypes[findVariable(name)];
}

/* <term> ::= <id> | <id> "(" [ <expr> { "," <expr> } ] ")" | <id> "[" <expr> "]" | <int> | <char> | <string> | <paren_expr> | "-" <term> */
/* The scalar variable that ++ or -- changes: a local, a parameter, or a global. */
static node *scalarTarget(const char *name) {
    int local = localIndex(name);
    int parameter = parameterIndex(name);
    node *x;
    if (local >= 0) {
        x = newNode(LOCAL);
        x->val = functions[currentFunction].numParameters + local;
        x->type = functions[currentFunction].localTypes[local];
    } else if (parameter >= 0) {
        x = newNode(VAR);
        x->val = variableIndex(name);
        x->type = functions[currentFunction].parameterTypes[parameter];
    } else {
        x = newNode(VAR);
        x->val = variableIndex(name);
        x->type = variableTypes[findVariable(name)];
    }
    return x;
}

static node *term() {
  node *x = NULL;
  if (sym == MINUS) {
      nextSym();
      x = newNode(CST);
      x->val = 0;
      return gen(SUB, x, term());
  } else if (sym == PLUS_PLUS || sym == MINUS_MINUS) {
      int delta = sym == PLUS_PLUS ? 1 : -1;
      nextSym();
      if (sym != ID) { syntaxError(); }
      char name[sizeof(idName)];
      strcpy(name, idName);
      nextSym();
      x = newNode(PRE_INC);
      x->o1 = scalarTarget(name);
      x->val = delta;
      x->type = x->o1->type;
  } else if (sym == ID || sym == EMIT_SYM) {
      char name[sizeof(idName)];
      strcpy(name, idName);
      int isEmit = sym == EMIT_SYM;
      nextSym();
      if (sym == LPAR) {
          x = callArguments(name);
      } else if (isEmit) {
          fail("-emit is a built-in function-");
      } else if (sym == LBRACKET) {
          int base = arrayIndex(name);
          nextSym();
          node *index = expr();
          expectSym(RBRACKET);
          x = gen(ARRAY_GET, index, NULL);
          x->val = base;
          x->type = arrayElementType(name);
      } else {
          int variable = findVariable(name);
          int parameter = parameterIndex(name);
          int local = localIndex(name);
          if (local >= 0) {
              x = newNode(LOCAL);
              x->val = functions[currentFunction].numParameters + local;
              x->type = functions[currentFunction].localTypes[local];
          } else if (parameter >= 0 &&
                     functions[currentFunction].parameterIsArray[parameter]) {
            x = newNode(ARRAY_REF);
            x->val = arrayReference(name);
            x->type = functions[currentFunction].parameterTypes[parameter];
          } else if (parameter >= 0) {
            x = newNode(VAR);
            x->val = -parameter - 1;
            x->type = functions[currentFunction].parameterTypes[parameter];
          } else if (variable >= 0 && variableIsArray[variable]) {
            x = newNode(ARRAY_REF);
            x->val = variableBases[variable];
            x->type = variableTypes[variable];
          } else {
            x=newNode(VAR);
            x->val=variableIndex(name);
            x->type = variableTypes[variable];
          }
          if (sym == PLUS_PLUS || sym == MINUS_MINUS) {
              if (x->kind != VAR && x->kind != LOCAL) { fail("-++ and -- need a scalar variable-"); }
              node *post = newNode(POST_INC);
              post->o1 = x;
              post->val = sym == PLUS_PLUS ? 1 : -1;
              post->type = x->type;
              nextSym();
              x = post;
          }
      }
  } else if (sym == INT || sym == CHAR_LITERAL) {
      x=newNode(CST);
      x->val=intVal;
      nextSym();
  } else if (sym == STRING_LITERAL) {
      int base = allocateStringLiteral();
      nextSym();
      x = newNode(ARRAY_REF);
      x->val = base;
      x->type = TYPE_BYTE;
  } else x = parenExpr();
  return x;
}

static int mathop() {
    if (sym==PLUS) { return ADD; }
    else if (sym==MINUS) { return SUB; }
    else if (sym==STAR)  { return MUL; }
    else if (sym==SLASH) { return DIV; }
    else if (sym==MODULO) { return MOD; }
    return 0;
}

/* <math> ::= <term> | <math> <math_op> <term> */
/* <math_op> ::= "+" | "-" | "*" | "/" | "%" */
static node *sum() {
  node *x = term();
  while (mathop()) {
    x=gen(mathop(), x, 0);
    nextSym();
    x->o2=term();
  }
  return x;
}

/* <expr> ::= <math> | <math> "<" <math> | <math> ">" <math> | <math> "<=" <math> | <math> ">=" <math> | <math> "==" <math> | <math> "!=" <math> */
node *expr() {
  node *x = sum();
  if (sym == LESS)     { nextSym(); return gen(LT, x, sum()); }
  if (sym == GRT)      { nextSym(); return gen(GT, x, sum()); }
  if (sym == LESS_EQ)  { nextSym(); return gen(LE, x, sum()); }
  if (sym == GRT_EQ)   { nextSym(); return gen(GE, x, sum()); }
  if (sym == EQ_SYM)   { nextSym(); return gen(EQ, x, sum()); }
  if (sym == NE_SYM)   { nextSym(); return gen(NE, x, sum()); }
  return x;
}

/* <assignment> ::= <id> "=" <expr> | <id> "[" <expr> "]" "=" <expr> */
static node *assignment(node *target) {
  if (target->kind != VAR && target->kind != LOCAL && target->kind != ARRAY_GET) {
      fail("-left side of = must be a variable or array element-");
  }
  nextSym();
  node *value = expr();
  if (target->type == TYPE_BYTE) { value = byteConversion(value); }
  if (target->kind == ARRAY_GET) {
      node *set = gen(ARRAY_SET, target->o1, value);
      set->val = target->val;
      set->type = target->type;
      return set;
  }
  node *set = gen(SET, target, value);
  set->type = target->type;
  return set;
}

/* <paren_expr> ::= "(" <expr> ")" */
node *parenExpr() {
  node *x;
  expectSym(LPAR);
  x = expr();
  expectSym(RPAR);
  return x;
}

static node *statement() {
  node *x;
  if ((sym == INT_SYM || sym == BYTE_SYM) && currentFunction >= 0) {
      int localType = keywordType(sym);
      nextSym();
      if (sym != ID) { syntaxError(); }
      x = newNode(EMPTY);
      while (1) {
          char name[sizeof(idName)];
          strcpy(name, idName);
          if (parameterIndex(name) >= 0 || localIndex(name) >= 0) {
              fail("-duplicate local variable %s-\n", name);
          }
          struct function_s *function = &functions[currentFunction];
          if (function->numLocals >= MAX_LOCALS) { fail("-too many local variables-"); }
          int local = function->numLocals;
          strcpy(function->localNames[local], name);
          function->localTypes[function->numLocals++] = localType;
          nextSym();
          if (sym == EQUAL) {
              nextSym();
              node *value = expr();
              if (localType == TYPE_BYTE) { value = byteConversion(value); }
              node *target = newNode(LOCAL);
              target->val = function->numParameters + local;
              target->type = localType;
              node *set = gen(SET, target, value);
              set->type = localType;
              x = gen(SEQ, x, 0);
              x->o2 = gen(EXPR, set, NULL);
          }
          if (sym != COMMA) { break; }
          nextSym();
          if (sym != ID) { syntaxError(); }
      }
      expectSym(SEMI);
  } else if (sym == RETURN_SYM) {
      nextSym();
      if (sym == SEMI) {
          if (currentFunction < 0 || functions[currentFunction].returnsValue) {
              fail("-return without a value requires a void function-");
          }
          x = newNode(VOID_RETURN);
      } else {
          if (currentFunction < 0 || !functions[currentFunction].returnsValue) {
              fail("-return value requires a non-void function-");
          }
          node *value = expr();
          if (functions[currentFunction].returnType == TYPE_BYTE) {
              value = byteConversion(value);
          }
          x = gen(RETURN_NODE, value, NULL);
      }
      expectSym(SEMI);
  } else if (sym == IF_SYM) { /* "if" <paren_expr> <statement> */
      x = newNode(IF1);
      nextSym();
      x->o1 = parenExpr();
      x->o2 = statement();
      if (sym == ELSE_SYM) { /* ... "else" <statement> */
          x->kind = IF2;
          nextSym();
          x->o3 = statement();
      }
  } else if (sym == WHILE_SYM) { /* "while" <paren_expr> <statement> */
     x=newNode(WHILE);
     nextSym();
     x->o1=parenExpr();
     x->o2=statement();
  } else if (sym == DO_SYM) { /* "do" <statement> "while" <paren_expr> ";" */
      x = newNode(DO);
      nextSym();
      x->o1 = statement();
      expectSym(WHILE_SYM);
      x->o2 = parenExpr();
      expectSym(SEMI);
  } else if (sym == SEMI) { /* ";" */
      x = newNode(EMPTY);
      nextSym();
  } else if (sym == LBRA) { /* "{" <statement> "}" */
      x = newNode(EMPTY);
      nextSym();
      while (sym != RBRA) {
        x =gen(SEQ, x, 0);
        x->o2=statement();
      }
      nextSym();
  } else { /* <expr> ";" | <assignment> ";" */
      node *value = expr();
      if (sym == EQUAL) { value = assignment(value); }
      x = gen(EXPR, value, NULL);
      expectSym(SEMI);
  }
  return x;
}

static int returnsOnAllPaths(node *x) {
    switch (x->kind) {
        case RETURN_NODE: return 1;
        case SEQ: return returnsOnAllPaths(x->o1) || returnsOnAllPaths(x->o2);
        case IF2:
            return returnsOnAllPaths(x->o2) && returnsOnAllPaths(x->o3);
        case DO: return returnsOnAllPaths(x->o1);
        default: return 0;
    }
}

static void parseParameters(struct function_s *function) {
    expectSym(LPAR);
    if (sym == RPAR) {
        nextSym();
        return;
    }
    while (1) {
        if (sym != INT_SYM && sym != BYTE_SYM) { syntaxError(); }
        int parameterType = keywordType(sym);
        nextSym();
        if (sym != ID) { syntaxError(); }
        for (int i = 0; i < function->numParameters; i++) {
            if (strcmp(function->parameterNames[i], idName) == 0) {
                fail("-duplicate parameter %s-\n", idName);
            }
        }
        if (function->numParameters >= MAX_PARAMETERS) {
            fail("-too many parameters-");
        }
        strcpy(function->parameterNames[function->numParameters++], idName);
        int parameter = function->numParameters - 1;
        function->parameterTypes[parameter] = parameterType;
        nextSym();
        if (sym == LBRACKET) {
            nextSym();
            expectSym(RBRACKET);
            function->parameterIsArray[parameter] = 1;
        }
        if (sym != COMMA) { break; }
        nextSym();
    }
    expectSym(RPAR);
}

static void parseVariableDeclarator(const char *name, int type) {
    int size = 1;
    int isArray = 0;
    int inferredSize = 0;
    if (sym == LBRACKET) {
        isArray = 1;
        nextSym();
        if (sym == RBRACKET) {
            inferredSize = 1;
        } else {
            if (sym != INT) { syntaxError(); }
            size = intVal;
            nextSym();
        }
        expectSym(RBRACKET);
    }
    int hasScalarInitializer = 0;
    int scalarValue = 0;
    int hasStringInitializer = 0;
    if (sym == EQUAL && !isArray) {
        nextSym();
        node *value = expr();
        if (type == TYPE_BYTE) { value = byteConversion(value); }
        if (!fold(value, &scalarValue)) { fail("-global initializer must be constant-"); }
        hasScalarInitializer = 1;
    } else if (sym == EQUAL) {
        nextSym();
        if (type != TYPE_BYTE || sym != STRING_LITERAL) {
            fail("-string initializer requires a byte array-");
        }
        int requiredSize = stringLength + STRING_OVERHEAD;
        if (!inferredSize && size < requiredSize) {
            fail("-array is too small for string initializer-");
        }
        if (inferredSize) { size = requiredSize; }
        hasStringInitializer = 1;
    } else if (inferredSize) {
        fail("-array size required unless initialized by a string-");
    }
    int variable = declareVariable(name, size, isArray, type);
    if (hasScalarInitializer) {
        int base = variableBases[variable];
        if (type == TYPE_BYTE) { globalMem[base] = scalarValue; }
        else { storeInt(base, scalarValue); }
    }
    if (hasStringInitializer) {
        storeStringBytes(variableBases[variable]);
        nextSym();
    }
}

static void variableDeclarations(const char *firstName, int type) {
    parseVariableDeclarator(firstName, type);
    while (sym == COMMA) {
        nextSym();
        if (sym != ID) { syntaxError(); }
        char name[sizeof(idName)];
        strcpy(name, idName);
        nextSym();
        parseVariableDeclarator(name, type);
    }
    expectSym(SEMI);
}

static int sameSignature(struct function_s *declared, struct function_s *header) {
    if (declared->returnType != header->returnType ||
        declared->numParameters != header->numParameters) {
        return 0;
    }
    for (int i = 0; i < header->numParameters; i++) {
        if (declared->parameterTypes[i] != header->parameterTypes[i] ||
            declared->parameterIsArray[i] != header->parameterIsArray[i]) {
            return 0;
        }
    }
    return 1;
}

// A header that repeats an earlier declaration must match it.
static int parseHeader(const char *name, int returnType) {
    if (strcmp(name, "emit") == 0) {
        fail("-emit is a built-in function-");
    }
    int index = functionIndex(name);
    struct function_s *function = &functions[index];
    struct function_s previous = *function;
    function->returnType = returnType;
    function->returnsValue = returnType != TYPE_VOID;
    function->numParameters = 0;
    memset(function->parameterIsArray, 0, sizeof(function->parameterIsArray));
    parseParameters(function);
    if (previous.declared && !sameSignature(&previous, function)) {
        fail("-declaration of %s does not match earlier declaration-\n", name);
    }
    function->declared = 1;
    return index;
}

// Handles a prototype (ending in ';') or a function definition.
static void parseFunctionDefinition(const char *name, int returnType) {
    int index = parseHeader(name, returnType);
    if (sym == SEMI) {
        nextSym();
        return;
    }
    if (functions[index].defined) {
        fail("-duplicate function %s-\n", name);
    }
    functions[index].defined = 1;
    if (strcmp(name, "main") == 0) {
        if (returnType == TYPE_BYTE) { fail("-main must return int or void-"); }
        if (functions[index].numParameters != 0) { fail("-main must not have parameters-"); }
        mainFunction = index;
    }
    expectSym(LBRA);
    int previousFunction = currentFunction;
    currentFunction = index;
    node *body = newNode(EMPTY);
    while (sym != RBRA) {
        if (sym == EOI) { syntaxError(); }
        body = gen(SEQ, body, NULL);
        body->o2 = statement();
    }
    functions[index].body = body;
    currentFunction = previousFunction;
    if (returnType != TYPE_VOID && !returnsOnAllPaths(body)) {
        fail("-non-void function %s does not return on all paths-\n", name);
    }
    nextSym();
}

// An array reference is valid only as a direct array argument, which callArguments() has already checked.
static void checkArrayRefs(node *x) {
    if (!x) { return; }
    if (x->kind == ARRAY_REF) {
        fail("-array reference only valid as an array argument-");
    }
    if (x->kind == FUNC_CALL) {
        for (node *arg = x->o1; arg; arg = arg->o2) {
            if (arg->o1->kind != ARRAY_REF) { checkArrayRefs(arg->o1); }
        }
        return;
    }
    checkArrayRefs(x->o1);
    checkArrayRefs(x->o2);
    checkArrayRefs(x->o3);
}

/* <defs> ::= { <var_def> | <func_def> }. Globals and functions may appear in any order; main must be defined somewhere. */
static node *defs(node *st) {
  nextSym();
  while (1) {
    if (sym==EOI) { break; }
    if (sym==LBRA) {
        if (mainFunction < 0) { fail("-program must define main()-"); }
        syntaxError();
    }
    if (sym==VOID_SYM || sym==INT_SYM || sym==BYTE_SYM) {
      int type = keywordType(sym);
      nextSym();
      if (sym != ID) { syntaxError(); }
      char name[sizeof(idName)];
      strcpy(name, idName);
      nextSym();
      if (sym == LPAR) {
          parseFunctionDefinition(name, type);
      } else if (type != TYPE_VOID) {
          variableDeclarations(name, type);
      } else {
          syntaxError();
      }
      continue;
    }
    fail("-def?-\n");
  }
  return st;
}

/* <program> ::= <defs> */
node *program() {
  node *prog = gen(PROG, NULL, NULL);
  defs(prog);
  if (mainFunction < 0) {
      fail("-program must define main()-");
  }
  if (sym != EOI) { syntaxError(); }
  for (int i = 0; i < numFunctions; i++) {
      if (!functions[i].defined) {
          fail("-undefined function %s-\n", functions[i].name);
      }
  }
  for (int i = 0; i < numFunctions; i++) { checkArrayRefs(functions[i].body); }
  prog->o1 = functions[mainFunction].body;
  return prog;
}
