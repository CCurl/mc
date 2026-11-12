// ---------------------------------------------------------------------------
// Lexer for tiny-c

#include "tiny-c.h"

char *reservedWords[] = { "do", "else", "if", "while", "void", "int", "byte", "emit", "return", "include", NULL };

int ch = ' ', sym, intVal;
char idName[ID_NAME_LENGTH];

FILE *inputFp = NULL;
char currentPath[MAX_PATH_LENGTH] = "";
int lineNumber = 1;
struct source_s { FILE *fp; char path[MAX_PATH_LENGTH]; int line; int savedCh; };
struct source_s sourceStack[MAX_INCLUDE_DEPTH];
int sourceDepth = 0;
char includedPaths[MAX_INCLUDED_FILES][MAX_PATH_LENGTH];
int numIncludedPaths = 0;

char variableNames[MAX_NAMES][ID_NAME_LENGTH];
int variableBases[MAX_NAMES], variableSizes[MAX_NAMES];
int variableIsArray[MAX_NAMES];
int variableTypes[MAX_NAMES];
int stringValues[MAX_STRING_LENGTH], stringLength;
int globals[MAX_GLOBAL_SLOTS];
byte globalBytes[MAX_GLOBAL_BYTES];
int numVariables;
int numGlobalSlots;
int numGlobalBytes;

void fail(const char *fmt, ...) {
    va_list args;
    printf("%s:%d: ", currentPath, lineNumber);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    exit(1);
}
void error(char *err) { fail("%s\n", err); }
void syntaxError() { error("-syntax error-"); }

int findVariable(const char *name) {
    for (int i = 0; i < numVariables; i++) {
        if (strcmp(variableNames[i], name) == 0) { return i; }
    }
    return -1;
}

int declareVariable(const char *name, int size, int isArray, int type) {
    int index = findVariable(name);
    if (index >= 0) {
        fail("-duplicate variable %s-\n", name);
    }
    if (size <= 0) { error("-array size must be positive-"); }
    int isByte = type == TYPE_BYTE;
    int *used = isByte ? &numGlobalBytes : &numGlobalSlots;
    int capacity = isByte ? MAX_GLOBAL_BYTES : MAX_GLOBAL_SLOTS;
    if (numVariables >= MAX_NAMES || size > capacity - *used) {
        error("-too many variables-");
    }
    strcpy(variableNames[numVariables], name);
    variableBases[numVariables] = *used;
    variableSizes[numVariables] = size;
    variableIsArray[numVariables] = isArray;
    variableTypes[numVariables] = type;
    *used += size;
    return numVariables++;
}

/* Byte arrays hold a 2-byte little-endian length, the characters, then a 0. */
#define STRING_OVERHEAD 3

void storeStringBytes(int base) {
    globalBytes[base] = stringLength & 0xff;
    globalBytes[base + 1] = (stringLength >> 8) & 0xff;
    for (int i = 0; i < stringLength; i++) {
        globalBytes[base + i + 2] = stringValues[i];
    }
}

int allocateStringLiteral() {
    int size = stringLength + STRING_OVERHEAD;
    if (size > MAX_GLOBAL_BYTES - numGlobalBytes) {
        error("-too much global storage-");
    }
    int base = numGlobalBytes;
    numGlobalBytes += size;
    storeStringBytes(base);
    return base;
}

int parameterIndex(const char *name);
int localIndex(const char *name);
int currentParameterIsArray(int parameter);

int variableIndex(const char *name) {
    int parameter = parameterIndex(name);
    if (parameter >= 0) {
        if (currentParameterIsArray(parameter)) {
            fail("-array parameter %s requires an index-\n", name);
        }
        return -parameter - 1;
    }
    int index = findVariable(name);
    if (index < 0) {
        fail("-undeclared variable %s-\n", name);
    }
    if (variableIsArray[index]) {
        fail("-array %s requires an index-\n", name);
    }
    return variableBases[index];
}

int arrayIndex(const char *name) {
    if (localIndex(name) >= 0) {
        fail("-local variable %s is not an array-\n", name);
    }
    int parameter = parameterIndex(name);
    if (parameter >= 0) {
        if (!currentParameterIsArray(parameter)) {
            fail("-parameter %s is not an array-\n", name);
        }
        return -parameter - 1;
    }
    int index = findVariable(name);
    if (index < 0) {
        fail("-undeclared variable %s-\n", name);
    }
    if (!variableIsArray[index]) {
        fail("-variable %s is not an array-\n", name);
    }
    return variableBases[index];
}

int arrayReference(const char *name) {
    if (localIndex(name) >= 0) {
        fail("-local variable %s is not an array-\n", name);
    }
    int parameter = parameterIndex(name);
    if (parameter >= 0) {
        if (currentParameterIsArray(parameter)) {
            return -parameter - 1;
        }
        fail("-parameter %s is not an array-\n", name);
    }
    int index = findVariable(name);
    if (index < 0) {
        fail("-undeclared variable %s-\n", name);
    }
    if (!variableIsArray[index]) {
        fail("-variable %s is not an array-\n", name);
    }
    return variableBases[index];
}

