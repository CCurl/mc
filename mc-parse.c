//--------------------------------------------------------------------------
// Parser for min-c

#include "mc.h"

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

char variableNames[MAX_GLOBAL_NAMES][ID_NAME_LENGTH];
int variableTypes[MAX_GLOBAL_NAMES];
int variableBases[MAX_GLOBAL_NAMES];
int variableIsArray[MAX_GLOBAL_NAMES];
int numVariables;
int numGlobalBytes;
int numZeroedBytes;

int findVariable(const char *name) {
    for (int i = 0; i < numVariables; i++) {
        if (strcmp(variableNames[i], name) == 0) { return i; }
    }
    return -1;
}

// Initialized globals grow up from LOAD_BASE. Zero-filled globals grow down from the top of the global area,
// so their addresses stay fixed as more are declared, and they are not stored in the executable.
int declareVariable(const char *name, int size, int isArray, int type, int zeroed) {
    int index = findVariable(name);
    qFail(index >= 0, "-duplicate variable %s-\n", name);
    qFail((size <= 0), "-array size must be positive-");
    int elementSize = type == TYPE_BYTE ? 1 : 4;
    qFail(numVariables >= MAX_GLOBAL_NAMES, "-too many variables-");
    qFail(size > (MAX_GLOBAL_BYTES - numGlobalBytes - numZeroedBytes) / elementSize, "-too many variables-");
    strcpy(variableNames[numVariables], name);
    variableIsArray[numVariables] = isArray;
    variableTypes[numVariables] = type;
    if (zeroed) {
        numZeroedBytes += size * elementSize;
        variableBases[numVariables] = LOAD_BASE + MAX_GLOBAL_BYTES - numZeroedBytes;
    } else {
        variableBases[numVariables] = LOAD_BASE + numGlobalBytes;
        numGlobalBytes += size * elementSize;
    }
    return numVariables++;
}

void storeStringBytes(int base) {
    *MEM_AT(base) = stringLength & 0xff;
    *MEM_AT(base + 1) = (stringLength >> 8) & 0xff;
    for (int i = 0; i < stringLength; i++) {
        *MEM_AT(base + i + 2) = stringValues[i];
    }
}

int allocateStringLiteral() {
    int size = stringLength + STRING_OVERHEAD;
    qFail(size > (MAX_GLOBAL_BYTES - numGlobalBytes - numZeroedBytes), "-too much global storage-");
    int base = LOAD_BASE + numGlobalBytes;
    numGlobalBytes += size;
    storeStringBytes(base);
    return base;
}

int variableIndex(const char *name) {
    int parameter = parameterIndex(name);
    if (parameter >= 0) {
        qFail(currentParameterIsArray(parameter), "-array parameter %s requires an index-\n", name);
        return -parameter - 1;
    }
    int index = findVariable(name);
    qFail(index < 0, "-undeclared variable %s-\n", name);
    qFail(variableIsArray[index], "-array %s requires an index-\n", name);
    return variableBases[index];
}

int arrayIndex(const char *name) {
    int local = localIndex(name);
    if (local >= 0) {
        struct function_s *function = &functions[currentFunction];
        qFail(!function->localIsArray[local], "-local variable %s is not an array-\n", name);
        return -(function->numParameters + local) - 1;
    }
    int parameter = parameterIndex(name);
    if (parameter >= 0) {
        qFail(!currentParameterIsArray(parameter), "-parameter %s is not an array-\n", name);
        return -parameter - 1;
    }
    int index = findVariable(name);
    qFail((index < 0), "-undeclared variable %s-\n", name);
    qFail(!variableIsArray[index], "-variable %s is not an array-\n", name);
    return variableBases[index];
}

static int functionIndex(const char *name) {
    for (int i = 0; i < numFunctions; i++) {
        if (strcmp(functions[i].name, name) == 0) { return i; }
    }
    qFail((numFunctions >= MAX_FUNCTIONS), "-too many functions-");
    struct function_s *function = &functions[numFunctions];
    memset(function, 0, sizeof(*function));
    strcpy(function->name, name);
    function->returnType = TYPE_VOID;
    return numFunctions++;
}

static node *newNode(int k) {
    qFail((MAX_NODES <= numNodes), "-too many syntax nodes-");
    node *x = &nodes[numNodes++];
    memset(x, 0, sizeof(*x));
    x->kind = k;
    return x;
}

