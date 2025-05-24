#pragma once

#include "wasm.h"

// WebAssembly section IDs
typedef enum {
    WASM_SECTION_CUSTOM = 0,
    WASM_SECTION_TYPE = 1,
    WASM_SECTION_IMPORT = 2,
    WASM_SECTION_FUNCTION = 3,
    WASM_SECTION_TABLE = 4,
    WASM_SECTION_MEMORY = 5,
    WASM_SECTION_GLOBAL = 6,
    WASM_SECTION_EXPORT = 7,
    WASM_SECTION_START = 8,
    WASM_SECTION_ELEMENT = 9,
    WASM_SECTION_CODE = 10,
    WASM_SECTION_DATA = 11,
    WASM_SECTION_DATACOUNT = 12
} wasm_section_id_t;

// WebAssembly section header
typedef struct {
    wasm_section_id_t id;
    uint32_t size;
    uint32_t offset;
} wasm_section_header_t;

// Function declarations
bool wasm_parse_module(wasm_module_t* module);
bool wasm_parse_section_header(const uint8_t* bytes, size_t size, size_t* offset, wasm_section_header_t* header);
bool wasm_parse_type_section(wasm_module_t* module, const uint8_t* bytes, size_t size, size_t* offset);
bool wasm_parse_function_section(wasm_module_t* module, const uint8_t* bytes, size_t size, size_t* offset);
bool wasm_parse_code_section(wasm_module_t* module, const uint8_t* bytes, size_t size, size_t* offset);
bool wasm_parse_export_section(wasm_module_t* module, const uint8_t* bytes, size_t size, size_t* offset);
bool wasm_parse_memory_section(wasm_module_t* module, const uint8_t* bytes, size_t size, size_t* offset);

// Parse a WebAssembly module from binary data
wasm_module_t* wasm_module_new(const uint8_t* bytes, size_t size);

// Delete a WebAssembly module
void wasm_module_delete(wasm_module_t* module);

// Parse a function type
bool parse_functype(const uint8_t* bytes, size_t size, size_t* offset, wasm_functype_t* type);

void wasm_parser_test(const uint8_t* wasm_bytes, size_t wasm_size); 