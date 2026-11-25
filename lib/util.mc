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

byte emitBuf[1];

// Writes the low byte of c to stdout and returns that byte.
int emit(int c) {
    emitBuf[0] = c;
    syscall(4, 1, emitBuf, 1);
    return emitBuf[0];
}

void print(byte str[]) {
    syscall(4, 1, str+2, strlen(str));
}

void printNum(int n, int b) {
    if (n < 0) { emit('-'); n = -n; }
    if (n > (b-1)) { printNum(n / b, b); }
    n = n % b;
    if (n > 9) { n=n+7; }
    emit(n+'0');
}

void cr() { emit('\n'); }

// ---------------------------------------------
// file I/O. The flag and mode values are the Linux ones: O_RDONLY 0, O_WRONLY 1, O_CREAT 64, O_TRUNC 512,
// and mode 0644 is 420.

// Opens the file named by the string name (mc string format) and returns its descriptor, or a negative errno on failure.
int fopen(byte name[], int flags) { return syscall(5, name+2, flags, 420); }

// Closes a file descriptor. Returns 0 on success.
int fclose(int fd) { return syscall(6, fd); }

// Reads up to n bytes into buf. Returns the number of bytes read.
int fread(int fd, byte buf[], int n) { return syscall(3, fd, buf, n); }

// Writes n bytes from buf. Returns the number of bytes written.
int fwrite(int fd, byte buf[], int n) { return syscall(4, fd, buf, n); }
