// I/O standard utilities

// Writes the low byte of c to stdout and returns that byte.
// syscall 4: write to file descriptor (fd, buf, count)
// fd 1: stdout

byte emitBuf[1];
int emit(int c) {
    emitBuf[0] = c;
    syscall(4, 1, emitBuf, 1);
    return emitBuf[0];
}

void print(byte str[]) {
	int len = str[1] * 256 + str[0];
    syscall(4, 1, str+2, len);
}

void printNum(int n, int b) {
    if (n < 0) { emit('-'); n = -n; }
    if (n > (b-1)) { printNum(n / b, b); }
    n = n % b;
    if (n > 9) { n=n+7; }
    emit(n+'0');
}

void cr() { emit('\n'); }
