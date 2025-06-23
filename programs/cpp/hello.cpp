#include <emscripten.h>

int fact(int a) {
    if (a == 0 || a == 1) return 1;
    return a * fact(a - 1);
}

__attribute__((import_module("env"), import_name("print")))
extern "C" void print(int a);

int main() {

    for (int i = 0; i < 10; ++i) {
        print(fact(i));
    }

    return 0;
}
