#include "wasm/wasm_parser.h"
#include "kernel/kprintf.h"
#include "kernel/mm/kmalloc.h"
#include <string.h>

// Helper function to read a LEB128-encoded unsigned integer
static bool read_leb128_u32(const uint8_t* bytes, size_t size, size_t* offset, uint32_t* value) {
    if (!bytes || !offset || !value || *offset >= size) {
        return false;
    }
    
    uint32_t result = 0;
    uint32_t shift = 0;
    
    while (true) {
        if (*offset >= size) {
            return false;
        }
        
        uint8_t byte = bytes[(*offset)++];
        result |= ((byte & 0x7F) << shift);
        
        if ((byte & 0x80) == 0) {
            break;
        }
        
        shift += 7;
    }
    
    *value = result;
    return true;
}

// Parse a function type
bool parse_functype(const uint8_t* bytes, size_t size, size_t* offset, wasm_functype_t* type) {
    if (!bytes || !offset || !type || *offset >= size) {
        return false;
    }
    
    // Check form (must be 0x60)
    if (bytes[(*offset)++] != 0x60) {
        kprintf(ERROR, "Invalid function type form\n");
        return false;
    }
    
    // Read parameter count
    uint32_t param_count;
    if (!read_leb128_u32(bytes, size, offset, &param_count)) {
        kprintf(ERROR, "Failed to read parameter count\n");
        return false;
    }
    
    // Allocate and read parameter types
    if (param_count > 0) {
        type->params = kmalloc(sizeof(wasm_value_type_t) * param_count);
        if (!type->params) {
            kprintf(ERROR, "Failed to allocate parameter types\n");
            return false;
        }
    } else {
        type->params = NULL;
    }
    
    for (uint32_t i = 0; i < param_count; i++) {
        if (*offset >= size) {
            kprintf(ERROR, "Unexpected end of data while reading parameter types\n");
            kfree(type->params);
            return false;
        }
        type->params[i] = (wasm_value_type_t)bytes[(*offset)++];
    }
    type->param_count = param_count;
    
    // Read result count
    uint32_t result_count;
    if (!read_leb128_u32(bytes, size, offset, &result_count)) {
        kprintf(ERROR, "Failed to read result count\n");
        kfree(type->params);
        return false;
    }
    
    // Allocate and read result types
    if (result_count > 0) {
        type->results = kmalloc(sizeof(wasm_value_type_t) * result_count);
        if (!type->results) {
            kprintf(ERROR, "Failed to allocate result types\n");
            kfree(type->params);
            return false;
        }
    } else {
        type->results = NULL;
    }
    
    // Read result types
    for (uint32_t i = 0; i < result_count; i++) {
        if (!type->results) {
            kprintf(ERROR, "Invalid result types array\n");
            return false;
        }
        type->results[i] = (wasm_value_type_t)bytes[(*offset)++];
    }
    type->result_count = result_count;
    
    return true;
}

// Parse the type section
bool wasm_parse_type_section(wasm_module_t* module, const uint8_t* bytes, size_t size, size_t* offset) {
    if (!bytes || !offset || !module || *offset >= size) {
        return false;
    }
    
    // Read number of types
    uint32_t type_count;
    if (!read_leb128_u32(bytes, size, offset, &type_count)) {
        kprintf(ERROR, "Failed to read type count\n");
        return false;
    }
    
    // Allocate type array
    module->types = kmalloc(sizeof(wasm_functype_t) * type_count);
    if (!module->types) {
        kprintf(ERROR, "Failed to allocate type array\n");
        return false;
    }
    module->type_count = type_count;
    
    // Parse each function type
    for (uint32_t i = 0; i < type_count; i++) {
        if (!parse_functype(bytes, size, offset, &module->types[i])) {
            // Cleanup on failure
            if (module->types[i].params) {
                kfree(module->types[i].params);
            }
            if (module->types[i].results) {
                kfree(module->types[i].results);
            }
            // Clean up previously allocated types
            for (uint32_t j = 0; j < i; j++) {
                kfree(module->types[j].params);
                kfree(module->types[j].results);
            }
            kfree(module->types);
            return false;
        }
    }
    
    return true;
}

