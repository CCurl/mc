// Some standard utilities

// ---------------------------------------------
// Strings are byte arrays. Format:
// A 2-byte little-endian length, the characters, then a 0.
int strlen(byte text[]) { return (text[1] * 256) + text[0]; }

void strcatc(byte txt[], int c) {
    int len = strlen(txt);
    txt[len+2] = c; txt[len+3] = 0;
    len = len+1;
    txt[0] = len%256; txt[1] = len/256;
}

// ---------------------------------------------
// common I/O functions

void print(byte str[]) {
    int len = strlen(str), i=2;
    while (len) { emit(str[i]); i=i+1; len=len-1; }
}

void printNum(int n, int b) {
    if (n < 0) { emit('-'); n = -n; }
    if (n > (b-1)) { printNum(n / b, b); }
    n = n % b;
    if (n > 9) { n=n+7; }
    emit(n+'0');
}

void cr() { emit('\n'); }
