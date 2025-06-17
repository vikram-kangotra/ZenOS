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
            kprintf(DEBUG, "[WASM] Found import '%s.%s' at index %u\n", module_name, field_name, i);
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

    // Store the function at the correct import index
    instance->host_functions[import_index] = function;
    kprintf(DEBUG, "[WASM] Registered host function '%s.%s' at import index %u\n", module_name, field_name, import_index);
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

// Host print function implementation
static bool host_print(wasm_instance_t* instance, wasm_value_t* args, uint32_t arg_count, wasm_value_t* result) {

    (void) result;

    if (!instance || !args || arg_count != 1) {
        return false;
    }
    
    // Get the value to print from the first argument
    int32_t value = args[0].i32;
    kprintf(INFO, "WebAssembly print: %d\n", value);
    
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
    uint32_t total_functions = module->import_count + module->function_count;
    instance->functions = kmalloc(sizeof(wasm_function_t) * total_functions);
    if (!instance->functions) {
        kprintf(ERROR, "Failed to allocate function array\n");
        if (instance->memory) {
            kfree(instance->memory);
        }
        kfree(instance);
        return NULL;
    }
    // Initialize all functions to NULL/0
    memset(instance->functions, 0, sizeof(wasm_function_t) * total_functions);
    
    // Fill in defined functions at the correct indices
    for (uint32_t i = 0; i < module->function_count; i++) {
        uint32_t idx = module->import_count + i;
        instance->functions[idx].type = module->functions[i].type;
        instance->functions[idx].code = module->functions[i].code;
        instance->functions[idx].code_size = module->functions[i].code_size;
        instance->functions[idx].module = instance;
        instance->functions[idx].local_count = module->functions[i].local_count;
        kprintf(DEBUG, "[WASM] Copied function %u to index %u\n", i, idx);
    }
    instance->function_count = total_functions;

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
    bool needs_print = false;
    for (uint32_t i = 0; i < module->import_count; i++) {
        if (module->imports[i].kind == 0) {
            if (strcmp(module->imports[i].module_name, "wasi_snapshot_preview1") == 0 &&
                strcmp(module->imports[i].field_name, "proc_exit") == 0) {
                needs_proc_exit = true;
            }
            if (strcmp(module->imports[i].module_name, "env") == 0 &&
                strcmp(module->imports[i].field_name, "print") == 0) {
                needs_print = true;
            }
        }
    }

    // Allocate space for all imports
    if (module->import_count > 0) {
        instance->host_functions = kmalloc(sizeof(wasm_host_function_t) * module->import_count);
        if (!instance->host_functions) {
            kprintf(ERROR, "Failed to allocate host functions array\n");
            wasm_instance_delete(instance);
            return NULL;
        }
        // Initialize all host functions to NULL
        memset(instance->host_functions, 0, sizeof(wasm_host_function_t) * module->import_count);
        instance->host_function_count = module->import_count;
        
        if (needs_proc_exit) {
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

        if (needs_print) {
            wasm_functype_t print_type = {
                .params = kmalloc(sizeof(wasm_value_type_t)),
                .param_count = 1,
                .results = NULL,
                .result_count = 0
            };
            print_type.params[0] = WASM_I32;  // print takes one i32 parameter

            // Find the import type for print
            for (uint32_t i = 0; i < module->import_count; i++) {
                if (strcmp(module->imports[i].module_name, "env") == 0 &&
                    strcmp(module->imports[i].field_name, "print") == 0) {
                    wasm_functype_t* import_type = &module->types[module->imports[i].type_index];
                    kprintf(DEBUG, "Print function type: param_count=%d, result_count=%d\n",
                           import_type->param_count, import_type->result_count);
                    if (import_type->param_count > 0) {
                        kprintf(DEBUG, "First param type: 0x%x\n", import_type->params[0]);
                    }
                    if (import_type->result_count > 0) {
                        kprintf(DEBUG, "First result type: 0x%x\n", import_type->results[0]);
                    }
                    break;
                }
            }

            if (!wasm_register_host_function(instance, "env", "print", 
                                           print_type, host_print)) {
                kprintf(ERROR, "Failed to register print host function\n");
                kfree(print_type.params);
                wasm_instance_delete(instance);
                return NULL;
            }
            kfree(print_type.params);
        }
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

// Create a new import object
wasm_import_object_t* wasm_import_object_new(void) {
    wasm_import_object_t* import_object = kmalloc(sizeof(wasm_import_object_t));
    if (!import_object) {
        kprintf(ERROR, "Failed to allocate import object\n");
        return NULL;
    }
    
    import_object->functions = NULL;
    import_object->function_count = 0;
    import_object->function_capacity = 0;
    
    return import_object;
}

// Delete an import object
void wasm_import_object_delete(wasm_import_object_t* import_object) {
    if (!import_object) return;
    
    if (import_object->functions) {
        for (uint32_t i = 0; i < import_object->function_count; i++) {
            if (import_object->functions[i].module_name) {
                kfree(import_object->functions[i].module_name);
            }
            if (import_object->functions[i].field_name) {
                kfree(import_object->functions[i].field_name);
            }
            if (import_object->functions[i].type.params) {
                kfree(import_object->functions[i].type.params);
            }
            if (import_object->functions[i].type.results) {
                kfree(import_object->functions[i].type.results);
            }
        }
        kfree(import_object->functions);
    }
    
    kfree(import_object);
}

// Add a function to the import object
bool wasm_import_object_add_function(wasm_import_object_t* import_object, const char* module_name, 
                                   const char* field_name, wasm_functype_t type, wasm_host_function_t function) {
    if (!import_object || !module_name || !field_name || !function) {
        kprintf(ERROR, "Invalid parameters for import object function\n");
        return false;
    }
    
    // Check if we need to resize the functions array
    if (import_object->function_count >= import_object->function_capacity) {
        uint32_t new_capacity = import_object->function_capacity == 0 ? 4 : import_object->function_capacity * 2;
        wasm_host_function_def_t* new_functions = kmalloc(sizeof(wasm_host_function_def_t) * new_capacity);
        if (!new_functions) {
            kprintf(ERROR, "Failed to resize import object functions array\n");
            return false;
        }
        
        // Copy existing functions
        if (import_object->functions) {
            memcpy(new_functions, import_object->functions, 
                   sizeof(wasm_host_function_def_t) * import_object->function_count);
            kfree(import_object->functions);
        }
        
        import_object->functions = new_functions;
        import_object->function_capacity = new_capacity;
    }
    
    // Add the new function
    wasm_host_function_def_t* def = &import_object->functions[import_object->function_count];
    
    // Copy module name
    def->module_name = kmalloc(strlen(module_name) + 1);
    if (!def->module_name) {
        kprintf(ERROR, "Failed to allocate module name\n");
        return false;
    }
    strcpy(def->module_name, module_name);
    
    // Copy field name
    def->field_name = kmalloc(strlen(field_name) + 1);
    if (!def->field_name) {
        kprintf(ERROR, "Failed to allocate field name\n");
        kfree(def->module_name);
        return false;
    }
    strcpy(def->field_name, field_name);
    
    // Copy function type
    def->type.param_count = type.param_count;
    def->type.result_count = type.result_count;
    
    if (type.param_count > 0) {
        def->type.params = kmalloc(sizeof(wasm_value_type_t) * type.param_count);
        if (!def->type.params) {
            kprintf(ERROR, "Failed to allocate parameter types\n");
            kfree(def->module_name);
            kfree(def->field_name);
            return false;
        }
        memcpy(def->type.params, type.params, sizeof(wasm_value_type_t) * type.param_count);
    } else {
        def->type.params = NULL;
    }
    
    if (type.result_count > 0) {
        def->type.results = kmalloc(sizeof(wasm_value_type_t) * type.result_count);
        if (!def->type.results) {
            kprintf(ERROR, "Failed to allocate result types\n");
            kfree(def->module_name);
            kfree(def->field_name);
            if (def->type.params) kfree(def->type.params);
            return false;
        }
        memcpy(def->type.results, type.results, sizeof(wasm_value_type_t) * type.result_count);
    } else {
        def->type.results = NULL;
    }
    
    def->function = function;
    import_object->function_count++;
    
    kprintf(INFO, "Added host function '%s.%s' to import object\n", module_name, field_name);
    return true;
}

// Set the import object for an instance
bool wasm_instance_set_import_object(wasm_instance_t* instance, wasm_import_object_t* import_object) {
    if (!instance || !import_object) {
        kprintf(ERROR, "Invalid parameters for setting import object\n");
        return false;
    }
    
    // Free existing host functions
    if (instance->host_functions) {
        kfree(instance->host_functions);
    }
    
    // Allocate new host functions array
    instance->host_functions = kmalloc(sizeof(wasm_host_function_t) * import_object->function_count);
    if (!instance->host_functions) {
        kprintf(ERROR, "Failed to allocate host functions array\n");
        return false;
    }
    
    // Copy host functions
    for (uint32_t i = 0; i < import_object->function_count; i++) {
        instance->host_functions[i] = import_object->functions[i].function;
    }
    
    instance->host_function_count = import_object->function_count;
    kprintf(INFO, "Set import object with %d host functions\n", import_object->function_count);
    return true;
} 