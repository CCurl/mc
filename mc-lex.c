// ---------------------------------------------------------------------------
// Lexer for min-c

#include "mc.h"

// -------------------------------------------------
char *reservedWords[] = { "do", "else", "if", "while", "void", "int", "byte", "emit", "return", NULL };

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

int stringValues[MAX_STRING_LENGTH], stringLength;


void fail(const char *fmt, ...) {
    va_list args;
    printf("%s:%d: ", currentPath, lineNumber);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    exit(1);
}

void syntaxError() { fail("-syntax error-"); }

static int isAlpha(int ch) { return BTWI(ch,'A','Z') || BTWI(ch,'a','z') || (ch=='_'); }
static int isNum(int ch) { return BTWI(ch,'0','9'); }
static int isAlphaNum(int ch) { return isAlpha(ch) || isNum(ch); }

static int escapedCharacter(int ch) {
    switch (ch) {
      case 'n': return '\n';  case 'r': return '\r';  case 't': return '\t';
      case '0': return '\0';  case 'e': return 27;
      case '\\': return '\\'; case '\'': return '\'';  case '"': return '"';
      default: fail("-invalid character escape-");
    }
    return 0;
}

static void nextCh() {
    if (inputFp) { ch = fgetc(inputFp); }
    else { ch = getchar(); }
    if (ch == '\n') { lineNumber++; }
}

// Reads one character inside a character or string literal, decoding a backslash escape.
static int literalCharacter() {
    int value = ch;
    if (ch == '\\') {
        nextCh();
        value = escapedCharacter(ch);
    }
    nextCh();
    return value;
}

// Called after the first character of a one- or two-character symbol has been read.
static int symbolPair(int second, int twoCharSym, int oneCharSym) {
    if (ch != second) { return oneCharSym; }
    nextCh();
    return twoCharSym;
}

// Reads an identifier or directive name into idName.
static void readName(const char *tooLongMessage) {
    int i = 0;
    while (isAlphaNum(ch)) {
        if (i >= (int)sizeof(idName) - 1) { fail(tooLongMessage); }
        idName[i++] = ch;
        nextCh();
    }
    idName[i] = '\0';
}

int openSource(const char *path) {
    if (strlen(path) >= MAX_PATH_LENGTH) { fail("-path too long-"); }
    inputFp = fopen(path, "rt");
    if (!inputFp) { fail("-cannot open %s-\n", path); }
    strcpy(currentPath, path);
    strcpy(includedPaths[numIncludedPaths++], path);
    return 1;
}

static void resolveInclude(const char *name, char *out) {
    int dirLength = 0;
    for (int i = 0; currentPath[i] != '\0'; i++) {
        if (currentPath[i] == '/' || currentPath[i] == '\\') { dirLength = i + 1; }
    }
    if (dirLength + strlen(name) >= MAX_PATH_LENGTH) { fail("-include path too long-"); }
    snprintf(out, MAX_PATH_LENGTH, "%.*s%s", dirLength, currentPath, name);
}

static void includeFile() {
    char name[MAX_PATH_LENGTH], path[MAX_PATH_LENGTH];
    int length = 0;
    FILE *fp;
    while (ch == ' ' || ch == 9) { nextCh(); }
    if (ch != '"') { fail("-include expects a quoted file name-"); }
    nextCh();
    while (ch != '"') {
        if (ch == EOF || ch == '\n' || ch == '\r') { fail("-unterminated include file name-"); }
        if (length >= MAX_PATH_LENGTH - 1) { fail("-include file name too long-"); }
        name[length++] = ch;
        nextCh();
    }
    name[length] = '\0';
    nextCh();
    while (ch == ' ' || ch == 9) { nextCh(); }
    if (ch != ';') { fail("-include must end with ';'-"); }
    nextCh();
    resolveInclude(name, path);
    for (int i = 0; i < numIncludedPaths; i++) {
        if (strcmp(includedPaths[i], path) == 0) { return; }
    }
    if (numIncludedPaths >= MAX_INCLUDED_FILES) { fail("-too many included files-"); }
    if (sourceDepth >= MAX_INCLUDE_DEPTH) { fail("-includes nested too deeply-"); }
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

static void closeSource() {
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
          if (strcmp(idName, "include") != 0) { fail("-unknown directive-"); }
          includeFile();
          goto again;
      case '<': nextCh(); sym = symbolPair('=', LESS_EQ, LESS); break;
      case '>': nextCh(); sym = symbolPair('=', GRT_EQ, GRT); break;
      case '!': nextCh(); if (ch != '=') { syntaxError(); } nextCh(); sym = NE_SYM; break;
      case ';': nextCh(); sym = SEMI;  break;
      case '=': nextCh(); sym = symbolPair('=', EQ_SYM, EQUAL); break;
      case '[': nextCh(); sym = LBRACKET; break;
      case ']': nextCh(); sym = RBRACKET; break;
      case ',': nextCh(); sym = COMMA; break;
      case '\'':
        nextCh();
        if (ch == EOF || ch == '\n' || ch == '\r' || ch == '\'') {
            fail("-invalid character literal-");
        }
        intVal = literalCharacter();
        if (ch != '\'') { fail("-character literal must contain one character-"); }
        nextCh();
        sym = CHAR_LITERAL;
        break;
      case '"':
        stringLength = 0;
        nextCh();
        while (ch != '"') {
            if (ch == EOF || ch == '\n' || ch == '\r') {
                fail("-unterminated string literal-");
            }
            int value = literalCharacter();
            if (value > 127) { fail("-string literals must contain ASCII characters-"); }
            if (stringLength >= MAX_STRING_LENGTH) {
                fail("-string literal too long-");
            }
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
        } else { fail("-ch-\n"); }
        break;
    }
}
