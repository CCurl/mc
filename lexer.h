// ---------------------------------------------------------------------------
// Lexer for tiny-c

#include "tiny-c.h"

char *words[] = { "do", "else", "if", "while", "void", "int", NULL };

int ch = ' ', sym, int_val;
char id_name[64];

FILE *input_fp = NULL;
char current_path[MAX_PATH_LENGTH] = "";
int line_number = 1;
struct source_s { FILE *fp; char path[MAX_PATH_LENGTH]; int line; int saved_ch; };
struct source_s source_stack[MAX_INCLUDE_DEPTH];
int source_depth = 0;
char included_paths[MAX_INCLUDED_FILES][MAX_PATH_LENGTH];
int num_included_paths = 0;

char variable_names[MAX_NAMES][sizeof(id_name)];
int variable_bases[MAX_NAMES], variable_sizes[MAX_NAMES];
int variable_is_array[MAX_NAMES];
int string_values[MAX_STRING_LENGTH], string_length;
int globals[MAX_GLOBAL_SLOTS];
int num_variables;
int num_global_slots;

void fail(const char *fmt, ...) {
    va_list args;
    printf("%s:%d: ", current_path, line_number);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    exit(1);
}
void error(char *err) { fail("%s\n", err); }
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
        fail("-duplicate variable %s-\n", name);
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
            fail("-array parameter %s requires an index-\n", name);
        }
        return -parameter - 1;
    }
    int index = find_variable(name);
    if (index < 0) {
        fail("-undeclared variable %s-\n", name);
    }
    if (variable_is_array[index]) {
        fail("-array %s requires an index-\n", name);
    }
    return variable_bases[index];
}

int array_index(const char *name) {
    if (local_index(name) >= 0) {
        fail("-local variable %s is not an array-\n", name);
    }
    int parameter = parameter_index(name);
    if (parameter >= 0) {
        if (!current_parameter_is_array(parameter)) {
            fail("-parameter %s is not an array-\n", name);
        }
        return -parameter - 1;
    }
    int index = find_variable(name);
    if (index < 0) {
        fail("-undeclared variable %s-\n", name);
    }
    if (!variable_is_array[index]) {
        fail("-variable %s is not an array-\n", name);
    }
    return variable_bases[index];
}

int array_reference(const char *name) {
    if (local_index(name) >= 0) {
        fail("-local variable %s is not an array-\n", name);
    }
    int parameter = parameter_index(name);
    if (parameter >= 0) {
        if (current_parameter_is_array(parameter)) {
            return -parameter - 1;
        }
        fail("-parameter %s is not an array-\n", name);
    }
    int index = find_variable(name);
    if (index < 0) {
        fail("-undeclared variable %s-\n", name);
    }
    if (!variable_is_array[index]) {
        fail("-variable %s is not an array-\n", name);
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
    if (ch == '\n') { line_number++; }
}

void open_source(const char *path) {
    if (strlen(path) >= MAX_PATH_LENGTH) { error("-path too long-"); }
    input_fp = fopen(path, "rt");
    if (!input_fp) { fail("-cannot open %s-\n", path); }
    strcpy(current_path, path);
    strcpy(included_paths[num_included_paths++], path);
}

void resolve_include(const char *name, char *out) {
    int dir_length = 0;
    for (int i = 0; current_path[i] != '\0'; i++) {
        if (current_path[i] == '/' || current_path[i] == '\\') { dir_length = i + 1; }
    }
    if (dir_length + strlen(name) >= MAX_PATH_LENGTH) { error("-include path too long-"); }
    snprintf(out, MAX_PATH_LENGTH, "%.*s%s", dir_length, current_path, name);
}

void include_file() {
    char name[MAX_PATH_LENGTH], path[MAX_PATH_LENGTH];
    int length = 0;
    FILE *fp;
    while (ch == ' ' || ch == 9) { next_ch(); }
    if (ch != '"') { error("-include expects a quoted file name-"); }
    next_ch();
    while (ch != '"') {
        if (ch == EOF || ch == '\n' || ch == '\r') { error("-unterminated include file name-"); }
        if (length >= MAX_PATH_LENGTH - 1) { error("-include file name too long-"); }
        name[length++] = ch;
        next_ch();
    }
    name[length] = '\0';
    next_ch();
    while (ch == ' ' || ch == 9) { next_ch(); }
    if (ch != ';') { error("-include must end with ';'-"); }
    next_ch();
    resolve_include(name, path);
    for (int i = 0; i < num_included_paths; i++) {
        if (strcmp(included_paths[i], path) == 0) { return; }
    }
    if (num_included_paths >= MAX_INCLUDED_FILES) { error("-too many included files-"); }
    if (source_depth >= MAX_INCLUDE_DEPTH) { error("-includes nested too deeply-"); }
    fp = fopen(path, "rt");
    if (!fp) { fail("-cannot open include file %s-\n", path); }
    strcpy(included_paths[num_included_paths++], path);
    struct source_s *outer = &source_stack[source_depth++];
    outer->fp = input_fp;
    strcpy(outer->path, current_path);
    outer->line = line_number;
    outer->saved_ch = ch;
    input_fp = fp;
    strcpy(current_path, path);
    line_number = 1;
    next_ch();
}

void close_source() {
    fclose(input_fp);
    source_depth--;
    struct source_s *outer = &source_stack[source_depth];
    input_fp = outer->fp;
    strcpy(current_path, outer->path);
    line_number = outer->line;
    ch = outer->saved_ch;
}

void next_sym() {
  again:
    switch (ch) {
      case ' ': case 9: case 10: case 13: next_ch(); goto again;
      case EOF:
        if (source_depth > 0) { close_source(); goto again; }
        sym = EOI; break;
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
            if (strcmp(id_name, "include") == 0) {
                include_file();
                goto again;
            }
            if (strcmp(id_name, "return") == 0) {
                sym = RETURN_SYM;
            } else {
              sym = 0;
              while (words[sym] != NULL && strcmp(words[sym], id_name) != 0) { sym++; }
              if (words[sym] == NULL) {
                  sym = ID;
              }
            }
        } else { fail("-ch-\n"); }
        break;
    }
}
