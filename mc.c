//--------------------------------------------------------------------------
// The min-c compiler: lexer, parser, code generator, listing, and main.

#include "mc.h"
#include <sys/stat.h>

// -------------------------------------------------
// Lexer for min-c
// -------------------------------------------------
char *reservedWords[] = { "do", "else", "if", "while", "void", "int", "char", "return", "const", NULL };

FILE *inputFp = NULL;
int ch = ' ', sym, intVal;
char idName[ID_NAME_LENGTH];
char currentPath[MAX_PATH_LENGTH] = "";
int lineNumber = 1;
struct source_s { FILE *fp; char path[MAX_PATH_LENGTH]; int line; int savedCh; };
struct source_s sourceStack[MAX_INCLUDE_DEPTH];
int sourceDepth = 0;
char includedPaths[MAX_INCLUDED_FILES][MAX_PATH_LENGTH];
int numIncludedPaths = 0;
int stringValues[MAX_STRING_LENGTH], stringLength;

void qFail(int condition, const char *fmt, ...) {
    if (condition) {
        va_list args;
        printf("%s:%d: ", currentPath, lineNumber);
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        exit(1);
    }
}

void syntaxError() { qFail(1, "-syntax error-"); }

int isAlpha(int ch) { return BTWI(ch,'A','Z') || BTWI(ch,'a','z') || (ch=='_'); }
int isNum(int ch) { return BTWI(ch,'0','9'); }
int isAlphaNum(int ch) { return isAlpha(ch) || isNum(ch); }

int nextCh() {
    if (inputFp) { ch = fgetc(inputFp); }
    else { ch = getchar(); }
    if (ch == '\n') { lineNumber++; }
    return ch;
}

// Reads one character inside a character or string literal, decoding a backslash escape.
int literalCharacter() {
    int value = ch;
    if (ch == '\\') {
        switch (nextCh()) {
            case 'n': value = '\n'; break;
            case 'r': value = '\r'; break;
            case 't': value = '\t'; break;
            case '0': value = '\0'; break;
            case 'e': value =  27;  break;
            default:  value =  ch;  break;
        }
    }
    nextCh();
    return value;
}

// Called after the first character of a one- or two-character symbol has been read.
int symbolPair(int second, int twoCharSym, int oneCharSym) {
    if (ch != second) { return oneCharSym; }
    nextCh();
    return twoCharSym;
}

// Reads an identifier or directive name into idName.
void readName(const char *tooLongMessage) {
    int i = 0;
    while (isAlphaNum(ch)) {
        qFail((i >= (int)sizeof(idName) - 1), tooLongMessage);
        idName[i++] = ch;
        nextCh();
    }
    idName[i] = '\0';
}

int openSource(const char *path) {
    qFail((strlen(path) >= MAX_PATH_LENGTH), "-path too long-");
    inputFp = fopen(path, "rt");
    qFail((!inputFp), "-cannot open %s-\n", path);
    strcpy(currentPath, path);
    strcpy(includedPaths[numIncludedPaths++], path);
    return 1;
}

void includeFile() {
    char path[MAX_PATH_LENGTH];
    int length = 0;
    FILE *fp;
    while (ch == ' ' || ch == 9) { nextCh(); }
    qFail(ch != '"', "-include expects a quoted file name-");
    nextCh();
    while (ch != '"') {
        qFail(ch == EOF || ch == '\n' || ch == '\r', "-unterminated include file name-");
        qFail(length >= MAX_PATH_LENGTH - 1, "-include file name too long-");
        path[length++] = ch;
        nextCh();
    }
    path[length] = '\0';
    nextCh();
    for (int i = 0; i < numIncludedPaths; i++) {
        if (strcmp(includedPaths[i], path) == 0) { return; }
    }
    qFail((numIncludedPaths >= MAX_INCLUDED_FILES), "-too many included files-");
    qFail((sourceDepth >= MAX_INCLUDE_DEPTH), "-includes nested too deeply-");
    fp = fopen(path, "rt");
    qFail((!fp), "-cannot open include file %s-\n", path);
    strcpy(includedPaths[numIncludedPaths++], path);
    struct source_s *outer = &sourceStack[sourceDepth++];
    outer->fp = inputFp;
    strcpy(outer->path, currentPath);
    outer->line = lineNumber;
    outer->savedCh = ch;
    inputFp = fp;
    strcpy(currentPath, path);
    lineNumber = 1;
    nextCh();
}

void closeSource() {
    fclose(inputFp);
    sourceDepth--;
    struct source_s *outer = &sourceStack[sourceDepth];
    inputFp = outer->fp;
    strcpy(currentPath, outer->path);
    lineNumber = outer->line;
    ch = outer->savedCh;
}

