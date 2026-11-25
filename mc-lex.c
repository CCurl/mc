// ---------------------------------------------------------------------------
// Lexer for min-c

#include "mc.h"

// -------------------------------------------------
char *reservedWords[] = { "do", "else", "if", "while", "void", "int", "byte", "return", "const", NULL };

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

static int isAlpha(int ch) { return BTWI(ch,'A','Z') || BTWI(ch,'a','z') || (ch=='_'); }
static int isNum(int ch) { return BTWI(ch,'0','9'); }
static int isAlphaNum(int ch) { return isAlpha(ch) || isNum(ch); }

static int nextCh() {
    if (inputFp) { ch = fgetc(inputFp); }
    else { ch = getchar(); }
    if (ch == '\n') { lineNumber++; }
    return ch;
}

// Reads one character inside a character or string literal, decoding a backslash escape.
static int literalCharacter() {
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
static int symbolPair(int second, int twoCharSym, int oneCharSym) {
    if (ch != second) { return oneCharSym; }
    nextCh();
    return twoCharSym;
}

// Reads an identifier or directive name into idName.
static void readName(const char *tooLongMessage) {
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

static void includeFile() {
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
        } else { qFail(1, "-ch '%c' expected to be alpha-\n", ch); }
        break;
    }
}
