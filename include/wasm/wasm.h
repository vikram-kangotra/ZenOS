#pragma once

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct wasm_instance wasm_instance_t;
typedef struct wasm_import_object wasm_import_object_t;

// WebAssembly value types
typedef enum {
    WASM_I32 = 0x7F,
    WASM_I64 = 0x7E,
    WASM_F32 = 0x7D,
    WASM_F64 = 0x7C,
    WASM_V128 = 0x7B,
    WASM_FUNCREF = 0x70,
    WASM_EXTERNREF = 0x6F
} wasm_value_type_t;

// WebAssembly value union
typedef union {
    int32_t i32;
    int64_t i64;
    float f32;
    double f64;
    void* ref;
} wasm_value_t;

// WebAssembly function type
typedef struct {
    wasm_value_type_t* params;
    uint32_t param_count;
    wasm_value_type_t* results;
    uint32_t result_count;
} wasm_functype_t;

// WebAssembly export
typedef struct {
    char* name;
    uint8_t kind;  // 0 = function, 1 = table, 2 = memory, 3 = global
    uint32_t index;
} wasm_export_t;

// WebAssembly import
typedef struct {
    char* module_name;
    char* field_name;
    uint8_t kind;  // 0 = function, 1 = table, 2 = memory, 3 = global
    uint32_t type_index;  // For function imports, this is the type index
} wasm_import_t;

// WebAssembly function
typedef struct {
    wasm_functype_t* type;
    void* code;
    size_t code_size;  // Size of the function's code in bytes
    void* module;
    uint32_t local_count;  // Total number of locals (parameters + local variables)
} wasm_function_t;

// WebAssembly global variable
typedef struct {
    wasm_value_t value;
    bool mut;  // true for mutable, false for immutable
} wasm_global_t;

// Host function callback type
typedef bool (*wasm_host_function_t)(wasm_instance_t* instance, wasm_value_t* args, uint32_t arg_count, wasm_value_t* result);

// Host function definition
typedef struct {
    char* module_name;
    char* field_name;
    wasm_functype_t type;
    wasm_host_function_t function;
} wasm_host_function_def_t;

// Import object structure
struct wasm_import_object {
    wasm_host_function_def_t* functions;
    uint32_t function_count;
    uint32_t function_capacity;
};

// WebAssembly module
typedef struct {
    uint8_t* bytes;
    size_t size;
    wasm_functype_t* types;
    uint32_t type_count;
    wasm_import_t* imports;
    uint32_t import_count;
    wasm_function_t* functions;
    uint32_t function_count;
    wasm_export_t* exports;
    uint32_t export_count;
    uint32_t memory_initial;
    uint32_t memory_max;
    wasm_global_t* globals;
    uint32_t global_count;
} wasm_module_t;

// WebAssembly instance
struct wasm_instance {
    wasm_module_t* module;
    void* memory;
    size_t memory_size;
    wasm_function_t* functions;
    uint32_t function_count;
    wasm_global_t* globals;
    uint32_t global_count;
    wasm_host_function_t* host_functions;
    uint32_t host_function_count;
    bool should_exit;
};

// Function declarations
wasm_module_t* wasm_module_new(const uint8_t* bytes, size_t size);
void wasm_module_delete(wasm_module_t* module);
wasm_instance_t* wasm_instance_new(wasm_module_t* module);
void wasm_instance_delete(wasm_instance_t* instance);
bool wasm_function_call(wasm_function_t* function, wasm_value_t* args, uint32_t arg_count, wasm_value_t* result);

// Import object functions
wasm_import_object_t* wasm_import_object_new(void);
void wasm_import_object_delete(wasm_import_object_t* import_object);
bool wasm_import_object_add_function(wasm_import_object_t* import_object, const char* module_name, 
                                   const char* field_name, wasm_functype_t type, wasm_host_function_t function);
bool wasm_instance_set_import_object(wasm_instance_t* instance, wasm_import_object_t* import_object);

// Host function registration
bool wasm_register_host_function(wasm_instance_t* instance, const char* module_name, const char* field_name, 
                               wasm_functype_t type, wasm_host_function_t function); 