// Parse the function section
bool wasm_parse_function_section(wasm_module_t* module, const uint8_t* bytes, size_t size, size_t* offset) {
    if (!bytes || !offset || !module || *offset >= size) {
        return false;
    }
    
    // Read number of functions
    uint32_t function_count;
    if (!read_leb128_u32(bytes, size, offset, &function_count)) {
        kprintf(ERROR, "Failed to read function count\n");
        return false;
    }
    
    // Allocate function array
    module->functions = kmalloc(sizeof(wasm_function_t) * function_count);
    if (!module->functions) {
        kprintf(ERROR, "Failed to allocate function array\n");
        return false;
    }
    
    // Initialize all functions to NULL/0
    memset(module->functions, 0, sizeof(wasm_function_t) * function_count);
    module->function_count = function_count;
    
    // Read type indices for each function
    for (uint32_t i = 0; i < function_count; i++) {
        uint32_t type_index;
        if (!read_leb128_u32(bytes, size, offset, &type_index)) {
            kprintf(ERROR, "Failed to read type index for function %d\n", i);
            kfree(module->functions);
            return false;
        }
        
        if (type_index >= module->type_count) {
            kprintf(ERROR, "Invalid type index %d for function %d\n", type_index, i);
            kfree(module->functions);
            return false;
        }
        
        module->functions[i].type = &module->types[type_index];
        module->functions[i].code = NULL;  // Will be set by code section
        module->functions[i].module = module;
        module->functions[i].local_count = 0;  // Will be set by code section
    }
    
    return true;
}

// Parse the code section
bool wasm_parse_code_section(wasm_module_t* module, const uint8_t* bytes, size_t size, size_t* offset) {
    if (!bytes || !offset || !module || *offset >= size) {
        kprintf(ERROR, "Invalid parameters for code section parsing\n");
        return false;
    }
    
    // Read number of functions
    uint32_t function_count;
    if (!read_leb128_u32(bytes, size, offset, &function_count)) {
        kprintf(ERROR, "Failed to read function count\n");
        return false;
    }
    
    if (function_count != module->function_count) {
        kprintf(ERROR, "Function count mismatch: expected %d, got %d\n",
                module->function_count, function_count);
        return false;
    }
    
    // Parse each function's code
    for (uint32_t i = 0; i < function_count; i++) {
        // Read function body size
        uint32_t body_size;
        if (!read_leb128_u32(bytes, size, offset, &body_size)) {
            kprintf(ERROR, "Failed to read function body size for function %d\n", i);
            return false;
        }
        // Validate body size
        if (*offset + body_size > size) {
            kprintf(ERROR, "Function body size %d exceeds section bounds for function %d\n", body_size, i);
            return false;
        }
        // Save the offset at the start of the function body
        size_t body_start_offset = *offset;
        // Read local variable declarations
        uint32_t local_count = 0;
        uint32_t local_decl_count;
        if (!read_leb128_u32(bytes, size, offset, &local_decl_count)) {
            kprintf(ERROR, "Failed to read local declaration count for function %d\n", i);
            return false;
        }
        kprintf(DEBUG, "Function %d has %d local declarations\n", i, local_decl_count);
        for (uint32_t j = 0; j < local_decl_count; j++) {
            uint32_t count;
            if (!read_leb128_u32(bytes, size, offset, &count)) {
                kprintf(ERROR, "Failed to read local count for declaration %d in function %d\n", j, i);
                return false;
            }
            if (*offset >= size) {
                kprintf(ERROR, "Unexpected end of data while reading local type\n");
                return false;
            }
            uint8_t type = bytes[(*offset)++];
            kprintf(DEBUG, "Local declaration %d: count=%d, type=0x%02x\n", j, count, type);
            if (type == 0x7F || type == 0x7E || type == 0x7D || type == 0x7C) {
                local_count += count;
            } else {
                kprintf(ERROR, "Unsupported local type: 0x%x (expected 0x7F for i32)\n", type);
                return false;
            }
        }
        module->functions[i].local_count = module->functions[i].type->param_count + local_count;
        // Calculate code size: only the bytes after local decls up to end of body
        size_t locals_end_offset = *offset;
        size_t code_size = body_size - (locals_end_offset - body_start_offset);
        if (*offset + code_size > size) {
            kprintf(ERROR, "Function code overruns section bounds for function %d\n", i);
            return false;
        }
        uint8_t* code = kmalloc(code_size);
        if (!code) {
            kprintf(ERROR, "Failed to allocate %d bytes for function %d code\n", code_size, i);
            return false;
        }
        memcpy(code, bytes + *offset, code_size);
        module->functions[i].code = code;
        module->functions[i].code_size = code_size;  // Store the code size
        *offset += code_size;
        kprintf(INFO, "Successfully loaded function %d with %d locals and %d bytes of code\n", 
                i, module->functions[i].local_count, code_size);
    }
    
    return true;
}

