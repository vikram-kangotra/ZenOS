#include <emscripten.h>

__attribute__((import_module("env"), import_name("print")))
extern "C" void print(int a);

int main() {

    for (int i = 0; i < 10; ++i) {
        print(i);
    }

    return 0;
}
