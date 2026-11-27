// Benchmarks

#include "lib/util.mc"

int mil(int x) { return x * 1000 * 1000; }

int whileBench(int sz) {
    int i = mil(sz);
    print("while ("); printNum(i, 10); print(") ...");
    do { i--; } while (i);
}

int fib(int n) {
    if (n<2) { return n; }
    return fib(n-1) + fib(n-2);
}

int fact(int n) {
    if (n < 2) { return 1; }
    return n * fact(n - 1);
}

int whileRet() {
    int x = 50;
    while (x--) {
        if (x == 33) {
            print("x==33, returning\n");
            return x+1;
        }
    }
    print("x==0, returning 0\n");
}

int main() {
    whileBench(1000); cr();
    print("fib(30): ");  printNum(fib(30), 10); cr();
    print("fact(14): "); printNum(fact(14), 10); cr();
    // whileRet();
    return 0;
}