static node *constant(int value) {
    node *x = newNode(CST);
    x->val = value;
    return x;
}

// Replaces an operator node whose operands are all constants with the constant result.
static node *foldNode(node *x) {
    node *a = x->o1, *b = x->o2;
    if (x->kind == BYTE_CONVERT) { return a->kind == CST ? constant((byte)a->val) : x; }
    if (x->kind == BIT_NOT) { return a->kind == CST ? constant(~a->val) : x; }
    if (!a || !b || a->kind != CST || b->kind != CST) { return x; }
    switch (x->kind) {
        case ADD: return constant(a->val + b->val);
        case SUB: return constant(a->val - b->val);
        case MUL: return constant(a->val * b->val);
        case DIV: return b->val == 0 ? x : constant(a->val / b->val);
        case MOD: return b->val == 0 ? x : constant(a->val % b->val);
        case LT:  return constant(a->val < b->val);
        case GT:  return constant(a->val > b->val);
        case LE:  return constant(a->val <= b->val);
        case GE:  return constant(a->val >= b->val);
        case EQ:  return constant(a->val == b->val);
        case NE:  return constant(a->val != b->val);
        case BIT_AND: return constant(a->val & b->val);
        case BIT_OR:  return constant(a->val | b->val);
        case BIT_XOR: return constant(a->val ^ b->val);
        case BIT_SHL: return constant((int)((unsigned)a->val << (b->val & 31)));
        case BIT_SHR: return constant(a->val >> (b->val & 31));
        case LOG_AND: return constant(a->val && b->val);
        case LOG_OR:  return constant(a->val || b->val);
        default: return x;
    }
}

static node *gen(int k, node *o1, node *o2) {
    node *x=newNode(k);
    x->o1=o1; x->o2=o2;
    return foldNode(x);
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
    qFail((sym != exp), "-expected symbol[%d],not[%d]-\n", exp, sym);
    nextSym();
}

// Consumes an identifier and copies its name into name.
static void expectName(char *name) {
    qFail((sym != ID), "-expected identifier, not[%d]-\n", sym);
    strcpy(name, idName);
    nextSym();
}

