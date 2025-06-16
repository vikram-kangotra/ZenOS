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
    
    // Free imports
    if (module->imports) {
        for (uint32_t i = 0; i < module->import_count; i++) {
            if (module->imports[i].module_name) {
                kfree(module->imports[i].module_name);
            }
            if (module->imports[i].field_name) {
                kfree(module->imports[i].field_name);
            }
        }
        kfree(module->imports);
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

// Register a host function with the WebAssembly instance
bool wasm_register_host_function(wasm_instance_t* instance, const char* module_name, const char* field_name, 
                               wasm_functype_t type, wasm_host_function_t function) {
    if (!instance || !module_name || !field_name || !function) {
        kprintf(ERROR, "Invalid parameters for host function registration\n");
        return false;
    }

    // Check if this import exists in the module
    bool found = false;
    uint32_t import_index = 0;
    for (uint32_t i = 0; i < instance->module->import_count; i++) {
        if (strcmp(instance->module->imports[i].module_name, module_name) == 0 &&
            strcmp(instance->module->imports[i].field_name, field_name) == 0) {
            found = true;
            import_index = i;
            break;
        }
    }

    if (!found) {
        kprintf(ERROR, "Import '%s.%s' not found in module\n", module_name, field_name);
        return false;
    }

    // Verify function type matches
    wasm_import_t* import = &instance->module->imports[import_index];
    if (import->kind != 0) { // 0 = function
        kprintf(ERROR, "Import '%s.%s' is not a function\n", module_name, field_name);
        return false;
    }

    wasm_functype_t* import_type = &instance->module->types[import->type_index];
    if (import_type->param_count != type.param_count ||
        import_type->result_count != type.result_count) {
        kprintf(ERROR, "Function type mismatch for '%s.%s'\n", module_name, field_name);
        return false;
    }

    // Allocate or resize host functions array
    if (!instance->host_functions) {
        instance->host_functions = kmalloc(sizeof(wasm_host_function_t));
        if (!instance->host_functions) {
            kprintf(ERROR, "Failed to allocate host functions array\n");
            return false;
        }
        instance->host_functions[0] = function;  // Store the first function
        instance->host_function_count = 1;
    } else {
        // We already have the array allocated with size 1, just store the function
        instance->host_functions[0] = function;
    }

    kprintf(INFO, "Registered host function '%s.%s'\n", module_name, field_name);
    return true;
}

// Example proc_exit implementation
static bool host_proc_exit(wasm_instance_t* instance, wasm_value_t* args, uint32_t arg_count, wasm_value_t* result) {
    if (!instance || !args || arg_count != 1) {
        return false;
    }
    
    // Get exit code from first argument
    int32_t exit_code = args[0].i32;
    kprintf(INFO, "WebAssembly program exited with code %d\n", exit_code);
    
    // Set result to 0 to indicate successful exit
    if (result) {
        result->i32 = exit_code;
    }

    instance->should_exit = true;
    // Here you would implement the actual exit logic
    return true;
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
    
    // Initialize all fields to 0/NULL
    memset(instance, 0, sizeof(wasm_instance_t));
    
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
    
    // Initialize all functions to NULL/0
    memset(instance->functions, 0, sizeof(wasm_function_t) * module->function_count);
    
    // Copy function data
    for (uint32_t i = 0; i < module->function_count; i++) {
        instance->functions[i].type = module->functions[i].type;
        instance->functions[i].code = module->functions[i].code;
        instance->functions[i].code_size = module->functions[i].code_size;
        instance->functions[i].module = instance;
        instance->functions[i].local_count = module->functions[i].local_count;
    }
    instance->function_count = module->function_count;
    
    // Initialize globals
    instance->global_count = module->global_count;
    if (module->global_count > 0) {
        instance->globals = kmalloc(sizeof(wasm_global_t) * module->global_count);
        if (!instance->globals) {
            kprintf(ERROR, "Failed to allocate globals array\n");
            if (instance->memory) {
                kfree(instance->memory);
            }
            if (instance->functions) {
                kfree(instance->functions);
            }
            kfree(instance);
            return NULL;
        }
        memcpy(instance->globals, module->globals, 
               sizeof(wasm_global_t) * module->global_count);
    } else {
        instance->globals = NULL;
    }
    
    // Initialize host functions array
    instance->host_functions = NULL;
    instance->host_function_count = 0;
    
    // Register default host functions only if imported
    bool needs_proc_exit = false;
    for (uint32_t i = 0; i < module->import_count; i++) {
        if (module->imports[i].kind == 0 &&
            strcmp(module->imports[i].module_name, "wasi_snapshot_preview1") == 0 &&
            strcmp(module->imports[i].field_name, "proc_exit") == 0) {
            needs_proc_exit = true;
            break;
        }
    }
    if (needs_proc_exit) {
        // Allocate host functions array with size 1 since we only have one host function
        instance->host_functions = kmalloc(sizeof(wasm_host_function_t));
        if (!instance->host_functions) {
            kprintf(ERROR, "Failed to allocate host functions array\n");
            wasm_instance_delete(instance);
            return NULL;
        }
        instance->host_function_count = 1;
        
        wasm_functype_t proc_exit_type = {
            .params = kmalloc(sizeof(wasm_value_type_t)),
            .param_count = 1,
            .results = NULL,
            .result_count = 0
        };
        proc_exit_type.params[0] = WASM_I32;  // proc_exit takes one i32 parameter
        if (!wasm_register_host_function(instance, "wasi_snapshot_preview1", "proc_exit", 
                                       proc_exit_type, host_proc_exit)) {
            kprintf(ERROR, "Failed to register proc_exit host function\n");
            kfree(proc_exit_type.params);
            wasm_instance_delete(instance);
            return NULL;
        }
        kfree(proc_exit_type.params);
    }
    
    instance->should_exit = false;
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
    
    if (instance->globals) {
        kfree(instance->globals);
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