// Parse the export section
bool wasm_parse_export_section(wasm_module_t* module, const uint8_t* bytes, size_t size, size_t* offset) {
    if (!bytes || !offset || !module || *offset >= size) {
        return false;
    }
    
    // Read number of exports
    uint32_t export_count;
    if (!read_leb128_u32(bytes, size, offset, &export_count)) {
        kprintf(ERROR, "Failed to read export count\n");
        return false;
    }
    
    // Allocate export array
    module->exports = kmalloc(sizeof(wasm_export_t) * export_count);
    if (!module->exports) {
        kprintf(ERROR, "Failed to allocate export array\n");
        return false;
    }
    module->export_count = export_count;
    
    // Parse each export
    for (uint32_t i = 0; i < export_count; i++) {
        // Read name length
        uint32_t name_length;
        if (!read_leb128_u32(bytes, size, offset, &name_length)) {
            kprintf(ERROR, "Failed to read export name length\n");
            kfree(module->exports);
            return false;
        }
        
        // Read name
        if (*offset + name_length > size) {
            kprintf(ERROR, "Export name exceeds section bounds\n");
            kfree(module->exports);
            return false;
        }
        
        module->exports[i].name = kmalloc(name_length + 1);
        if (!module->exports[i].name) {
            kprintf(ERROR, "Failed to allocate export name\n");
            kfree(module->exports);
            return false;
        }
        
        memcpy(module->exports[i].name, bytes + *offset, name_length);
        module->exports[i].name[name_length] = '\0';
        *offset += name_length;
        
        // Read export kind
        if (*offset >= size) {
            kprintf(ERROR, "Unexpected end of data while reading export kind\n");
            kfree(module->exports[i].name);
            kfree(module->exports);
            return false;
        }
        module->exports[i].kind = bytes[(*offset)++];
        
        // Read export index
        uint32_t export_index;
        if (!read_leb128_u32(bytes, size, offset, &export_index)) {
            kprintf(ERROR, "Failed to read export index\n");
            kfree(module->exports[i].name);
            kfree(module->exports);
            return false;
        }
        module->exports[i].index = export_index;
    }
    
    return true;
}

// Parse the memory section
bool wasm_parse_memory_section(wasm_module_t* module, const uint8_t* bytes, size_t size, size_t* offset) {
    if (!bytes || !offset || !module || *offset >= size) {
        return false;
    }
    
    // Read number of memory entries
    uint32_t memory_count;
    if (!read_leb128_u32(bytes, size, offset, &memory_count)) {
        kprintf(ERROR, "Failed to read memory count\n");
        return false;
    }
    
    if (memory_count > 1) {
        kprintf(ERROR, "Multiple memory sections not supported\n");
        return false;
    }
    
    if (memory_count == 1) {
        // Read memory type (limits)
        if (*offset >= size) {
            kprintf(ERROR, "Unexpected end of data while reading memory type\n");
            return false;
        }
        
        uint8_t flags = bytes[(*offset)++];
        bool has_max = (flags & 0x01) != 0;
        
        // Read initial size
        uint32_t initial_size;
        if (!read_leb128_u32(bytes, size, offset, &initial_size)) {
            kprintf(ERROR, "Failed to read initial memory size\n");
            return false;
        }
        
        // Read maximum size if present
        uint32_t max_size = 0;
        if (has_max) {
            if (!read_leb128_u32(bytes, size, offset, &max_size)) {
                kprintf(ERROR, "Failed to read maximum memory size\n");
                return false;
            }
        }
        
        module->memory_initial = initial_size;
        module->memory_max = has_max ? max_size : 0;
    }
    
    return true;
}