int isAlpha(int ch) { return BTWI(ch,'A','Z') || BTWI(ch,'a','z') || (ch=='_'); }
int isNum(int ch) { return BTWI(ch,'0','9'); }
int isAlphaNum(int ch) { return isAlpha(ch) || isNum(ch); }

int escapedCharacter(int ch) {
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

void nextCh() {
    if (inputFp) { ch = fgetc(inputFp); }
    else { ch = getchar(); }
    if (ch == '\n') { lineNumber++; }
}

void openSource(const char *path) {
    if (strlen(path) >= MAX_PATH_LENGTH) { error("-path too long-"); }
    inputFp = fopen(path, "rt");
    if (!inputFp) { fail("-cannot open %s-\n", path); }
    strcpy(currentPath, path);
    strcpy(includedPaths[numIncludedPaths++], path);
}

void resolveInclude(const char *name, char *out) {
    int dirLength = 0;
    for (int i = 0; currentPath[i] != '\0'; i++) {
        if (currentPath[i] == '/' || currentPath[i] == '\\') { dirLength = i + 1; }
    }
    if (dirLength + strlen(name) >= MAX_PATH_LENGTH) { error("-include path too long-"); }
    snprintf(out, MAX_PATH_LENGTH, "%.*s%s", dirLength, currentPath, name);
}

void includeFile() {
    char name[MAX_PATH_LENGTH], path[MAX_PATH_LENGTH];
    int length = 0;
    FILE *fp;
    while (ch == ' ' || ch == 9) { nextCh(); }
    if (ch != '"') { error("-include expects a quoted file name-"); }
    nextCh();
    while (ch != '"') {
        if (ch == EOF || ch == '\n' || ch == '\r') { error("-unterminated include file name-"); }
        if (length >= MAX_PATH_LENGTH - 1) { error("-include file name too long-"); }
        name[length++] = ch;
        nextCh();
    }
    name[length] = '\0';
    nextCh();
    while (ch == ' ' || ch == 9) { nextCh(); }
    if (ch != ';') { error("-include must end with ';'-"); }
    nextCh();
    resolveInclude(name, path);
    for (int i = 0; i < numIncludedPaths; i++) {
        if (strcmp(includedPaths[i], path) == 0) { return; }
    }
    if (numIncludedPaths >= MAX_INCLUDED_FILES) { error("-too many included files-"); }
    if (sourceDepth >= MAX_INCLUDE_DEPTH) { error("-includes nested too deeply-"); }
    fp = fopen(path, "rt");
    if (!fp) { fail("-cannot open include file %s-\n", path); }
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
      case '+': nextCh(); if (ch == '+') { nextCh(); sym = PLUS_PLUS; } else { sym = PLUS; } break;
      case '-': nextCh(); if (ch == '-') { nextCh(); sym = MINUS_MINUS; } else { sym = MINUS; } break;
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
      case '<': nextCh(); sym = LESS;  break;
      case '>': nextCh(); sym = GRT;   break;
      case '!':
        nextCh();
        if (ch != '=') { syntaxError(); }
        nextCh();
        sym = NE_SYM;
        break;
      case ';': nextCh(); sym = SEMI;  break;
      case '=':
        nextCh();
        if (ch == '=') {
            nextCh();
            sym = EQ_SYM;
        } else {
            sym = EQUAL;
        }
        break;
      case '[': nextCh(); sym = LBRACKET; break;
      case ']': nextCh(); sym = RBRACKET; break;
      case ',': nextCh(); sym = COMMA; break;
      case '\'':
        nextCh();
        if (ch == EOF || ch == '\n' || ch == '\r' || ch == '\'') {
            error("-invalid character literal-");
        }
        if (ch == '\\') {
            nextCh();
            intVal = escapedCharacter(ch);
        } else {
            intVal = ch;
        }
        nextCh();
        if (ch != '\'') { error("-character literal must contain one character-"); }
        nextCh();
        sym = CHAR_LITERAL;
        break;
      case '"':
        stringLength = 0;
        nextCh();
        while (ch != '"') {
            if (ch == EOF || ch == '\n' || ch == '\r') {
                error("-unterminated string literal-");
            }
            int value;
            if (ch == '\\') {
                nextCh();
                value = escapedCharacter(ch);
            } else {
                value = ch;
            }
            if (value > 127) { error("-string literals must contain ASCII characters-"); }
            if (stringLength >= MAX_STRING_LENGTH) {
                error("-string literal too long-");
            }
            stringValues[stringLength++] = value;
            nextCh();
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
            int i = 0;
            while (isAlphaNum(ch)) {
                if (i >= (int)sizeof(idName) - 1) { error("-identifier too long-"); }
                idName[i++]=ch;
                nextCh();
            }
            idName[i] = '\0';
            sym = 0;
            while (reservedWords[sym] != NULL && strcmp(reservedWords[sym], idName) != 0) { sym++; }
            if (reservedWords[sym] == NULL) {
                sym = ID;
            } else if (sym == INCLUDE_SYM) {
                includeFile();
                goto again;
            }
        } else { fail("-ch-\n"); }
        break;
    }
}