void nextSym() {
  again:
    switch (ch) {
      case ' ': case 9: case 10: case 13: nextCh(); goto again;
      case EOF:
        if (sourceDepth > 0) { closeSource(); goto again; }
        sym = EOI; break;
      case '{': nextCh(); sym = LBRA;  break;
      case '}': nextCh(); sym = RBRA;  break;
      case '(': nextCh(); sym = LPAR;  break;
      case ')': nextCh(); sym = RPAR;  break;
      case '+': nextCh(); sym = symbolPair('+', PLUS_PLUS, PLUS); break;
      case '-': nextCh(); sym = symbolPair('-', MINUS_MINUS, MINUS); break;
      case '*': nextCh(); sym = STAR;  break;
      case '/':
        nextCh();
        if (ch == '/') {
            while (ch != EOF && ch != '\n' && ch != '\r') { nextCh(); }
            goto again;
        }
        sym = SLASH;
        break;
      case '%': nextCh(); sym = MODULO; break;
      case '#':
          nextCh();
          readName("-directive too long-");
          qFail(strcmp(idName, "include") != 0, "-unknown directive-");
          includeFile();
          goto again;
      case '<': nextCh(); if (ch == '<') { nextCh(); sym = LSHIFT; } else { sym = symbolPair('=', LESS_EQ, LESS); } break;
      case '>': nextCh(); if (ch == '>') { nextCh(); sym = RSHIFT; } else { sym = symbolPair('=', GRT_EQ, GRT); } break;
      case '&': nextCh(); sym = symbolPair('&', AND_AND, AMP); break;
      case '|': nextCh(); sym = symbolPair('|', OR_OR, PIPE); break;
      case '^': nextCh(); sym = CARET; break;
      case '~': nextCh(); sym = TILDE; break;
      case '!': nextCh(); if (ch != '=') { syntaxError(); } nextCh(); sym = NE_SYM; break;
      case ';': nextCh(); sym = SEMI;  break;
      case '=': nextCh(); sym = symbolPair('=', EQ_SYM, EQUAL); break;
      case '[': nextCh(); sym = LBRACKET; break;
      case ']': nextCh(); sym = RBRACKET; break;
      case ',': nextCh(); sym = COMMA; break;
      case '\'':
        nextCh();
        qFail((ch == EOF || ch == '\n' || ch == '\r' || ch == '\''), "-invalid character literal-");
        intVal = literalCharacter();
        qFail((ch != '\''), "-character literal must contain one character-");
        nextCh();
        sym = CHAR_LITERAL;
        break;
      case '"':
        stringLength = 0;
        nextCh();
        while (ch != '"') {
            qFail((ch == EOF || ch == '\n' || ch == '\r'), "-unterminated string literal-");
            int value = literalCharacter();
            qFail((value > 127), "-string literals must contain ASCII characters-");
            qFail((stringLength >= MAX_STRING_LENGTH), "-string literal too long-");
            stringValues[stringLength++] = value;
        }
        nextCh();
        sym = STRING_LITERAL;
        break;
      default:
        if (isNum(ch)) {
            intVal = 0; /* missing overflow check */
            while (isNum(ch)) { intVal = intVal*10 + (ch - '0'); nextCh(); }
            sym = INT;
        } else if (isAlpha(ch)) {
            readName("-identifier too long-");
            sym = 0;
            while (reservedWords[sym] != NULL && strcmp(reservedWords[sym], idName) != 0) { sym++; }
            if (reservedWords[sym] == NULL) { sym = ID; }
            if (sym == VOID_SYM) { qFail(1, "-void is not supported-"); }
        } else { qFail(1, "-ch '%c' expected to be alpha-\n", ch); }
        break;
    }
}

// -------------------------------------------------
// Parser for min-c
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
char constNames[MAX_GLOBAL_NAMES][ID_NAME_LENGTH];
int constTypes[MAX_GLOBAL_NAMES];
int constValues[MAX_GLOBAL_NAMES];
int numConstants;
int numVariables;
int numGlobalBytes = ELF_HEADER_BYTES;
int numZeroedBytes;

int findVariable(const char *name) {
    for (int i = 0; i < numVariables; i++) {
        if (strcmp(variableNames[i], name) == 0) { return i; }
    }
    return -1;
}

// Scalar constants are substituted by the parser, so they need no storage in the program.
int findConstant(const char *name) {
    for (int i = 0; i < numConstants; i++) {
        if (strcmp(constNames[i], name) == 0) { return i; }
    }
    return -1;
}

void declareConstant(const char *name, int type, int value) {
    qFail(findVariable(name) >= 0 || findConstant(name) >= 0, "-duplicate variable %s-\n", name);
    qFail(numConstants >= MAX_GLOBAL_NAMES, "-too many constants-");
    strcpy(constNames[numConstants], name);
    constTypes[numConstants] = type;
    constValues[numConstants] = value;
    numConstants++;
}

