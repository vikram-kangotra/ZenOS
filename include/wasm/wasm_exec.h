#pragma once

#include "wasm/wasm.h"
#include <stdint.h>
#include <stdbool.h>

// WebAssembly opcodes
typedef enum {
    // Control instructions
    WASM_OP_UNREACHABLE = 0x00,
    WASM_OP_NOP = 0x01,
    WASM_OP_BLOCK = 0x02,
    WASM_OP_LOOP = 0x03,
    WASM_OP_IF = 0x04,
    WASM_OP_ELSE = 0x05,
    WASM_OP_END = 0x0B,
    WASM_OP_BR = 0x0C,
    WASM_OP_BR_IF = 0x0D,
    WASM_OP_RETURN = 0x0F,
    WASM_OP_CALL = 0x10,
    
    // Parametric instructions
    WASM_OP_DROP = 0x1A,
    WASM_OP_SELECT = 0x1B,
    
    // Variable instructions
    WASM_OP_LOCAL_GET = 0x20,
    WASM_OP_LOCAL_SET = 0x21,
    WASM_OP_LOCAL_TEE = 0x22,
    WASM_OP_GLOBAL_GET = 0x23,
    WASM_OP_GLOBAL_SET = 0x24,
    
    // Memory instructions
    WASM_OP_I32_LOAD = 0x28,
    WASM_OP_I64_LOAD = 0x29,
    WASM_OP_F32_LOAD = 0x2A,
    WASM_OP_F64_LOAD = 0x2B,
    WASM_OP_I32_STORE = 0x36,
    WASM_OP_I64_STORE = 0x37,
    WASM_OP_F32_STORE = 0x38,
    WASM_OP_F64_STORE = 0x39,
    
    // Numeric instructions
    WASM_OP_I32_CONST = 0x41,
    WASM_OP_I64_CONST = 0x42,
    WASM_OP_F32_CONST = 0x43,
    WASM_OP_F64_CONST = 0x44,
    
    // Integer comparison
    WASM_OP_I32_EQZ = 0x45,
    WASM_OP_I32_EQ = 0x46,
    WASM_OP_I32_NE = 0x47,
    WASM_OP_I32_LT_S = 0x48,
    WASM_OP_I32_LT_U = 0x49,
    WASM_OP_I32_GT_S = 0x4A,
    WASM_OP_I32_GT_U = 0x4B,
    WASM_OP_I32_LE_S = 0x4C,
    
    // Integer arithmetic
    WASM_OP_I32_ADD = 0x6A,
    WASM_OP_I32_SUB = 0x6B,
    WASM_OP_I32_MUL = 0x6C,
    WASM_OP_I32_DIV_S = 0x6D,
    WASM_OP_I32_DIV_U = 0x6E,
    WASM_OP_I32_REM_S = 0x6F,
    WASM_OP_I32_REM_U = 0x70,
    WASM_OP_I32_AND = 0x71,
    WASM_OP_I32_XOR = 0x72,
    WASM_OP_I32_OR = 0x73,
} wasm_opcode_t;

// Control flow block
typedef struct {
    const uint8_t* start_pc;
    const uint8_t* end_pc;
    uint32_t stack_size;
    uint8_t block_type;
    bool is_loop;
} wasm_block_t;

// WebAssembly execution context
typedef struct {
    wasm_instance_t* instance;
    uint32_t* locals;
    uint32_t local_count;
    wasm_value_t* stack;
    uint32_t stack_size;
    uint32_t stack_capacity;
    const uint8_t* pc;
    wasm_block_t* block_stack;
    uint32_t block_stack_size;
    uint32_t block_stack_capacity;
} wasm_exec_context_t;

// Function declarations
bool wasm_execute_function(wasm_function_t* function, wasm_value_t* args, uint32_t arg_count, wasm_value_t* result);
bool wasm_execute_instruction(wasm_exec_context_t* ctx);
void wasm_exec_context_init(wasm_exec_context_t* ctx, wasm_instance_t* instance, uint32_t local_count);
void wasm_exec_context_cleanup(wasm_exec_context_t* ctx); 