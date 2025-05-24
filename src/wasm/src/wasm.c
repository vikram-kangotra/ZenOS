#include "wasm/wasm.h"
#include "kernel/mm/kmalloc.h"
#include "kernel/kprintf.h"
#include "wasm/wasm_parser.h"
#include "wasm/wasm_exec.h"
#include <string.h>

// Helper function to validate WebAssembly module magic number and version
static bool validate_wasm_header(const uint8_t* bytes) {
    // Check magic number (0x00 0x61 0x73 0x6D)
    if (bytes[0] != 0x00 || bytes[1] != 0x61 || 
        bytes[2] != 0x73 || bytes[3] != 0x6D) {
        kprintf(ERROR, "Invalid WebAssembly magic number\n");
        return false;
    }
    
    // Check version (0x01 0x00 0x00 0x00)
    if (bytes[4] != 0x01 || bytes[5] != 0x00 || 
        bytes[6] != 0x00 || bytes[7] != 0x00) {
        kprintf(ERROR, "Unsupported WebAssembly version\n");
        return false;
    }
    
    return true;
}

// Create a new WebAssembly module
wasm_module_t* wasm_module_new(const uint8_t* bytes, size_t size) {
    if (!bytes || size < 8) {
        kprintf(ERROR, "Invalid WebAssembly module data\n");
        return NULL;
    }
    
    // Validate WebAssembly header
    if (!validate_wasm_header(bytes)) {
        return NULL;
    }
    
    // Allocate module structure
    wasm_module_t* module = kmalloc(sizeof(wasm_module_t));
    if (!module) {
        kprintf(ERROR, "Failed to allocate WebAssembly module structure\n");
        return NULL;
    }
    
    // Initialize module fields
    module->bytes = kmalloc(size);
    if (!module->bytes) {
        kprintf(ERROR, "Failed to allocate %d bytes for module data\n", size);
        kfree(module);
        return NULL;
    }
    
    memcpy(module->bytes, bytes, size);
    module->size = size;
    module->types = NULL;
    module->type_count = 0;
    module->functions = NULL;
    module->function_count = 0;
    module->exports = NULL;
    module->export_count = 0;
    module->memory_initial = 0;
    module->memory_max = 0;
    
    kprintf(INFO, "Initializing WebAssembly module with %d bytes\n", size);
    
    // Parse module sections
    if (!wasm_parse_module(module)) {
        kprintf(ERROR, "Failed to parse WebAssembly module\n");
        wasm_module_delete(module);
        return NULL;
    }
    
    kprintf(INFO, "Successfully loaded WebAssembly module with %d functions\n", module->function_count);
    return module;
}

// Delete a WebAssembly module
void wasm_module_delete(wasm_module_t* module) {
    if (!module) return;
    
    // Free function types
    if (module->types) {
        for (uint32_t i = 0; i < module->type_count; i++) {
            if (module->types[i].params) {
                kfree(module->types[i].params);
            }
            if (module->types[i].results) {
                kfree(module->types[i].results);
            }
        }
        kfree(module->types);
    }
    
    // Free functions
    if (module->functions) {
        for (uint32_t i = 0; i < module->function_count; i++) {
            if (module->functions[i].code) {
                kfree(module->functions[i].code);
            }
        }
        kfree(module->functions);
    }
    
    // Free exports
    if (module->exports) {
        for (uint32_t i = 0; i < module->export_count; i++) {
            if (module->exports[i].name) {
                kfree(module->exports[i].name);
            }
        }
        kfree(module->exports);
    }
    
    // Free module bytes
    if (module->bytes) {
        kfree(module->bytes);
    }
    
    // Free module structure
    kfree(module);
}

// Create a new WebAssembly instance
wasm_instance_t* wasm_instance_new(wasm_module_t* module) {
    if (!module) {
        kprintf(ERROR, "Invalid WebAssembly module\n");
        return NULL;
    }
    
    // Allocate instance structure
    wasm_instance_t* instance = kmalloc(sizeof(wasm_instance_t));
    if (!instance) {
        kprintf(ERROR, "Failed to allocate WebAssembly instance\n");
        return NULL;
    }
    
    instance->module = module;
    
    // Initialize memory if needed
    if (module->memory_initial > 0) {
        size_t memory_size = module->memory_initial * 65536;  // 64KB pages
        instance->memory = kmalloc(memory_size);
        if (!instance->memory) {
            kprintf(ERROR, "Failed to allocate WebAssembly memory\n");
            kfree(instance);
            return NULL;
        }
        instance->memory_size = memory_size;
    } else {
        instance->memory = NULL;
        instance->memory_size = 0;
    }
    
    // Copy functions
    instance->functions = kmalloc(sizeof(wasm_function_t) * module->function_count);
    if (!instance->functions) {
        kprintf(ERROR, "Failed to allocate function array\n");
        if (instance->memory) {
            kfree(instance->memory);
        }
        kfree(instance);
        return NULL;
    }
    
    memcpy(instance->functions, module->functions, 
           sizeof(wasm_function_t) * module->function_count);
    instance->function_count = module->function_count;
    
    return instance;
}

// Delete a WebAssembly instance
void wasm_instance_delete(wasm_instance_t* instance) {
    if (!instance) return;
    
    if (instance->memory) {
        kfree(instance->memory);
    }
    
    if (instance->functions) {
        kfree(instance->functions);
    }
    
    kfree(instance);
}

// Call a WebAssembly function
bool wasm_function_call(wasm_function_t* function, wasm_value_t* args, uint32_t arg_count, wasm_value_t* result) {
    if (!function || !function->code || !result) {
        kprintf(ERROR, "Invalid WebAssembly function call\n");
        return false;
    }
    
    // Validate argument count
    if (arg_count != function->type->param_count) {
        kprintf(ERROR, "Invalid argument count: expected %d, got %d\n",
                function->type->param_count, arg_count);
        return false;
    }
    
    // Execute the function
    return wasm_execute_function(function, args, arg_count, result);
} 