// Parse the import section
bool wasm_parse_import_section(wasm_module_t* module, const uint8_t* bytes, size_t size, size_t* offset) {
    if (!bytes || !offset || !module || *offset >= size) {
        return false;
    }
    
    // Read number of imports
    uint32_t import_count;
    if (!read_leb128_u32(bytes, size, offset, &import_count)) {
        kprintf(ERROR, "Failed to read import count\n");
        return false;
    }
    
    // Allocate import array
    module->imports = kmalloc(sizeof(wasm_import_t) * import_count);
    if (!module->imports) {
        kprintf(ERROR, "Failed to allocate import array\n");
        return false;
    }
    module->import_count = import_count;
    
    // Parse each import
    for (uint32_t i = 0; i < import_count; i++) {
        // Read module name length
        uint32_t module_name_len;
        if (!read_leb128_u32(bytes, size, offset, &module_name_len)) {
            kprintf(ERROR, "Failed to read module name length for import %d\n", i);
            kfree(module->imports);
            return false;
        }
        
        // Read module name
        if (*offset + module_name_len > size) {
            kprintf(ERROR, "Module name exceeds section bounds for import %d\n", i);
            kfree(module->imports);
            return false;
        }
        
        module->imports[i].module_name = kmalloc(module_name_len + 1);
        if (!module->imports[i].module_name) {
            kprintf(ERROR, "Failed to allocate module name for import %d\n", i);
            kfree(module->imports);
            return false;
        }
        
        memcpy(module->imports[i].module_name, bytes + *offset, module_name_len);
        module->imports[i].module_name[module_name_len] = '\0';
        *offset += module_name_len;
        
        // Read field name length
        uint32_t field_name_len;
        if (!read_leb128_u32(bytes, size, offset, &field_name_len)) {
            kprintf(ERROR, "Failed to read field name length for import %d\n", i);
            kfree(module->imports[i].module_name);
            kfree(module->imports);
            return false;
        }
        
        // Read field name
        if (*offset + field_name_len > size) {
            kprintf(ERROR, "Field name exceeds section bounds for import %d\n", i);
            kfree(module->imports[i].module_name);
            kfree(module->imports);
            return false;
        }
        
        module->imports[i].field_name = kmalloc(field_name_len + 1);
        if (!module->imports[i].field_name) {
            kprintf(ERROR, "Failed to allocate field name for import %d\n", i);
            kfree(module->imports[i].module_name);
            kfree(module->imports);
            return false;
        }
        
        memcpy(module->imports[i].field_name, bytes + *offset, field_name_len);
        module->imports[i].field_name[field_name_len] = '\0';
        *offset += field_name_len;
        
        // Read import kind
        if (*offset >= size) {
            kprintf(ERROR, "Unexpected end of data while reading import kind\n");
            kfree(module->imports[i].field_name);
            kfree(module->imports[i].module_name);
            kfree(module->imports);
            return false;
        }
        module->imports[i].kind = bytes[(*offset)++];
        
        // For functions, read the type index
        if (module->imports[i].kind == 0) { // Function import
            uint32_t type_index;
            if (!read_leb128_u32(bytes, size, offset, &type_index)) {
                kprintf(ERROR, "Failed to read type index for import %d\n", i);
                kfree(module->imports[i].field_name);
                kfree(module->imports[i].module_name);
                kfree(module->imports);
                return false;
            }
            module->imports[i].type_index = type_index;
        }
    }
    
    return true;
}

