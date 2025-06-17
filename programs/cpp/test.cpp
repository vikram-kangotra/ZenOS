#include <emscripten.h>

EMSCRIPTEN_KEEPALIVE
extern "C" int add(int a, int b) {
    return a + b;
}

EMSCRIPTEN_KEEPALIVE
extern "C" int mul(int a, int b) {
    return a * b;
}

int main() {
    return add(2, 3) + mul(4, 5);
}