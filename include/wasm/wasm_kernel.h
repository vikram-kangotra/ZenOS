#pragma once

#include "wasm/wasm.h"

// Load a WebAssembly module from a file
bool wasm_load_module(const char* filename, wasm_module_t** module);

// Execute a function by name from a loaded module
bool wasm_execute_function_by_name(wasm_module_t* module, const char* function_name, uint64_t* result);

// Test the WebAssembly runtime
void wasm_test(void); 