// Parse a WebAssembly module
bool wasm_parse_module(wasm_module_t* module) {
    if (!module || !module->bytes || module->size < 8) {
        kprintf(ERROR, "Invalid module data\n");
        return false;
    }
    
    size_t offset = 8;  // Skip magic number and version
    
    while (offset < module->size) {
        if (offset + 1 > module->size) {
            kprintf(ERROR, "Unexpected end of data while reading section header\n");
            return false;
        }
        
        uint8_t section_id = module->bytes[offset++];
        uint32_t section_size;
        if (!read_leb128_u32(module->bytes, module->size, &offset, &section_size)) {
            kprintf(ERROR, "Failed to read section size for section %d\n", section_id);
            return false;
        }
        
        if (offset + section_size > module->size) {
            kprintf(ERROR, "Section %d size %d exceeds module bounds\n", section_id, section_size);
            return false;
        }
        
        bool success = false;
        switch (section_id) {
            case WASM_SECTION_TYPE:
                success = wasm_parse_type_section(module, module->bytes, module->size, &offset);
                break;
            case WASM_SECTION_IMPORT:
                success = wasm_parse_import_section(module, module->bytes, module->size, &offset);
                break;
            case WASM_SECTION_FUNCTION:
                success = wasm_parse_function_section(module, module->bytes, module->size, &offset);
                break;
            case WASM_SECTION_CODE:
                success = wasm_parse_code_section(module, module->bytes, module->size, &offset);
                break;
            case WASM_SECTION_EXPORT:
                success = wasm_parse_export_section(module, module->bytes, module->size, &offset);
                break;
            case WASM_SECTION_MEMORY:
                success = wasm_parse_memory_section(module, module->bytes, module->size, &offset);
                break;
            default:
                // Skip unknown sections
                offset += section_size;
                success = true;
                break;
        }
        
        if (!success) {
            kprintf(ERROR, "Failed to parse section %d\n", section_id);
            return false;
        }
    }
    
    return true;
}

// Implement wasm_parse_section_header
bool wasm_parse_section_header(const uint8_t* bytes, size_t size, size_t* offset, wasm_section_header_t* header) {
    if (!bytes || !offset || !header || *offset >= size) {
        return false;
    }
    uint8_t section_id = bytes[(*offset)++];
    uint32_t section_size = 0;
    if (!read_leb128_u32(bytes, size, offset, &section_size)) {
        return false;
    }
    header->id = (wasm_section_id_t)section_id;
    header->size = section_size;
    header->offset = *offset;
    return true;
}

// Test function to check WASM parsing correctness
void wasm_parser_test(const uint8_t* wasm_bytes, size_t wasm_size) {
    wasm_module_t module = {0};
    module.bytes = (uint8_t*)wasm_bytes;
    module.size = wasm_size;

    if (!wasm_parse_module(&module)) {
        kprintf(ERROR, "WASM parse failed\n");
        return;
    }

    kprintf(INFO, "[TEST] Parsed %d types, %d functions\n", module.type_count, module.function_count);
    for (uint32_t i = 0; i < module.type_count; i++) {
        kprintf(INFO, "[TEST] Type %d: param_count=%d, result_count=%d\n", i, module.types[i].param_count, module.types[i].result_count);
    }
    for (uint32_t i = 0; i < module.function_count; i++) {
        kprintf(INFO, "[TEST] Function %d: param_count=%d, local_count=%d, code_size=%u\n",
            i,
            module.functions[i].type ? module.functions[i].type->param_count : 0,
            module.functions[i].local_count,
            (unsigned int)module.functions[i].code_size);  // Use stored code size
        // Print first up to 16 code bytes (or less)
        if (module.functions[i].code) {
            uint8_t* code_bytes = (uint8_t*)module.functions[i].code;
            char buf[128] = {0};
            char* p = buf;
            for (size_t j = 0; j < module.functions[i].code_size && j < 16; j++) {  // Use stored code size
                uint8_t byte = code_bytes[j];
                // Write two hex digits
                *p++ = "0123456789abcdef"[(byte >> 4) & 0xF];
                *p++ = "0123456789abcdef"[byte & 0xF];
                *p++ = ' ';
            }
            *p = '\0';
            kprintf(INFO, "[TEST] Code bytes: %s\n", buf);
        }
    }
} 