#include "wasm/wasm_kernel.h"
#include "wasm/wasm.h"
#include "wasm/wasm_exec.h"
#include "wasm/wasm_parser.h"
#include "kernel/mm/kmalloc.h"
#include "kernel/kprintf.h"
#include "fs/vfs.h"
#include <string.h>

#define O_RDONLY 0

bool wasm_load_module(const char* filename, wasm_module_t** module) {
    // Open the file
    struct vfs_node* node = vfs_open(filename, O_RDONLY);
    if (!node) {
        kprintf(ERROR, "Failed to open WebAssembly module: %s\n", filename);
        return false;
    }

    // Get file size
    uint32_t size = node->length;
    if (size == 0) {
        kprintf(ERROR, "Failed to get file size for: %s\n", filename);
        vfs_close(node);
        return false;
    }

    // Allocate buffer for module
    uint8_t* buffer = kmalloc(size);
    if (!buffer) {
        kprintf(ERROR, "Failed to allocate memory for module\n");
        vfs_close(node);
        return false;
    }

    // Read the file
    uint32_t bytes_read = vfs_read(node, 0, size, buffer);
    vfs_close(node);

    if (bytes_read != size) {
        kprintf(ERROR, "Failed to read entire module file\n");
        kfree(buffer);
        return false;
    }

    // Parse the module
    *module = wasm_module_new(buffer, size);
    if (!*module) {
        kprintf(ERROR, "Failed to parse WebAssembly module\n");
        kfree(buffer);
        return false;
    }

    return true;
}

bool wasm_execute_function_by_name(wasm_module_t* module, const char* function_name, uint64_t* result) {
    if (!module || !function_name) {
        return false;
    }

    // Find the function index
    int32_t func_idx = -1;
    for (size_t i = 0; i < module->export_count; i++) {
        if (strcmp(module->exports[i].name, function_name) == 0) {
            func_idx = module->exports[i].index;
            break;
        }
    }

    if (func_idx < 0) {
        kprintf(ERROR, "Function '%s' not found in module\n", function_name);
        return false;
    }

    // Create instance and execute
    wasm_instance_t* instance = wasm_instance_new(module);
    if (!instance) {
        kprintf(ERROR, "Failed to create WebAssembly instance\n");
        return false;
    }

    // Prepare arguments and result
    wasm_function_t* function = &instance->functions[func_idx];
    wasm_value_t args[2] = {0}; // Adjust as needed for your test functions
    wasm_value_t wasm_result = {0};
    bool success = wasm_execute_function(function, args, 0, &wasm_result);
    if (result) *result = wasm_result.i64;
    wasm_instance_delete(instance);
    return success;
}

bool wasm_execute_function_by_name_with_args(wasm_module_t* module, const char* function_name, wasm_value_t* args, uint32_t arg_count, uint64_t* result) {
    if (!module || !function_name) {
        return false;
    }

    // Find the function index
    int32_t func_idx = -1;
    for (size_t i = 0; i < module->export_count; i++) {
        if (strcmp(module->exports[i].name, function_name) == 0) {
            func_idx = module->exports[i].index;
            break;
        }
    }

    if (func_idx < 0) {
        kprintf(ERROR, "Function '%s' not found in module\n", function_name);
        return false;
    }

    // Create instance and execute
    wasm_instance_t* instance = wasm_instance_new(module);
    if (!instance) {
        kprintf(ERROR, "Failed to create WebAssembly instance\n");
        return false;
    }

    // Prepare arguments and result
    wasm_function_t* function = &instance->functions[func_idx];
    wasm_value_t wasm_result = {0};
    bool success = wasm_execute_function(function, args, arg_count, &wasm_result);
    if (result) *result = wasm_result.i64;
    wasm_instance_delete(instance);
    return success;
}

void wasm_test(void) {
    kprintf(INFO, "Initializing WebAssembly runtime...\n");

    // Load test module
    wasm_module_t* module = NULL;
    if (!wasm_load_module("/TEST.WSM", &module)) {
        kprintf(ERROR, "Failed to load test module\n");
        return;
    }

    // Run parser test before any execution
    wasm_parser_test(module->bytes, module->size);

    // Test add function
    uint64_t result;
    wasm_value_t add_args[2] = { {.i32 = 2}, {.i32 = 3} };
    if (wasm_execute_function_by_name_with_args(module, "add", add_args, 2, &result)) {
        kprintf(INFO, "add(2, 3) = %d\n", result);
    } else {
        kprintf(ERROR, "Failed to execute add function\n");
    }

    // Test mul function
    wasm_value_t mul_args[2] = { {.i32 = 4}, {.i32 = 5} };
    if (wasm_execute_function_by_name_with_args(module, "mul", mul_args, 2, &result)) {
        kprintf(INFO, "mul(4, 5) = %d\n", result);
    } else {
        kprintf(ERROR, "Failed to execute mul function\n");
    }

    // Cleanup
    wasm_module_delete(module);
    kprintf(INFO, "WebAssembly runtime test completed\n");
} 