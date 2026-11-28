// Some standard utilities

#include "lib/io.mc"

// ---------------------------------------------
// Strings are char arrays. Format:
// The characters, then a 0.
int strlen(char *text) {
    int n = 0;
    while (text[n]) { n++; }
    return n;
}

int strcatc(char *txt, int c) {
    int len = strlen(txt);
    txt[len] = c; txt[len+1] = 0;
}

// ---------------------------------------------
// file I/O. The flag and mode values are the Linux ones: O_RDONLY 0, O_WRONLY 1, O_CREAT 64, O_TRUNC 512,

// owner read/write, group read, others read - 0644 (base 8) is 420 (decimal)
const int kMode = 420;

// valid values for flags:
const int O_RDONLY = 0;
const int O_WRONLY = 1;
const int O_CREAT = 64;
const int O_TRUNC = 512;

// Opens the file named by the string name and returns its descriptor, or a negative errno on failure.
int fopen(char *name, int flags) { return syscall(5, name, flags, kMode); }

// Closes a file descriptor. Returns 0 on success.
int fclose(int fd) { return syscall(6, fd); }

// Reads up to n bytes into buf. Returns the number of bytes read.
int fread(int fd, char *buf, int n) { return syscall(3, fd, buf, n); }

// Writes n bytes from buf. Returns the number of bytes written.
int fwrite(int fd, char *buf, int n) { return syscall(4, fd, buf, n); }

// Deletes the file named by the string name. Returns 0 on success.
int fdelete(char *name) { return syscall(10, name); }
