// I/O standard utilities

// Writes the low byte of c to stdout and returns that byte.
// syscall 4: write to file descriptor (fd, buf, count)
// fd 1: stdout

char emitBuf[1];
int emit(int c) {
    emitBuf[0] = c;
    syscall(4, 1, emitBuf, 1);
    return emitBuf[0];
}

int print(char *str) {
    int len = 0;
    while (str[len]) { len++; }
    syscall(4, 1, str, len);
}

int printNum(int n, int b) {
    if (n < 0) { emit('-'); n = -n; }
    if (n > (b-1)) { printNum(n / b, b); }
    n = n % b;
    if (n > 9) { n=n+7; }
    emit(n+'0');
}

int cr() { emit('\n'); }