// Initialized globals grow up from LOAD_BASE + ELF_HEADER_BYTES. Zero-filled globals grow down from the top of the global area,
// so their addresses stay fixed as more are declared, and they are not stored in the executable.
int declareVariable(const char *name, int size, int isArray, int type, int zeroed) {
    int index = findVariable(name);
    qFail(index >= 0 || findConstant(name) >= 0, "-duplicate variable %s-\n", name);
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
    for (int i = 0; i < stringLength; i++) {
        *MEM_AT(base + i) = stringValues[i];
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
    qFail(findConstant(name) >= 0, "-constant %s is not an array-\n", name);
    qFail((index < 0), "-undeclared variable %s-\n", name);
    qFail(!variableIsArray[index], "-variable %s is not an array-\n", name);
    return variableBases[index];
}

int functionIndex(const char *name) {
    for (int i = 0; i < numFunctions; i++) {
        if (strcmp(functions[i].name, name) == 0) { return i; }
    }
    qFail((numFunctions >= MAX_FUNCTIONS), "-too many functions-");
    struct function_s *function = &functions[numFunctions];
    memset(function, 0, sizeof(*function));
    strcpy(function->name, name);
    function->returnType = TYPE_INT;
    return numFunctions++;
}

node *newNode(int k) {
    qFail((MAX_NODES <= numNodes), "-too many syntax nodes-");
    node *x = &nodes[numNodes++];
    memset(x, 0, sizeof(*x));
    x->kind = k;
    return x;
}

node *constant(int value) {
    node *x = newNode(CST);
    x->val = value;
    return x;
}

// Replaces an operator node whose operands are all constants with the constant result.
node *foldNode(node *x) {
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

node *gen(int k, node *o1, node *o2) {
    node *x=newNode(k);
    x->o1=o1; x->o2=o2;
    return foldNode(x);
}

node *byteConversion(node *value) {
    node *conversion = gen(BYTE_CONVERT, value, NULL);
    conversion->type = TYPE_BYTE;
    return conversion;
}

// The type named by an int or char keyword.
int keywordType(int keyword) {
    return keyword == CHAR_SYM ? TYPE_BYTE : TYPE_INT;
}

void expectSym(int exp) {
    qFail((sym != exp), "-expected symbol[%d],not[%d]-\n", exp, sym);
    nextSym();
}

// Consumes an identifier and copies its name into name.
void expectName(char *name) {
    qFail((sym != ID), "-expected identifier, not[%d]-\n", sym);
    strcpy(name, idName);
    nextSym();
}

node *callArguments(const char *name) {
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
                  function->parameterTypes[i] == TYPE_BYTE ? "a char" : "an int");
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

int arrayElementType(const char *name) {
    int local = localIndex(name);
    if (local >= 0) { return functions[currentFunction].localTypes[local]; }
    int parameter = parameterIndex(name);
    if (parameter >= 0) {
        return functions[currentFunction].parameterTypes[parameter];
    }
    return variableTypes[findVariable(name)];
}

// The LOCAL, VAR, or ARRAY_REF node for a bare name.
node *nameNode(const char *name) {
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
    } else if (findConstant(name) >= 0) {
        x = constant(constValues[findConstant(name)]);
        x->type = constTypes[findConstant(name)];
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
node *increment(int kind, node *target, int delta) {
    qFail(target->kind != VAR && target->kind != LOCAL, "-++ and -- need a scalar variable-");
    node *x = newNode(kind);
    x->o1 = target;
    x->val = delta;
    x->type = target->type;
    return x;
}

/* <term> ::= <id> | <id> "(" [ <expr> { "," <expr> } ] ")" | <id> "[" <expr> "]" | <int> | <char> | <string> | <paren_expr> | "-" <term> | "~" <term> */
node *term() {
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

int mathop() {
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
node *sum() {
  node *x = term();
  int op;
  while ((op = mathop())) {
    nextSym();
    x = gen(op, x, term());
  }
  return x;
}

/* <test> ::= <math> | <math> "<" <math> | <math> ">" <math> | <math> "<=" <math> | <math> ">=" <math> | <math> "==" <math> | <math> "!=" <math> */
node *comparison() {
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
node *assignment(node *target) {
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

node *block(node *x);

node *statement() {
  node *x;
  if ((sym == INT_SYM || sym == CHAR_SYM) && currentFunction >= 0) {
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
          qFail(1, "-return without a value-");
      } else {
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
node *block(node *x) {
    while (sym != RBRA) {
        if (sym == EOI) { syntaxError(); }
        x = gen(SEQ, x, NULL);
        x->o2 = statement();
    }
    nextSym();
    return x;
}

void parseParameters(struct function_s *function) {
    expectSym(LPAR);
    if (sym != RPAR) {
        while (1) {
            if (sym != INT_SYM && sym != CHAR_SYM) { syntaxError(); }
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

void parseVariableDeclarator(const char *name, int type) {
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
        qFail(type != TYPE_BYTE || sym != STRING_LITERAL, "-string initializer requires a char array-");
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

void variableDeclarations(const char *firstName, int type) {
    parseVariableDeclarator(firstName, type);
    while (sym == COMMA) {
        nextSym();
        char name[sizeof(idName)];
        expectName(name);
        parseVariableDeclarator(name, type);
    }
    expectSym(SEMI);
}

// <const_def> ::= "const" ("int" | "char") <id> "=" <expr> ";". The initializer must fold to a constant.
void constDefinition(void) {
    nextSym();
    qFail(sym != INT_SYM && sym != CHAR_SYM, "-const must be int or char-");
    int type = keywordType(sym);
    nextSym();
    char name[sizeof(idName)];
    expectName(name);
    expectSym(EQUAL);
    node *value = expr();
    if (type == TYPE_BYTE) { value = byteConversion(value); }
    qFail(value->kind != CST, "-const initializer must be constant-");
    expectSym(SEMI);
    declareConstant(name, type, value->val);
}

int sameSignature(struct function_s *declared, struct function_s *header) {
    if (declared->returnType != header->returnType || declared->numParameters != header->numParameters) { return 0; }
    for (int i = 0; i < header->numParameters; i++) {
        if (declared->parameterTypes[i] != header->parameterTypes[i] ||
            declared->parameterIsArray[i] != header->parameterIsArray[i]) { return 0; }
    }
    return 1;
}

// A header that repeats an earlier declaration must match it.
int parseHeader(const char *name, int returnType) {
    qFail((strcmp(name, "syscall") == 0), "-syscall is a built-in function-");
    int index = functionIndex(name);
    struct function_s *function = &functions[index];
    struct function_s previous = *function;
    function->returnType = returnType;
    function->numParameters = 0;
    memset(function->parameterIsArray, 0, sizeof(function->parameterIsArray));
    parseParameters(function);
    qFail(previous.declared && !sameSignature(&previous, function), "-declaration of %s does not match earlier declaration-\n", name);
    function->declared = 1;
    return index;
}

// Handles a prototype (ending in ';') or a function definition.
void parseFunctionDefinition(const char *name, int returnType) {
    int index = parseHeader(name, returnType);
    if (sym == SEMI) {
        nextSym();
        return;
    }
    qFail(functions[index].defined, "-duplicate function %s-\n", name);
    functions[index].defined = 1;
    if (strcmp(name, "main") == 0) {
        qFail(returnType != TYPE_INT, "-main must return int-");
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
void checkArrayRefs(node *x) {
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
node *defs(node *st) {
  nextSym();
  while (sym != EOI) {
    if (sym == CONST_SYM) { constDefinition(); continue; }
    qFail(sym != INT_SYM && sym != CHAR_SYM, "-def?-\n");
    int type = keywordType(sym);
    nextSym();
    char name[sizeof(idName)];
    expectName(name);
    if (sym == LPAR) { parseFunctionDefinition(name, type); }
    else { variableDeclarations(name, type); }
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

// -------------------------------------------------
// Code generator for min-c
// -------------------------------------------------
// Code generation. Expressions leave their value in eax. A binary operation evaluates its left operand and
// pushes it, evaluates the right operand into eax, moves that to ecx, pops the left operand into eax, and then
// applies the operator. A statement leaves the stack as it found it.
//
// Calling convention: arguments are evaluated left to right and pushed, then the call pushes the return address.
// The callee pushes ebp, sets ebp to esp, and pushes a zero for each local. Parameter i of n is at
// [ebp + 8 + 4*(n-1-i)] and local j is at [ebp - 4*(j+1)]. The caller removes the arguments after the call.
// The result is returned in eax.

struct call_fixup_s callFixups[MAX_CALL_FIXUPS];
int here;
int numCallFixups;
struct function_s *curFunction;

void checkSize(int sz) { qFail(here + sz > CODE_TOP, "-program too large-"); }
void g(int b) { checkSize(1); *MEM_AT(here++) = (byte)b; }
void g4(int v) { checkSize(4); memcpy(MEM_AT(here), &v, 4); here += 4; }
void movImm(int reg, int v) { g(X_MOV_R_IMM + reg); g4(v); }

// Reserves a 4-byte field for a rel32 displacement. fix() or patchCalls() fills it in.
int hole() {
    checkSize(4);
    int p = here;
    here += 4;
    return p;
}

// The rel32 displacement is measured from the end of the field, which is also the end of the instruction.
void fix(int field, int dst) {
    int rel = dst - (field + 4);
    memcpy(MEM_AT(field), &rel, 4);
}

// rr() emits an opcode with a ModRM byte for two registers. reg is a register or an opcode extension (/digit).
// ri() adds an imm32 after such an operand.
void rr(int opcode, int reg, int rm) { g(opcode); g(0xC0 | (reg << 3) | rm); }
void ri(int opcode, int ext, int rm, int imm) { rr(opcode, ext, rm); g4(imm); }
void movzxAl() { g(X_ESCAPE); rr(X_MOVZX_R_RM8, EAX, EAX); }
void int80() { g(X_INT); g(0x80); }

// Memory operands: a frame slot is [ebp+disp32], a fixed address is [disp32], and [base] is a register.
int slotDisp(int slot) {
    int numParams = curFunction->numParameters;
    return slot < numParams ? 8 + 4 * (numParams - 1 - slot) : -4 * (slot - numParams + 1);
}
void modrmFrame(int reg, int slot) { g(0x80 | (reg << 3) | EBP); g4(slotDisp(slot)); }
void modrmAbs(int reg, int addr) { g((reg << 3) | 5); g4(addr); }  // mod=00, rm=101 means disp32
void modrmIndirect(int reg, int base) { g((reg << 3) | base); }

// Emits a call to a user function. patchCalls() fills in the target address.
void emitCall(int index) {
    qFail(numCallFixups >= MAX_CALL_FIXUPS, "-too many function calls-");
    g(X_CALL);
    callFixups[numCallFixups].operand = hole();
    callFixups[numCallFixups].functionIndex = index;
    numCallFixups++;
}

void patchCalls() {
    for (int i = 0; i < numCallFixups; i++) {
        struct call_fixup_s *f = &callFixups[i];
        fix(f->operand, functions[f->functionIndex].address);
    }
}


// Writes a little-endian value of the given size in bytes.
void putLE(FILE *fp, unsigned value, int size) {
    for (int i = 0; i < size; i++) { fputc((value >> (8*i)) & 0xff, fp); }
}

// Writes a 32-bit Linux executable with two PT_LOAD segments. The data segment starts at file offset 0 and LOAD_BASE,
// so the ELF headers occupy the first ELF_HEADER_BYTES of it, followed by the globals. The code segment starts at the
// next page boundary, and execution starts at CODE_BASE.
int saveExecutable(const char *fileName) {
    const int pageSize = 0x1000, headerSize = 52, programHeaderSize = 32;
    unsigned dataSize = (unsigned)numGlobalBytes;
    unsigned codeSize = (unsigned)(here - CODE_BASE);
    unsigned codeOffset = (dataSize + pageSize - 1) & ~(unsigned)(pageSize - 1);
    FILE *fp = fopen(fileName, "wb");
    qFail(!fp, "-cannot write executable %s-\n", fileName);
    fwrite("\x7f" "ELF", 1, 4, fp);
    putLE(fp, 1, 1);                    // 32-bit
    putLE(fp, 1, 1);                    // little-endian
    putLE(fp, 1, 1);                    // ELF version
    putLE(fp, 0, 9);                    // OS/ABI, ABI version, padding
    putLE(fp, 2, 2);                    // e_type: ET_EXEC
    putLE(fp, 3, 2);                    // e_machine: EM_386
    putLE(fp, 1, 4);                    // e_version
    putLE(fp, CODE_BASE, 4);            // e_entry
    putLE(fp, headerSize, 4);           // e_phoff
    putLE(fp, 0, 4);                    // e_shoff
    putLE(fp, 0, 4);                    // e_flags
    putLE(fp, headerSize, 2);           // e_ehsize
    putLE(fp, programHeaderSize, 2);    // e_phentsize
    putLE(fp, 2, 2);                    // e_phnum
    putLE(fp, 0, 2);                    // e_shentsize
    putLE(fp, 0, 2);                    // e_shnum
    putLE(fp, 0, 2);                    // e_shstrndx
    // Data segment: globals, read/write. The zeroed part of the global area is not stored in the file.
    putLE(fp, 1, 4);                    // p_type: PT_LOAD
    putLE(fp, 0, 4);                    // p_offset
    putLE(fp, LOAD_BASE, 4);            // p_vaddr
    putLE(fp, LOAD_BASE, 4);            // p_paddr
    putLE(fp, dataSize, 4);             // p_filesz
    putLE(fp, MAX_GLOBAL_BYTES, 4);     // p_memsz
    putLE(fp, 6, 4);                    // p_flags: read, write
    putLE(fp, pageSize, 4);             // p_align
    // Text segment: code, read/execute, starting at CODE_BASE.
    putLE(fp, 1, 4);                    // p_type: PT_LOAD
    putLE(fp, codeOffset, 4);           // p_offset
    putLE(fp, CODE_BASE, 4);            // p_vaddr
    putLE(fp, CODE_BASE, 4);            // p_paddr
    putLE(fp, codeSize, 4);             // p_filesz
    putLE(fp, codeSize, 4);             // p_memsz
    putLE(fp, 5, 4);                    // p_flags: read, execute
    putLE(fp, pageSize, 4);             // p_align
    for (int i = headerSize + 2 * programHeaderSize; i < ELF_HEADER_BYTES; i++) { fputc(0, fp); }
    fwrite(MEM_AT(LOAD_BASE + ELF_HEADER_BYTES), 1, dataSize - ELF_HEADER_BYTES, fp);
    for (unsigned i = dataSize; i < codeOffset; i++) { fputc(0, fp); }
    fwrite(MEM_AT(CODE_BASE), 1, codeSize, fp);
    fclose(fp);
    chmod(fileName, 0755);
    return (int)(codeOffset + codeSize);
}
    
// A parameter or local is a frame slot. Parameters are slots 0..numParameters-1 and the locals follow them.
int isSlot(node *x) { return x->kind == LOCAL || x->val < 0; }
int slotOf(node *x) { return x->kind == LOCAL ? x->val : -x->val - 1; }

// Loads variable t into eax.
void loadVar(node *t) {
    if (isSlot(t)) { g(X_MOV_R_RM); modrmFrame(EAX, slotOf(t)); }
    else if (t->type == TYPE_BYTE) { g(X_ESCAPE); g(X_MOVZX_R_RM8); modrmAbs(EAX, t->val); }
    else { g(X_MOV_R_RM); modrmAbs(EAX, t->val); }
}

// Stores eax into variable t.
void storeVar(node *t) {
    if (isSlot(t)) { g(X_MOV_RM_R); modrmFrame(EAX, slotOf(t)); }
    else if (t->type == TYPE_BYTE) { g(X_MOV_RM8_R8); modrmAbs(EAX, t->val); }
    else { g(X_MOV_RM_R); modrmAbs(EAX, t->val); }
}

// Loads the base address of array x into eax: a global's address, or the address held by a parameter.
void loadBase(node *x) {
    if (x->val < 0) { g(X_MOV_R_RM); modrmFrame(EAX, -x->val - 1); }
    else { movImm(EAX, x->val); }
}

void cExpr(node *x);
void cEffect(node *x);

// Leaves the address of element x->o1 of array x in eax.
void cElementAddress(node *x) {
    cExpr(x->o1);
    if (x->type != TYPE_BYTE) { rr(X_GRP2_RM_IMM8, 4, EAX); g(2); }
    if (x->val >= 0) { ri(X_GRP1_RM_IMM32, 0, EAX, x->val); }
    else {
        rr(X_MOV_RM_R, EAX, ECX);
        loadBase(x);
        rr(X_ADD_RM_R, ECX, EAX);
    }
}

// Calls a user function. The arguments are pushed in order, and the caller removes them after the call.
void cCall(node *x) {
    int n = 0;
    for (node *a = x->o1; a; a = a->o2) {
        cExpr(a->o1);
        g(X_PUSH + EAX);
        n++;
    }
    emitCall(x->val);
    if (n > 0) { ri(X_GRP1_RM_IMM32, 0, ESP, 4 * n); }
}

// The system call of syscall(n, a1, ..., a6). ebp is the frame pointer, and it carries a6, so it is saved first
// and restored after the call. Each argument is pushed, unused ones are zero, and the values are popped into
// eax (n), ebx, ecx, edx, esi, edi, and ebp. The result is left in eax.
void cSyscall(node *args) {
    g(X_PUSH + EBP);
    int n = 0;
    for (node *a = args; a; a = a->o2, n++) {
        cExpr(a->o1);
        g(X_PUSH + EAX);
    }
    for (; n < 7; n++) {
        movImm(EAX, 0);
        g(X_PUSH + EAX);
    }
    g(X_POP + EBP); g(X_POP + EDI); g(X_POP + ESI); g(X_POP + EDX);
    g(X_POP + ECX); g(X_POP + EBX); g(X_POP + EAX);
    int80();
    g(X_POP + EBP);
}

// Evaluates the operands of a binary operation: the left operand ends up in eax and the right one in ecx.
void cOperands(node *x) {
    cExpr(x->o1);
    g(X_PUSH + EAX);
    cExpr(x->o2);
    rr(X_MOV_RM_R, EAX, ECX);
    g(X_POP + EAX);
}

// Compares eax with ecx and leaves 0 or 1 in eax.
void cCompare(int cc) {
    rr(X_CMP_RM_R, ECX, EAX);
    g(X_ESCAPE); rr(X_SETCC + cc, 0, EAX);
    movzxAl();
}

// && and || leave 0 or 1 in eax. The right operand is skipped when the left one decides the result.
// A skipped right side leaves the left value in eax, which the final test still normalizes to 0 or 1.
void cLogical(node *x) {
    cExpr(x->o1);
    rr(X_TEST_RM_R, EAX, EAX);
    g(X_ESCAPE); g(X_JCC + (x->kind == LOG_AND ? CC_E : CC_NE));
    int skip = hole();
    cExpr(x->o2);
    fix(skip, here);
    rr(X_TEST_RM_R, EAX, EAX);
    g(X_ESCAPE); rr(X_SETCC + CC_NE, 0, EAX);
    movzxAl();
}

// Adds x->val to the variable of x. With keep, the expression's value is left on the stack:
// the old value for a post-increment, the new value for a pre-increment.
void cIncrement(node *x, int keep) {
    node *t = x->o1;
    int isPost = x->kind == POST_INC;
    loadVar(t);
    if (keep && isPost) { g(X_PUSH + EAX); }
    ri(X_GRP1_RM_IMM32, 0, EAX, x->val);
    if (x->type == TYPE_BYTE) { movzxAl(); }
    storeVar(t);
    if (keep && isPost) { g(X_POP + EAX); }
}

// Loads the element at eax (as found by cElementAddress) into eax.
void loadElement(int type) {
    if (type == TYPE_BYTE) { g(X_ESCAPE); g(X_MOVZX_R_RM8); modrmIndirect(EAX, EAX); }
    else { g(X_MOV_R_RM); modrmIndirect(EAX, EAX); }
}

// Evaluates expression x into eax. Every expression leaves its value in eax.
void cExpr(node *x) {
    switch (x->kind) {
        case CST: movImm(EAX, x->val); break;
        case VAR: case LOCAL: loadVar(x); break;
        case ARRAY_REF: loadBase(x); break;
        case ARRAY_GET: cElementAddress(x); loadElement(x->type); break;
        case BYTE_CONVERT: cExpr(x->o1); movzxAl(); break;
        case ADD: cOperands(x); rr(X_ADD_RM_R, ECX, EAX); break;
        case SUB: cOperands(x); rr(X_SUB_RM_R, ECX, EAX); break;
        case MUL: cOperands(x); g(X_ESCAPE); rr(X_IMUL_R_RM, EAX, ECX); break;
        case DIV: case MOD:
            cOperands(x);
            g(X_CDQ);
            rr(X_GRP3_RM, 7, ECX);  // idiv ecx: the quotient is in eax and the remainder in edx
            if (x->kind == MOD) { rr(X_MOV_RM_R, EDX, EAX); }
            break;
        case LT:  cOperands(x); cCompare(CC_L); break;
        case GT:  cOperands(x); cCompare(CC_G); break;
        case LE:  cOperands(x); cCompare(CC_LE); break;
        case GE:  cOperands(x); cCompare(CC_GE); break;
        case EQ:  cOperands(x); cCompare(CC_E); break;
        case NE:  cOperands(x); cCompare(CC_NE); break;
        case BIT_AND: cOperands(x); rr(X_AND_RM_R, ECX, EAX); break;
        case BIT_OR:  cOperands(x); rr(X_OR_RM_R, ECX, EAX); break;
        case BIT_XOR: cOperands(x); rr(X_XOR_RM_R, ECX, EAX); break;
        case BIT_NOT: cExpr(x->o1); rr(X_GRP3_RM, 2, EAX); break;  // not eax
        case BIT_SHL: cOperands(x); rr(X_GRP2_RM_CL, 4, EAX); break;  // shl eax, cl
        case BIT_SHR: cOperands(x); rr(X_GRP2_RM_CL, 7, EAX); break;  // sar eax, cl
        case LOG_AND: case LOG_OR: cLogical(x); break;
        case PRE_INC: case POST_INC: cIncrement(x, 1); break;
        case FUNC_CALL:
            if (x->val == BUILTIN_SYSCALL) { cSyscall(x->o1); break; }
            cCall(x);
            break;
        default: qFail(1, "-internal: bad expression kind %d-", x->kind);
    }
}

// Stores into element x->o1 of array x (an ARRAY_SET node). The address is computed first, then the value.
void cElementStore(node *x) {
    cElementAddress(x);
    g(X_PUSH + EAX);
    cExpr(x->o2);
    g(X_POP + ECX);
    if (x->type == TYPE_BYTE) { g(X_MOV_RM8_R8); } else { g(X_MOV_RM_R); }
    modrmIndirect(EAX, ECX);
}

// Evaluates x for its side effects. A value that x leaves in eax is simply overwritten by the next expression.
void cEffect(node *x) {
    switch (x->kind) {
        case SET: cExpr(x->o2); storeVar(x->o1); break;
        case PRE_INC: case POST_INC: cIncrement(x, 0); break;
        case ARRAY_SET: cElementStore(x); break;
        case FUNC_CALL:
            if (x->val == BUILTIN_SYSCALL) { cExpr(x); } else { cCall(x); }
            break;
        default: cExpr(x); break;
    }
}

// Emits a jump taken when x is true (whenTrue) or false. Returns the hole for fix().
int condJump(node *x, int whenTrue) {
    cExpr(x);
    rr(X_TEST_RM_R, EAX, EAX);
    g(X_ESCAPE); g(X_JCC + (whenTrue ? CC_NE : CC_E));
    return hole();
}

// Tears down the frame and returns to the caller. The caller removes the arguments.
void epilogue() { g(X_LEAVE); g(X_RET); }

// Emits statement x. A statement leaves the stack as it found it.
void cStmt(node *x) {
    int p1, p2, top;
    if (!x) { return; }
    switch (x->kind) {
        case SEQ: cStmt(x->o1); cStmt(x->o2); break;
        case EMPTY: break;
        case EXPR: cEffect(x->o1); break;
        case IF1:
            p1 = condJump(x->o1, 0);
            cStmt(x->o2);
            fix(p1, here);
            break;
        case IF2:
            p1 = condJump(x->o1, 0);
            cStmt(x->o2);
            g(X_JMP); p2 = hole();
            fix(p1, here);
            cStmt(x->o3);
            fix(p2, here);
            break;
        case WHILE:
            top = here;
            p1 = condJump(x->o1, 0);
            cStmt(x->o2);
            g(X_JMP); fix(hole(), top);
            fix(p1, here);
            break;
        case DO:
            top = here;
            cStmt(x->o1);
            p1 = condJump(x->o2, 1);
            fix(p1, top);
            break;
        case RETURN_NODE: cExpr(x->o1); epilogue(); break;
        default: cEffect(x); break;
    }
}

// Generates the program. The code starts with a call to main that passes its return value to the exit system
// call, then each function. A function sets up its frame, runs its body, and returns 0 if it falls off the end.
void c(node *prog) {
    qFail(prog->kind != PROG, "-program expected-");
    here = CODE_BASE;
    emitCall(mainFunction);
    rr(X_MOV_RM_R, EAX, EBX);  // the exit status
    movImm(EAX, 1);            // the exit system call
    int80();
    for (int i = 0; i < numFunctions; i++) {
        struct function_s *f = &functions[i];
        curFunction = f;
        f->address = here;
        g(X_PUSH + EBP);
        rr(X_MOV_RM_R, ESP, EBP);
        movImm(EAX, 0);
        for (int j = 0; j < f->numLocals; j++) { g(X_PUSH + EAX); }
        cStmt(f->body);
        movImm(EAX, 0);
        epilogue();
    }
    patchCalls();
}

// -------------------------------------------------
// Listing of the generated code. Writes a disassembly of the code to listing.txt when mc is run with -l.
// -------------------------------------------------

const char *labelAt(int address) {
    if (address == CODE_BASE) { return "start"; }
    for (int i = 0; i < numFunctions; i++) {
        if (functions[i].defined && functions[i].address == address) { return functions[i].name; }
    }
    return NULL;
}

const char *reg32Names[8] = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi" };
const char *reg8Names[8] = { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh" };
const char *ccNames[16] = { "o", "no", "b", "ae", "e", "ne", "be", "a", "s", "ns", "p", "np", "l", "ge", "le", "g" };

int readInt(int addr) {
    int value;
    memcpy(&value, MEM_AT(addr), 4);
    return value;
}

// Reads the ModRM byte at addr and any displacement after it. Sets reg and writes the r/m operand to out.
// byteRegs selects 8-bit register names. memPrefix (such as "dword ") goes before a memory operand.
// Returns the number of bytes used.
int disassembleModRM(int addr, int *reg, char *out, int byteRegs, const char *memPrefix) {
    int b = *MEM_AT(addr);
    int mod = b >> 6, rm = b & 7;
    *reg = (b >> 3) & 7;
    if (mod == 3) {
        strcpy(out, byteRegs ? reg8Names[rm] : reg32Names[rm]);
        return 1;
    }
    qFail(rm == ESP, "-unsupported x86 addressing: SIB-");
    qFail(mod == 1, "-unsupported x86 addressing: disp8-");
    if (mod == 0 && rm == EBP) {
        sprintf(out, "%s[0x%X]", memPrefix, (unsigned)readInt(addr + 1));
        return 5;
    }
    if (mod == 0) {
        sprintf(out, "%s[%s]", memPrefix, reg32Names[rm]);
        return 1;
    }
    int disp = readInt(addr + 1);
    sprintf(out, "%s[%s%c%d]", memPrefix, reg32Names[rm], disp < 0 ? '-' : '+', disp < 0 ? -disp : disp);
    return 5;
}

// A jump or call target is shown as the function name if one starts there, otherwise as an address.
void targetText(char *out, int target) {
    const char *label = labelAt(target);
    if (label) { strcpy(out, label); } else { sprintf(out, "%08X", (unsigned)target); }
}

// Writes the two-byte instruction (0F xx) at addr and returns its length.
int disassembleEscape(int addr, char *text) {
    int op2 = *MEM_AT(addr + 1);
    int reg, n, rel;
    char rm[64], target[32];
    if (op2 >= X_JCC && op2 < X_JCC + 16) {
        rel = readInt(addr + 2);
        targetText(target, addr + 6 + rel);
        sprintf(text, "j%s %s", ccNames[op2 - X_JCC], target);
        return 6;
    }
    if (op2 >= X_SETCC && op2 < X_SETCC + 16) {
        n = disassembleModRM(addr + 2, &reg, rm, 1, "byte ");
        sprintf(text, "set%s %s", ccNames[op2 - X_SETCC], rm);
        return 2 + n;
    }
    if (op2 == X_IMUL_R_RM) {
        n = disassembleModRM(addr + 2, &reg, rm, 0, "");
        sprintf(text, "imul %s, %s", reg32Names[reg], rm);
        return 2 + n;
    }
    if (op2 == X_MOVZX_R_RM8) {
        n = disassembleModRM(addr + 2, &reg, rm, 1, "byte ");
        sprintf(text, "movzx %s, %s", reg32Names[reg], rm);
        return 2 + n;
    }
    sprintf(text, "db 0x0F");
    return 1;
}

// Writes the instruction at addr to text and returns its length. Bytes outside the subset are shown as db.
int disassemble(int addr, char *text) {
    int op = *MEM_AT(addr);
    int reg, n, imm, rel;
    char rm[64], target[32];
    if (op >= X_PUSH && op < X_PUSH + 8) { sprintf(text, "push %s", reg32Names[op - X_PUSH]); return 1; }
    if (op >= X_POP && op < X_POP + 8) { sprintf(text, "pop %s", reg32Names[op - X_POP]); return 1; }
    if (op >= X_MOV_R_IMM && op < X_MOV_R_IMM + 8) {
        sprintf(text, "mov %s, %d", reg32Names[op - X_MOV_R_IMM], readInt(addr + 1));
        return 5;
    }
    switch (op) {
        case X_ADD_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "add %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_SUB_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "sub %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_AND_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "and %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_OR_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "or %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_XOR_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "xor %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_CMP_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "cmp %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_TEST_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "test %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_MOV_RM8_R8:
            n = disassembleModRM(addr + 1, &reg, rm, 1, "");
            sprintf(text, "mov %s, %s", rm, reg8Names[reg]);
            return 1 + n;
        case X_MOV_RM_R:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "mov %s, %s", rm, reg32Names[reg]);
            return 1 + n;
        case X_MOV_R_RM:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            sprintf(text, "mov %s, %s", reg32Names[reg], rm);
            return 1 + n;
        case X_CDQ:
            sprintf(text, "cdq");
            return 1;
        case X_GRP1_RM_IMM32:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "dword ");
            imm = readInt(addr + 1 + n);
            if (reg == 0 || reg == 5 || reg == 7) {
                sprintf(text, "%s %s, %d", reg == 0 ? "add" : reg == 5 ? "sub" : "cmp", rm, imm);
                return 1 + n + 4;
            }
            break;
        case X_GRP2_RM_IMM8:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "dword ");
            if (reg == 4) {
                sprintf(text, "shl %s, %d", rm, *MEM_AT(addr + 1 + n));
                return 2 + n;
            }
            break;
        case X_GRP2_RM_CL:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "");
            if (reg == 4 || reg == 7) {
                sprintf(text, "%s %s, cl", reg == 4 ? "shl" : "sar", rm);
                return 1 + n;
            }
            break;
        case X_GRP3_RM:
            n = disassembleModRM(addr + 1, &reg, rm, 0, "dword ");
            if (reg == 2) {
                sprintf(text, "not %s", rm);
                return 1 + n;
            }
            if (reg == 7) {
                return 1 + n;
            }
            break;
        case X_RET:
            sprintf(text, "ret");
            return 1;
        case X_LEAVE:
            sprintf(text, "leave");
            return 1;
        case X_INT:
            sprintf(text, "int 0x%02X", *MEM_AT(addr + 1));
            return 2;
        case X_CALL:
            rel = readInt(addr + 1);
            targetText(target, addr + 5 + rel);
            sprintf(text, "call %s", target);
            return 5;
        case X_JMP:
            rel = readInt(addr + 1);
            targetText(target, addr + 5 + rel);
            sprintf(text, "jmp %s", target);
            return 5;
        case X_ESCAPE:
            return disassembleEscape(addr, text);
    }
    sprintf(text, "db 0x%02X", op);
    return 1;
}

// Each line shows an address, the instruction's bytes, and the instruction. A label line goes before each function.
void listing(void) {
    FILE *fp = fopen("listing.txt", "wt");
    qFail(!fp, "-cannot write listing.txt-");
    int p = CODE_BASE;
    while (p < here) {
        const char *label = labelAt(p);
        if (label) { fprintf(fp, "%s%s:\n", p > CODE_BASE ? "\n" : "", label); }
        char text[128], bytes[64] = "";
        int len = disassemble(p, text);
        for (int i = 0; i < len; i++) { sprintf(bytes + strlen(bytes), "%02X ", *MEM_AT(p + i)); }
        fprintf(fp, "%08X  %-30s %s\n", (unsigned)p, bytes, text);
        p += len;
    }
    fclose(fp);
}

// -------------------------------------------------
// Entry point
// -------------------------------------------------

int main(int argc, char* argv[]) {
    const char *source = NULL, *exeName = NULL;
    int lSpec = 0, rSpec = 0, hSpec = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-?") == 0) { hSpec = 1; }
        else if (strcmp(argv[i], "-l") == 0) { lSpec = 1; }
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) { exeName = argv[++i]; }
        else if (strcmp(argv[i], "-r") == 0) { rSpec = 1; }
        else { source = argv[i]; }
    }

    // Do not run if an executable is specified
    if (exeName) { rSpec = 0; }

    if ((!source) || hSpec) {
        printf("usage: %s [-l] [-o <exe>] [-r] [-?] <source>\n", argv[0]);
        printf("  compiles the specified source file\n");
        printf("  -l: create 'listing.txt' file\n");
        printf("  -o <exe>: output compiled executable to <exe>\n");
        printf("  -r: run in vm after compile (default no)\n");
        printf("  -?: show this help message\n");
        return 1;
    }
    if (!openSource(source)) { return 1; }
    c(program()); // The compiler
    if (lSpec) { listing(); }
    if (exeName) { saveExecutable(exeName); }
    if (rSpec) { return run(); }
    return 0;
}
