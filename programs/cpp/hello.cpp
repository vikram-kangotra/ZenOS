#include <emscripten.h>

__attribute__((import_module("env"), import_name("print")))
extern "C" void print(int a);

int main() {

    print(42);

    return 0;
}