static node *callArguments(const char *name) {
    int index = strcmp(name, "syscall") == 0 ? BUILTIN_SYSCALL : functionIndex(name);
    qFail(index != BUILTIN_SYSCALL && !functions[index].declared, "-function %s used before declaration-\n", name);
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
    if (index == BUILTIN_SYSCALL) {
        qFail((count < 1 || count > 7), "-syscall expects 1 to 7 arguments-");
    } else {
        struct function_s *function = &functions[index];
        qFail(count != function->numParameters, "-function %s expects %d argument(s), got %d-\n", name, function->numParameters, count);
        node *arg = args;
        for (int i = 0; i < count; i++, arg = arg->o2) {
            int isArray = arg->o1->kind == ARRAY_REF;
            qFail(isArray != function->parameterIsArray[i], "-argument %d to %s must be %s-\n", i + 1, name,
                  function->parameterIsArray[i] ? "an array reference" : "a scalar");
            qFail(isArray && arg->o1->type != function->parameterTypes[i], "-argument %d to %s must be %s array-\n", i + 1, name,
                  function->parameterTypes[i] == TYPE_BYTE ? "a byte" : "an int");
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
    int local = localIndex(name);
    if (local >= 0) { return functions[currentFunction].localTypes[local]; }
    int parameter = parameterIndex(name);
    if (parameter >= 0) {
        return functions[currentFunction].parameterTypes[parameter];
    }
    return variableTypes[findVariable(name)];
}

// The LOCAL, VAR, or ARRAY_REF node for a bare name.
static node *nameNode(const char *name) {
    int local = localIndex(name);
    int parameter = parameterIndex(name);
    int variable = findVariable(name);
    node *x;
    if (local >= 0) {
        int slot = functions[currentFunction].numParameters + local;
        int isAlias = functions[currentFunction].localIsArray[local];
        x = newNode(isAlias ? ARRAY_REF : LOCAL);
        x->val = isAlias ? -slot - 1 : slot;
        x->type = functions[currentFunction].localTypes[local];
    } else if (parameter >= 0) {
        int isArray = functions[currentFunction].parameterIsArray[parameter];
        x = newNode(isArray ? ARRAY_REF : VAR);
        x->val = isArray ? arrayIndex(name) : variableIndex(name);
        x->type = functions[currentFunction].parameterTypes[parameter];
    } else if (variable >= 0 && variableIsArray[variable]) {
        x = newNode(ARRAY_REF);
        x->val = variableBases[variable];
        x->type = variableTypes[variable];
    } else {
        x = newNode(VAR);
        x->val = variableIndex(name);
        x->type = variableTypes[variable];
    }
    return x;
}

// A prefix or postfix ++ or -- applied to a scalar variable.
static node *increment(int kind, node *target, int delta) {
    qFail(target->kind != VAR && target->kind != LOCAL, "-++ and -- need a scalar variable-");
    node *x = newNode(kind);
    x->o1 = target;
    x->val = delta;
    x->type = target->type;
    return x;
}

/* <term> ::= <id> | <id> "(" [ <expr> { "," <expr> } ] ")" | <id> "[" <expr> "]" | <int> | <char> | <string> | <paren_expr> | "-" <term> | "~" <term> */
static node *term() {
  node *x = NULL;
  if (sym == MINUS) {
      nextSym();
      return gen(SUB, constant(0), term());
  } else if (sym == TILDE) {
      nextSym();
      return gen(BIT_NOT, term(), NULL);
  } else if (sym == PLUS_PLUS || sym == MINUS_MINUS) {
      int delta = sym == PLUS_PLUS ? 1 : -1;
      nextSym();
      char name[sizeof(idName)];
      expectName(name);
      x = increment(PRE_INC, nameNode(name), delta);
  } else if (sym == ID) {
      char name[sizeof(idName)];
      strcpy(name, idName);
      nextSym();
      if (sym == LPAR) {
          x = callArguments(name);
      } else if (sym == LBRACKET) {
          int base = arrayIndex(name);
          nextSym();
          node *index = expr();
          expectSym(RBRACKET);
          x = gen(ARRAY_GET, index, NULL);
          x->val = base;
          x->type = arrayElementType(name);
      } else {
          x = nameNode(name);
          if (sym == PLUS_PLUS || sym == MINUS_MINUS) {
              x = increment(POST_INC, x, sym == PLUS_PLUS ? 1 : -1);
              nextSym();
          }
      }
  } else if (sym == INT || sym == CHAR_LITERAL) {
      x = constant(intVal);
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
    else if (sym==AMP)   { return BIT_AND; }
    else if (sym==PIPE)  { return BIT_OR; }
    else if (sym==CARET) { return BIT_XOR; }
    else if (sym==LSHIFT) { return BIT_SHL; }
    else if (sym==RSHIFT) { return BIT_SHR; }
    return 0;
}

/* <math> ::= <term> | <math> <math_op> <term> */
/* <math_op> ::= "+" | "-" | "*" | "/" | "%" | "&" | "|" | "^" | "<<" | ">>" */
static node *sum() {
  node *x = term();
  int op;
  while ((op = mathop())) {
    nextSym();
    x = gen(op, x, term());
  }
  return x;
}

/* <test> ::= <math> | <math> "<" <math> | <math> ">" <math> | <math> "<=" <math> | <math> ">=" <math> | <math> "==" <math> | <math> "!=" <math> */
static node *comparison() {
  node *x = sum();
  if (sym == LESS)     { nextSym(); return gen(LT, x, sum()); }
  if (sym == GRT)      { nextSym(); return gen(GT, x, sum()); }
  if (sym == LESS_EQ)  { nextSym(); return gen(LE, x, sum()); }
  if (sym == GRT_EQ)   { nextSym(); return gen(GE, x, sum()); }
  if (sym == EQ_SYM)   { nextSym(); return gen(EQ, x, sum()); }
  if (sym == NE_SYM)   { nextSym(); return gen(NE, x, sum()); }
  return x;
}

/* <expr> ::= <test> { ("&&" | "||") <test> }, applied left to right */
node *expr() {
  node *x = comparison();
  while (sym == AND_AND || sym == OR_OR) {
    int op = sym == AND_AND ? LOG_AND : LOG_OR;
    nextSym();
    x = gen(op, x, comparison());
  }
  return x;
}

/* <assignment> ::= <id> "=" <expr> | <id> "[" <expr> "]" "=" <expr> */
static node *assignment(node *target) {
  qFail(target->kind != VAR && target->kind != LOCAL && target->kind != ARRAY_GET, "-left side of = must be a variable or array element-");
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

static node *block(node *x);

static node *statement() {
  node *x;
  if ((sym == INT_SYM || sym == BYTE_SYM) && currentFunction >= 0) {
      int localType = keywordType(sym);
      nextSym();
      x = newNode(EMPTY);
      while (1) {
          char name[sizeof(idName)];
          expectName(name);
          qFail(parameterIndex(name) >= 0 || localIndex(name) >= 0, "-duplicate local variable %s-\n", name);
          struct function_s *function = &functions[currentFunction];
          qFail(function->numLocals >= MAX_LOCALS, "-too many local variables-");
          int local = function->numLocals;
          int isAlias = 0;
          if (sym == LBRACKET) {
              nextSym();
              expectSym(RBRACKET);
              qFail(sym != EQUAL, "-array alias %s requires an initializer-\n", name);
              isAlias = 1;
          }
          strcpy(function->localNames[local], name);
          function->localTypes[local] = localType;
          function->localIsArray[local] = isAlias;
          function->numLocals++;
          if (sym == EQUAL) {
              nextSym();
              node *value = expr();
              node *target = newNode(LOCAL);
              target->val = function->numParameters + local;
              target->type = localType;
              node *set;
              if (isAlias) {
                  qFail(value->kind != ARRAY_REF || value->type != localType, "-array alias %s must be initialized by an array of the same element type-\n", name);
                  set = gen(SET, target, value);
                  set->val = 1;  // marks an alias initializer, the only place checkArrayRefs allows an array reference to be stored
              } else {
                  if (localType == TYPE_BYTE) { value = byteConversion(value); }
                  set = gen(SET, target, value);
              }
              set->type = localType;
              x = gen(SEQ, x, 0);
              x->o2 = gen(EXPR, set, NULL);
          }
          if (sym != COMMA) { break; }
          nextSym();
      }
      expectSym(SEMI);
  } else if (sym == RETURN_SYM) {
      nextSym();
      if (sym == SEMI) {
          qFail(currentFunction < 0 || functions[currentFunction].returnsValue, "-return without a value requires a void function-");
          x = newNode(VOID_RETURN);
      } else {
          qFail(currentFunction < 0 || !functions[currentFunction].returnsValue, "-return value requires a non-void function-");
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
      nextSym();
      x = block(newNode(EMPTY));
  } else { /* <expr> ";" | <assignment> ";" */
      node *value = expr();
      if (sym == EQUAL) { value = assignment(value); }
      x = gen(EXPR, value, NULL);
      expectSym(SEMI);
  }
  return x;
}

// Parses statements up to and including the closing "}"; the opening "{" has already been consumed.
static node *block(node *x) {
    while (sym != RBRA) {
        if (sym == EOI) { syntaxError(); }
        x = gen(SEQ, x, NULL);
        x->o2 = statement();
    }
    nextSym();
    return x;
}

static void parseParameters(struct function_s *function) {
    expectSym(LPAR);
    if (sym != RPAR) {
        while (1) {
            if (sym != INT_SYM && sym != BYTE_SYM) { syntaxError(); }
            int parameterType = keywordType(sym);
            nextSym();
            if (sym != ID) { syntaxError(); }
            for (int i = 0; i < function->numParameters; i++) {
                qFail(strcmp(function->parameterNames[i], idName) == 0, "-duplicate parameter %s-\n", idName);
            }
            qFail((function->numParameters >= MAX_PARAMETERS), "-too many parameters-");
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
    node *scalar = NULL;
    int hasStringInitializer = 0;
    if (sym == EQUAL && !isArray) {
        nextSym();
        scalar = expr();
        if (type == TYPE_BYTE) { scalar = byteConversion(scalar); }
        qFail(scalar->kind != CST, "-global initializer must be constant-");
    } else if (sym == EQUAL) {
        nextSym();
        qFail(type != TYPE_BYTE || sym != STRING_LITERAL, "-string initializer requires a byte array-");
        int requiredSize = stringLength + STRING_OVERHEAD;
        qFail(!inferredSize && size < requiredSize, "-array is too small for string initializer-");
        if (inferredSize) { size = requiredSize; }
        hasStringInitializer = 1;
    } else if (inferredSize) {
        qFail(1, "-array size required unless initialized by a string-");
    }
    int zeroed = !scalar && !hasStringInitializer;
    int base = variableBases[declareVariable(name, size, isArray, type, zeroed)];
    if (scalar) {
        if (type == TYPE_BYTE) { *MEM_AT(base) = scalar->val; }
        else { storeInt(base, scalar->val); }
    }
    if (hasStringInitializer) {
        storeStringBytes(base);
        nextSym();
    }
}

static void variableDeclarations(const char *firstName, int type) {
    parseVariableDeclarator(firstName, type);
    while (sym == COMMA) {
        nextSym();
        char name[sizeof(idName)];
        expectName(name);
        parseVariableDeclarator(name, type);
    }
    expectSym(SEMI);
}

static int sameSignature(struct function_s *declared, struct function_s *header) {
    if (declared->returnType != header->returnType || declared->numParameters != header->numParameters) { return 0; }
    for (int i = 0; i < header->numParameters; i++) {
        if (declared->parameterTypes[i] != header->parameterTypes[i] ||
            declared->parameterIsArray[i] != header->parameterIsArray[i]) { return 0; }
    }
    return 1;
}

// A header that repeats an earlier declaration must match it.
static int parseHeader(const char *name, int returnType) {
    qFail((strcmp(name, "syscall") == 0), "-syscall is a built-in function-");
    int index = functionIndex(name);
    struct function_s *function = &functions[index];
    struct function_s previous = *function;
    function->returnType = returnType;
    function->returnsValue = returnType != TYPE_VOID;
    function->numParameters = 0;
    memset(function->parameterIsArray, 0, sizeof(function->parameterIsArray));
    parseParameters(function);
    qFail(previous.declared && !sameSignature(&previous, function), "-declaration of %s does not match earlier declaration-\n", name);
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
    qFail(functions[index].defined, "-duplicate function %s-\n", name);
    functions[index].defined = 1;
    if (strcmp(name, "main") == 0) {
        qFail(returnType == TYPE_BYTE, "-main must return int or void-");
        qFail(functions[index].numParameters != 0, "-main must not have parameters-");
        mainFunction = index;
    }
    expectSym(LBRA);
    int previousFunction = currentFunction;
    currentFunction = index;
    functions[index].body = block(newNode(EMPTY));
    currentFunction = previousFunction;
}

// An array reference is valid only as a direct array argument, which callArguments() has already checked,
// or as an operand of + or -, where it stands for the array's address.
static void checkArrayRefs(node *x) {
    if (!x) { return; }
    if (x->kind == SET && x->val == 1 && x->o2->kind == ARRAY_REF) { return; }
    if (x->kind == ADD || x->kind == SUB) {
        int left = x->o1->kind == ARRAY_REF, right = x->o2->kind == ARRAY_REF;
        qFail(left && right, "-cannot combine two arrays-");
        qFail(right && x->kind == SUB, "-array must be the left operand of '-'-");
        if (!left) { checkArrayRefs(x->o1); }
        if (!right) { checkArrayRefs(x->o2); }
        return;
    }
    qFail(x->kind == ARRAY_REF, "-array reference only valid as an array argument-");
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
  while (sym != EOI) {
    qFail(sym != VOID_SYM && sym != INT_SYM && sym != BYTE_SYM, "-def?-\n");
    int type = keywordType(sym);
    nextSym();
    char name[sizeof(idName)];
    expectName(name);
    if (sym == LPAR) { parseFunctionDefinition(name, type); }
    else if (type != TYPE_VOID) { variableDeclarations(name, type); }
    else { syntaxError(); }
  }
  return st;
}

/* <program> ::= <defs> */
node *program() {
  node *prog = gen(PROG, NULL, NULL);
  defs(prog);
  qFail((mainFunction < 0), "-program must define main()-");
  if (sym != EOI) { syntaxError(); }
  for (int i = 0; i < numFunctions; i++) {
      qFail((!functions[i].defined), "-undefined function %s-\n", functions[i].name);
  }
  for (int i = 0; i < numFunctions; i++) { checkArrayRefs(functions[i].body); }
  prog->o1 = functions[mainFunction].body;
  return prog;
}
