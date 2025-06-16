#include "wasm/wasm_exec.h"
#include "kernel/mm/kmalloc.h"
#include "kernel/kprintf.h"
#include <string.h>
#include <stdint.h>

#define INT32_MIN (-2147483647 - 1)

// Add function declaration at the top and other static function declarations
static bool execute_i32_le_s(wasm_exec_context_t* ctx);

// Initialize execution context
void wasm_exec_context_init(wasm_exec_context_t* ctx, wasm_instance_t* instance, uint32_t local_count) {
    if (!ctx || !instance) {
        kprintf(ERROR, "Invalid parameters in wasm_exec_context_init\n");
        return;
    }
    
    // Zero out the context first
    memset(ctx, 0, sizeof(wasm_exec_context_t));
    kprintf(DEBUG, "[WASM] Initializing execution context: local_count=%u\n", local_count);

    ctx->instance = instance;
    ctx->local_count = local_count;
    if (local_count > 0) {
        ctx->locals = kmalloc(sizeof(uint32_t) * local_count);
        if (!ctx->locals) {
            kprintf(ERROR, "Failed to allocate locals array\n");
            return;
        }
        // Initialize locals to 0
        memset(ctx->locals, 0, sizeof(uint32_t) * local_count);
    } else {
        ctx->locals = NULL;
    }
    
    ctx->stack_capacity = 1024;  // Initial stack size
    ctx->stack = kmalloc(sizeof(wasm_value_t) * ctx->stack_capacity);
    if (!ctx->stack) {
        kprintf(ERROR, "Failed to allocate stack\n");
        kfree(ctx->locals);
        return;
    }
    ctx->stack_size = 0;
    ctx->pc = NULL;
    
    // Initialize control flow stack
    ctx->block_stack_capacity = 32;
    ctx->block_stack = kmalloc(sizeof(wasm_block_t) * ctx->block_stack_capacity);
    if (!ctx->block_stack) {
        kprintf(ERROR, "Failed to allocate block stack\n");
        kfree(ctx->locals);
        kfree(ctx->stack);
        return;
    }
    ctx->block_stack_size = 0;
    
    kprintf(DEBUG, "[WASM] Execution context initialized: stack_capacity=%u, block_stack_capacity=%u\n",
            ctx->stack_capacity, ctx->block_stack_capacity);
}

// Clean up execution context
void wasm_exec_context_cleanup(wasm_exec_context_t* ctx) {
    if (!ctx) return;
    
    if (ctx->locals) {
        kfree(ctx->locals);
    }
    
    if (ctx->stack) {
        kfree(ctx->stack);
    }
    
    if (ctx->block_stack) {
        kfree(ctx->block_stack);
    }
}

// Push a value onto the stack
static bool stack_push(wasm_exec_context_t* ctx, wasm_value_t value) {
    if (!ctx || !ctx->stack) {
        kprintf(ERROR, "Invalid stack context in stack_push\n");
        return false;
    }
    
    if (ctx->stack_size >= ctx->stack_capacity) {
        // Grow stack
        uint32_t new_capacity = ctx->stack_capacity * 2;
        wasm_value_t* new_stack = kmalloc(sizeof(wasm_value_t) * new_capacity);
        if (!new_stack) {
            kprintf(ERROR, "Failed to grow WebAssembly stack\n");
            return false;
        }
        
        memcpy(new_stack, ctx->stack, sizeof(wasm_value_t) * ctx->stack_size);
        kfree(ctx->stack);
        ctx->stack = new_stack;
        ctx->stack_capacity = new_capacity;
    }
    
    ctx->stack[ctx->stack_size++] = value;
    kprintf(DEBUG, "[WASM] Stack push: size=%u, value=%d\n", ctx->stack_size, value.i32);
    return true;
}

// Pop a value from the stack
static bool stack_pop(wasm_exec_context_t* ctx, wasm_value_t* value) {
    if (!ctx || !ctx->stack || !value) {
        kprintf(ERROR, "Invalid parameters in stack_pop\n");
        return false;
    }
    
    if (ctx->stack_size == 0) {
        kprintf(ERROR, "Stack underflow in stack_pop\n");
        return false;
    }
    
    *value = ctx->stack[--ctx->stack_size];
    kprintf(DEBUG, "[WASM] Stack pop: size=%u, value=%d\n", ctx->stack_size, value->i32);
    return true;
}

// Push a block onto the control flow stack
static bool block_stack_push(wasm_exec_context_t* ctx, wasm_block_t block) {
    if (!ctx || !ctx->block_stack) return false;
    
    if (ctx->block_stack_size >= ctx->block_stack_capacity) {
        // Grow block stack
        uint32_t new_capacity = ctx->block_stack_capacity * 2;
        wasm_block_t* new_stack = kmalloc(sizeof(wasm_block_t) * new_capacity);
        if (!new_stack) {
            kprintf(ERROR, "Failed to grow WebAssembly block stack\n");
            return false;
        }
        
        memcpy(new_stack, ctx->block_stack, sizeof(wasm_block_t) * ctx->block_stack_size);
        kfree(ctx->block_stack);
        ctx->block_stack = new_stack;
        ctx->block_stack_capacity = new_capacity;
    }
    
    ctx->block_stack[ctx->block_stack_size++] = block;
    return true;
}

// Pop a block from the control flow stack
static bool block_stack_pop(wasm_exec_context_t* ctx, wasm_block_t* block) {
    if (!ctx || !ctx->block_stack || !block || ctx->block_stack_size == 0) {
        return false;
    }
    
    *block = ctx->block_stack[--ctx->block_stack_size];
    return true;
}

// Read a block type
static bool read_block_type(const uint8_t* bytes, uint8_t* block_type) {
    if (!bytes || !block_type) {
        return false;
    }
    
    uint8_t type = *bytes;
    
    // Handle value types (0x7F = i32, 0x7E = i64, etc.)
    if (type == 0x7F || type == 0x7E || type == 0x7D || type == 0x7C) {
        *block_type = type;
        return true;
    }
    
    // Handle empty block type (0x40)
    if (type == 0x40) {
        *block_type = 0;
        return true;
    }
    
    // Handle type index (0x00-0x3F)
    if (type <= 0x3F) {
        *block_type = type;
        return true;
    }
    
    kprintf(ERROR, "Unsupported block type: 0x%02X\n", type);
    return false;
}

// Helper functions for LEB128 encoding
static uint32_t read_uleb128(const uint8_t** pc) {
    uint32_t result = 0;
    uint32_t shift = 0;
    while (true) {
        uint8_t byte = **pc;
        (*pc)++;
        result |= ((byte & 0x7f) << shift);
        if ((byte & 0x80) == 0) break;
        shift += 7;
    }
    return result;
}

static int32_t read_sleb128(const uint8_t** pc) {
    int32_t result = 0;
    uint32_t shift = 0;
    while (true) {
        uint8_t byte = **pc;
        (*pc)++;
        result |= ((byte & 0x7f) << shift);
        if ((byte & 0x80) == 0) {
            if (shift < 32 && (byte & 0x40)) {
                result |= -(1 << shift);
            }
            break;
        }
        shift += 7;
    }
    return result;
}

// Memory load operations
static bool execute_i32_load(wasm_exec_context_t* ctx, const uint8_t** pc) {
    uint32_t offset = read_uleb128(pc);
    uint32_t addr = ctx->stack[ctx->stack_size - 1].i32 + offset;
    
    if (addr + sizeof(uint32_t) > ctx->instance->memory_size) {
        kprintf(ERROR, "Memory access out of bounds\n");
        return false;
    }
    
    uint32_t value;
    memcpy(&value, (uint8_t*)ctx->instance->memory + addr, sizeof(uint32_t));
    ctx->stack[ctx->stack_size - 1].i32 = value;
    return true;
}

static bool execute_i64_load(wasm_exec_context_t* ctx, const uint8_t** pc) {
    uint32_t offset = read_uleb128(pc);
    uint32_t addr = ctx->stack[ctx->stack_size - 1].i32 + offset;
    
    if (addr + sizeof(uint64_t) > ctx->instance->memory_size) {
        kprintf(ERROR, "Memory access out of bounds\n");
        return false;
    }
    
    uint64_t value;
    memcpy(&value, (uint8_t*)ctx->instance->memory + addr, sizeof(uint64_t));
    ctx->stack[ctx->stack_size - 1].i64 = value;
    return true;
}

static bool execute_f32_load(wasm_exec_context_t* ctx, const uint8_t** pc) {
    uint32_t offset = read_uleb128(pc);
    uint32_t addr = ctx->stack[ctx->stack_size - 1].i32 + offset;
    
    if (addr + sizeof(float) > ctx->instance->memory_size) {
        kprintf(ERROR, "Memory access out of bounds\n");
        return false;
    }
    
    float value;
    memcpy(&value, (uint8_t*)ctx->instance->memory + addr, sizeof(float));
    ctx->stack[ctx->stack_size - 1].f32 = value;
    return true;
}

static bool execute_f64_load(wasm_exec_context_t* ctx, const uint8_t** pc) {
    uint32_t offset = read_uleb128(pc);
    uint32_t addr = ctx->stack[ctx->stack_size - 1].i32 + offset;
    
    if (addr + sizeof(double) > ctx->instance->memory_size) {
        kprintf(ERROR, "Memory access out of bounds\n");
        return false;
    }
    
    double value;
    memcpy(&value, (uint8_t*)ctx->instance->memory + addr, sizeof(double));
    ctx->stack[ctx->stack_size - 1].f64 = value;
    return true;
}

// Memory store operations
static bool execute_i32_store(wasm_exec_context_t* ctx, const uint8_t** pc) {
    uint32_t offset = read_uleb128(pc);
    uint32_t addr = ctx->stack[ctx->stack_size - 2].i32 + offset;
    uint32_t value = ctx->stack[ctx->stack_size - 1].i32;
    
    if (addr + sizeof(uint32_t) > ctx->instance->memory_size) {
        kprintf(ERROR, "Memory access out of bounds\n");
        return false;
    }
    
    memcpy((uint8_t*)ctx->instance->memory + addr, &value, sizeof(uint32_t));
    ctx->stack_size -= 2;
    return true;
}

static bool execute_i64_store(wasm_exec_context_t* ctx, const uint8_t** pc) {
    uint32_t offset = read_uleb128(pc);
    uint32_t addr = ctx->stack[ctx->stack_size - 2].i32 + offset;
    uint64_t value = ctx->stack[ctx->stack_size - 1].i64;
    
    if (addr + sizeof(uint64_t) > ctx->instance->memory_size) {
        kprintf(ERROR, "Memory access out of bounds\n");
        return false;
    }
    
    memcpy((uint8_t*)ctx->instance->memory + addr, &value, sizeof(uint64_t));
    ctx->stack_size -= 2;
    return true;
}

static bool execute_f32_store(wasm_exec_context_t* ctx, const uint8_t** pc) {
    uint32_t offset = read_uleb128(pc);
    uint32_t addr = ctx->stack[ctx->stack_size - 2].i32 + offset;
    float value = ctx->stack[ctx->stack_size - 1].f32;
    
    if (addr + sizeof(float) > ctx->instance->memory_size) {
        kprintf(ERROR, "Memory access out of bounds\n");
        return false;
    }
    
    memcpy((uint8_t*)ctx->instance->memory + addr, &value, sizeof(float));
    ctx->stack_size -= 2;
    return true;
}

static bool execute_f64_store(wasm_exec_context_t* ctx, const uint8_t** pc) {
    uint32_t offset = read_uleb128(pc);
    uint32_t addr = ctx->stack[ctx->stack_size - 2].i32 + offset;
    double value = ctx->stack[ctx->stack_size - 1].f64;
    
    if (addr + sizeof(double) > ctx->instance->memory_size) {
        kprintf(ERROR, "Memory access out of bounds\n");
        return false;
    }
    
    memcpy((uint8_t*)ctx->instance->memory + addr, &value, sizeof(double));
    ctx->stack_size -= 2;
    return true;
}

static bool execute_i64_const(wasm_exec_context_t* ctx) {
    int64_t value = read_sleb128(&ctx->pc);
    ctx->stack[ctx->stack_size].i64 = value;
    ctx->stack_size++;
    return true;
}

static bool execute_f32_const(wasm_exec_context_t* ctx) {
    float value;
    memcpy(&value, ctx->pc, 4);
    ctx->pc += 4;
    ctx->stack[ctx->stack_size].f32 = value;
    ctx->stack_size++;
    return true;
}

static bool execute_f64_const(wasm_exec_context_t* ctx) {
    double value;
    memcpy(&value, ctx->pc, 8);
    ctx->pc += 8;
    ctx->stack[ctx->stack_size].f64 = value;
    ctx->stack_size++;
    return true;
}

// Integer comparison operations
static bool execute_i32_eq(wasm_exec_context_t* ctx) {
    int32_t b = ctx->stack[ctx->stack_size - 1].i32;
    int32_t a = ctx->stack[ctx->stack_size - 2].i32;
    ctx->stack[ctx->stack_size - 2].i32 = (a == b) ? 1 : 0;
    ctx->stack_size--;
    return true;
}

static bool execute_i32_ne(wasm_exec_context_t* ctx) {
    int32_t b = ctx->stack[ctx->stack_size - 1].i32;
    int32_t a = ctx->stack[ctx->stack_size - 2].i32;
    ctx->stack[ctx->stack_size - 2].i32 = (a != b) ? 1 : 0;
    ctx->stack_size--;
    return true;
}

static bool execute_i32_lt_s(wasm_exec_context_t* ctx) {
    int32_t b = ctx->stack[ctx->stack_size - 1].i32;
    int32_t a = ctx->stack[ctx->stack_size - 2].i32;
    ctx->stack[ctx->stack_size - 2].i32 = (a < b) ? 1 : 0;
    ctx->stack_size--;
    return true;
}

static bool execute_i32_lt_u(wasm_exec_context_t* ctx) {
    uint32_t b = (uint32_t)ctx->stack[ctx->stack_size - 1].i32;
    uint32_t a = (uint32_t)ctx->stack[ctx->stack_size - 2].i32;
    ctx->stack[ctx->stack_size - 2].i32 = (a < b) ? 1 : 0;
    ctx->stack_size--;
    return true;
}

static bool execute_i32_gt_s(wasm_exec_context_t* ctx) {
    int32_t b = ctx->stack[ctx->stack_size - 1].i32;
    int32_t a = ctx->stack[ctx->stack_size - 2].i32;
    ctx->stack[ctx->stack_size - 2].i32 = (a > b) ? 1 : 0;
    ctx->stack_size--;
    return true;
}

static bool execute_i32_gt_u(wasm_exec_context_t* ctx) {
    uint32_t b = (uint32_t)ctx->stack[ctx->stack_size - 1].i32;
    uint32_t a = (uint32_t)ctx->stack[ctx->stack_size - 2].i32;
    ctx->stack[ctx->stack_size - 2].i32 = (a > b) ? 1 : 0;
    ctx->stack_size--;
    return true;
}

// Integer arithmetic operations
static bool execute_i32_add(wasm_exec_context_t* ctx) {
    int32_t b = ctx->stack[ctx->stack_size - 1].i32;
    int32_t a = ctx->stack[ctx->stack_size - 2].i32;
    ctx->stack[ctx->stack_size - 2].i32 = a + b;
    ctx->stack_size--;
    return true;
}

static bool execute_i32_sub(wasm_exec_context_t* ctx) {
    int32_t b = ctx->stack[ctx->stack_size - 1].i32;
    int32_t a = ctx->stack[ctx->stack_size - 2].i32;
    ctx->stack[ctx->stack_size - 2].i32 = a - b;
    ctx->stack_size--;
    return true;
}

static bool execute_i32_mul(wasm_exec_context_t* ctx) {
    int32_t b = ctx->stack[ctx->stack_size - 1].i32;
    int32_t a = ctx->stack[ctx->stack_size - 2].i32;
    ctx->stack[ctx->stack_size - 2].i32 = a * b;
    ctx->stack_size--;
    return true;
}

static bool execute_i32_div_s(wasm_exec_context_t* ctx) {
    int32_t b = ctx->stack[ctx->stack_size - 1].i32;
    int32_t a = ctx->stack[ctx->stack_size - 2].i32;
    
    if (b == 0) {
        return false; // Division by zero
    }
    
    if (a == INT32_MIN && b == -1) {
        return false; // Integer overflow
    }
    
    ctx->stack[ctx->stack_size - 2].i32 = a / b;
    ctx->stack_size--;
    return true;
}

static bool execute_i32_div_u(wasm_exec_context_t* ctx) {
    uint32_t b = (uint32_t)ctx->stack[ctx->stack_size - 1].i32;
    uint32_t a = (uint32_t)ctx->stack[ctx->stack_size - 2].i32;
    
    if (b == 0) {
        return false; // Division by zero
    }
    
    ctx->stack[ctx->stack_size - 2].i32 = (int32_t)(a / b);
    ctx->stack_size--;
    return true;
}

static bool execute_i32_rem_s(wasm_exec_context_t* ctx) {
    int32_t b = ctx->stack[ctx->stack_size - 1].i32;
    int32_t a = ctx->stack[ctx->stack_size - 2].i32;
    
    if (b == 0) {
        return false; // Division by zero
    }
    
    if (a == INT32_MIN && b == -1) {
        return false; // Integer overflow
    }
    
    ctx->stack[ctx->stack_size - 2].i32 = a % b;
    ctx->stack_size--;
    return true;
}

static bool execute_i32_rem_u(wasm_exec_context_t* ctx) {
    uint32_t b = (uint32_t)ctx->stack[ctx->stack_size - 1].i32;
    uint32_t a = (uint32_t)ctx->stack[ctx->stack_size - 2].i32;
    
    if (b == 0) {
        return false; // Division by zero
    }
    
    ctx->stack[ctx->stack_size - 2].i32 = (int32_t)(a % b);
    ctx->stack_size--;
    return true;
}

// Implement read_leb128_u32
static bool read_leb128_u32(const uint8_t* bytes, size_t size, size_t* offset, uint32_t* value) {
    if (!bytes || !offset || !value || *offset >= size) {
        return false;
    }
    uint32_t result = 0;
    uint32_t shift = 0;
    uint8_t byte;
    do {
        byte = bytes[(*offset)++];
        result |= ((byte & 0x7F) << shift);
        shift += 7;
    } while (byte & 0x80);
    *value = result;
    return true;
}

// Add this function before wasm_execute_instruction
static bool execute_i32_eqz(wasm_exec_context_t* ctx) {
    if (ctx->stack_size < 1) {
        kprintf(ERROR, "Stack underflow in i32.eqz\n");
        return false;
    }
    
    // Get the value from the top of the stack
    int32_t value = ctx->stack[ctx->stack_size - 1].i32;
    
    // Replace it with the result of the comparison
    ctx->stack[ctx->stack_size - 1].i32 = (value == 0) ? 1 : 0;
    
    return true;
}

// Execute a single WebAssembly instruction
bool wasm_execute_instruction(wasm_exec_context_t* ctx) {
    if (!ctx || !ctx->pc) return false;
    
    wasm_opcode_t opcode = (wasm_opcode_t)*ctx->pc++;
    
    switch (opcode) {
        case WASM_OP_UNREACHABLE:
            kprintf(ERROR, "Unreachable instruction executed\n");
            return false;
            
        case WASM_OP_NOP:
            return true;
            
        case WASM_OP_BLOCK: {
            uint8_t block_type;
            if (!read_block_type(ctx->pc, &block_type)) {
                return false;
            }
            ctx->pc++;  // Skip block type byte
            
            wasm_block_t block = {
                .start_pc = ctx->pc,
                .end_pc = NULL,  // Will be set by END
                .stack_size = ctx->stack_size,
                .block_type = block_type,
                .is_loop = false
            };
            
            if (!block_stack_push(ctx, block)) {
                return false;
            }
            return true;
        }
        
        case WASM_OP_LOOP: {
            uint8_t block_type;
            if (!read_block_type(ctx->pc, &block_type)) {
                return false;
            }
            ctx->pc++;  // Skip block type byte
            
            wasm_block_t block = {
                .start_pc = ctx->pc,
                .end_pc = NULL,  // Will be set by END
                .stack_size = ctx->stack_size,
                .block_type = block_type,
                .is_loop = true
            };
            
            if (!block_stack_push(ctx, block)) {
                return false;
            }
            return true;
        }
        
        case WASM_OP_IF: {
            wasm_value_t condition;
            if (!stack_pop(ctx, &condition)) {
                return false;
            }
            
            uint8_t block_type;
            if (!read_block_type(ctx->pc, &block_type)) {
                return false;
            }
            ctx->pc++;  // Skip block type byte
            
            wasm_block_t block = {
                .start_pc = ctx->pc,
                .end_pc = NULL,  // Will be set by END
                .stack_size = ctx->stack_size,
                .block_type = block_type
            };
            
            if (!block_stack_push(ctx, block)) {
                return false;
            }
            
            // If condition is false, skip to else/end
            if (!condition.i32) {
                // Skip to end of block
                while (*ctx->pc != WASM_OP_END && *ctx->pc != WASM_OP_ELSE) {
                    ctx->pc++;
                }
            }
            return true;
        }
        
        case WASM_OP_ELSE: {
            wasm_block_t block;
            if (!block_stack_pop(ctx, &block)) {
                return false;
            }
            
            // Skip to end of block
            while (*ctx->pc != WASM_OP_END) {
                ctx->pc++;
            }
            return true;
        }
        
        case WASM_OP_END: {
            wasm_block_t block;
            if (!block_stack_pop(ctx, &block)) {
                // If no block to pop, this is the end of the function
                ctx->pc = NULL;  // Signal end of function
                return true;
            }
            
            // Set end_pc for the block
            block.end_pc = ctx->pc;
            
            // Restore stack to block's initial size
            ctx->stack_size = block.stack_size;
            return true;
        }
        
        case WASM_OP_BR: {
            uint32_t depth;
            if (!read_leb128_u32(ctx->pc, 0, NULL, &depth)) {
                return false;
            }
            
            if (depth >= ctx->block_stack_size) {
                kprintf(ERROR, "Invalid branch depth: %d\n", depth);
                return false;
            }
            
            // Get target block
            wasm_block_t target = ctx->block_stack[ctx->block_stack_size - 1 - depth];
            
            // Restore stack to target block's initial size
            ctx->stack_size = target.stack_size;
            
            // Jump to start of target block if it's a loop, end if it's a block
            if (target.is_loop) {
                ctx->pc = target.start_pc;
            } else {
                ctx->pc = target.end_pc;
            }
            return true;
        }
        
        case WASM_OP_BR_IF: {
            wasm_value_t condition;
            if (!stack_pop(ctx, &condition)) {
                return false;
            }
            
            if (!condition.i32) {
                ctx->pc++;  // Skip branch target if condition is false
                return true;  // Don't branch
            }
            
            uint32_t depth;
            if (!read_leb128_u32(ctx->pc, 0, NULL, &depth)) {
                return false;
            }
            
            if (depth >= ctx->block_stack_size) {
                kprintf(ERROR, "Invalid branch depth: %d\n", depth);
                return false;
            }
            
            // Get target block
            wasm_block_t target = ctx->block_stack[ctx->block_stack_size - 1 - depth];
            
            // Restore stack to target block's initial size
            ctx->stack_size = target.stack_size;
            
            // Jump to start of target block if it's a loop, end if it's a block
            if (target.is_loop) {
                ctx->pc = target.start_pc;
            } else {
                ctx->pc = target.end_pc;
            }
            return true;
        }
        
        case WASM_OP_RETURN: {
            // Pop all blocks
            while (ctx->block_stack_size > 0) {
                wasm_block_t block;
                block_stack_pop(ctx, &block);
            }
            
            // Set PC to NULL to end execution
            ctx->pc = NULL;
            return true;
        }
        
        case WASM_OP_LOCAL_GET: {
            uint32_t index = 0;
            uint32_t shift = 0;
            uint8_t byte;
            
            do {
                if (!ctx->pc) {
                    kprintf(ERROR, "Unexpected end of code while reading local index\n");
                    return false;
                }
                byte = *ctx->pc++;
                index |= ((byte & 0x7F) << shift);
                shift += 7;
            } while (byte & 0x80);
            
            if (index >= ctx->local_count) {
                kprintf(ERROR, "Invalid local index: %d (max: %d)\n", index, ctx->local_count - 1);
                return false;
            }
            
            wasm_value_t value = { .i32 = ctx->locals[index] };
            return stack_push(ctx, value);
        }
        
        case WASM_OP_LOCAL_SET: {
            uint32_t index = 0;
            uint32_t shift = 0;
            uint8_t byte;
            
            do {
                if (!ctx->pc) {
                    kprintf(ERROR, "Unexpected end of code while reading local index\n");
                    return false;
                }
                byte = *ctx->pc++;
                index |= ((byte & 0x7F) << shift);
                shift += 7;
            } while (byte & 0x80);
            
            if (index >= ctx->local_count) {
                kprintf(ERROR, "Invalid local index: %d (max: %d)\n", index, ctx->local_count - 1);
                return false;
            }
            
            wasm_value_t value;
            if (!stack_pop(ctx, &value)) {
                kprintf(ERROR, "Stack underflow while setting local %d\n", index);
                return false;
            }
            
            ctx->locals[index] = value.i32;
            return true;
        }
        
        case WASM_OP_LOCAL_TEE: {
            uint32_t index = 0;
            uint32_t shift = 0;
            uint8_t byte;
            
            do {
                if (!ctx->pc) {
                    kprintf(ERROR, "Unexpected end of code while reading local index\n");
                    return false;
                }
                byte = *ctx->pc++;
                index |= ((byte & 0x7F) << shift);
                shift += 7;
            } while (byte & 0x80);
            
            if (index >= ctx->local_count) {
                kprintf(ERROR, "Invalid local index: %d (max: %d)\n", index, ctx->local_count - 1);
                return false;
            }
            
            // Get value from top of stack without popping
            wasm_value_t value = ctx->stack[ctx->stack_size - 1];
            
            // Set local
            ctx->locals[index] = value.i32;
            
            // Value remains on stack
            return true;
        }
        
        case WASM_OP_GLOBAL_GET: {
            uint32_t global_idx = 0;
            uint32_t shift = 0;
            uint8_t byte;

            do {
                if (!ctx->pc) {
                    kprintf(ERROR, "Unexpected end of code while reading global index\n");
                    return false;
                }
                byte = *ctx->pc++;
                global_idx |= ((byte & 0x7F) << shift);
                shift += 7;
            } while (byte & 0x80);

            if (global_idx >= ctx->instance->global_count) {
                kprintf(ERROR, "Invalid global index: %d (max: %d)\n", 
                        global_idx, ctx->instance->global_count - 1);
                return false;
            }

            // Push global value onto stack
            return stack_push(ctx, ctx->instance->globals[global_idx].value);
        }

        case WASM_OP_GLOBAL_SET: {
            uint32_t global_idx = 0;
            uint32_t shift = 0;
            uint8_t byte;

            do {
                if (!ctx->pc) {
                    kprintf(ERROR, "Unexpected end of code while reading global index\n");
                    return false;
                }
                byte = *ctx->pc++;
                global_idx |= ((byte & 0x7F) << shift);
                shift += 7;
            } while (byte & 0x80);

            if (global_idx >= ctx->instance->global_count) {
                kprintf(ERROR, "Invalid global index: %d (max: %d)\n", 
                        global_idx, ctx->instance->global_count - 1);
                return false;
            }
            // Check if global is mutable
            if (!ctx->instance->globals[global_idx].mut) {
                kprintf(ERROR, "Attempt to set immutable global %d\n", global_idx);
                return false;
            }
            // Pop value from stack and set global
            wasm_value_t value;
            if (!stack_pop(ctx, &value)) {
                kprintf(ERROR, "Stack underflow while setting global %d\n", global_idx);
                return false;
            }
            ctx->instance->globals[global_idx].value = value;
            return true;
        }
        
        // Memory operations
        case WASM_OP_I32_LOAD:
            return execute_i32_load(ctx, &ctx->pc);
        case WASM_OP_I64_LOAD:
            return execute_i64_load(ctx, &ctx->pc);
        case WASM_OP_F32_LOAD:
            return execute_f32_load(ctx, &ctx->pc);
        case WASM_OP_F64_LOAD:
            return execute_f64_load(ctx, &ctx->pc);
        case WASM_OP_I32_STORE:
            return execute_i32_store(ctx, &ctx->pc);
        case WASM_OP_I64_STORE:
            return execute_i64_store(ctx, &ctx->pc);
        case WASM_OP_F32_STORE:
            return execute_f32_store(ctx, &ctx->pc);
        case WASM_OP_F64_STORE:
            return execute_f64_store(ctx, &ctx->pc);
        
        // Numeric instructions
        case WASM_OP_I32_CONST: {
            int32_t value = 0;
            uint32_t shift = 0;
            uint8_t byte;
            
            do {
                if (!ctx->pc) {
                    kprintf(ERROR, "Unexpected end of code while reading i32.const value\n");
                    return false;
                }
                byte = *ctx->pc++;
                value |= ((byte & 0x7F) << shift);
                if (shift < 32 && (byte & 0x40)) {
                    value |= -(1 << shift);
                }
                shift += 7;
            } while (byte & 0x80);
            
            wasm_value_t val = { .i32 = value };
            return stack_push(ctx, val);
        }
        case WASM_OP_I64_CONST:
            return execute_i64_const(ctx);
        case WASM_OP_F32_CONST:
            return execute_f32_const(ctx);
        case WASM_OP_F64_CONST:
            return execute_f64_const(ctx);
            
        // Integer comparison
        case WASM_OP_I32_EQZ:
            return execute_i32_eqz(ctx);
        case WASM_OP_I32_EQ:
            return execute_i32_eq(ctx);
        case WASM_OP_I32_NE:
            return execute_i32_ne(ctx);
        case WASM_OP_I32_LT_S:
            return execute_i32_lt_s(ctx);
        case WASM_OP_I32_LT_U:
            return execute_i32_lt_u(ctx);
        case WASM_OP_I32_GT_S:
            return execute_i32_gt_s(ctx);
        case WASM_OP_I32_GT_U:
            return execute_i32_gt_u(ctx);
            
        // Integer arithmetic
        case WASM_OP_I32_ADD:
            return execute_i32_add(ctx);
        case WASM_OP_I32_SUB:
            return execute_i32_sub(ctx);
        case WASM_OP_I32_MUL:
            return execute_i32_mul(ctx);
        case WASM_OP_I32_DIV_S:
            return execute_i32_div_s(ctx);
        case WASM_OP_I32_DIV_U:
            return execute_i32_div_u(ctx);
        case WASM_OP_I32_REM_S:
            return execute_i32_rem_s(ctx);
        case WASM_OP_I32_REM_U:
            return execute_i32_rem_u(ctx);
        
        // Add i32_le_s implementation
        case WASM_OP_I32_LE_S:
            return execute_i32_le_s(ctx);
            
        case WASM_OP_CALL: {  // call
            uint32_t func_idx = 0;
            uint32_t shift = 0;
            uint8_t byte;
            
            do {
                if (!ctx->pc) {
                    kprintf(ERROR, "Unexpected end of code while reading function index\n");
                    return false;
                }
                byte = *ctx->pc++;
                func_idx |= ((byte & 0x7F) << shift);
                shift += 7;
            } while (byte & 0x80);

            kprintf(DEBUG, "[WASM] CALL: func_idx=%u, stack_size=%u\n", func_idx, ctx->stack_size);

            // Host function call
            if (func_idx < ctx->instance->module->import_count) {
                kprintf(DEBUG, "[WASM] Host function call: func_idx=%u, host_count=%u, function_count=%u\n", 
                       func_idx, ctx->instance->host_function_count, ctx->instance->function_count);
                // Prepare arguments from stack
                uint32_t arg_count = ctx->instance->module->types[ctx->instance->module->imports[func_idx].type_index].param_count;
                kprintf(DEBUG, "[WASM] Host function requires %u arguments\n", arg_count);
                
                // Check if we have enough arguments on the stack
                if (ctx->stack_size < arg_count) {
                    kprintf(ERROR, "Stack underflow: need %d arguments but only have %d\n", 
                           arg_count, ctx->stack_size);
                    return false;
                }
                
                wasm_value_t* args = kmalloc(sizeof(wasm_value_t) * arg_count);
                if (!args) {
                    kprintf(ERROR, "Failed to allocate arguments for host function call\n");
                    return false;
                }
                
                // Pop arguments from stack in reverse order
                for (int32_t i = arg_count - 1; i >= 0; i--) {
                    if (!stack_pop(ctx, &args[i])) {
                        kprintf(ERROR, "Failed to pop argument %d for host function call\n", i);
                        kfree(args);
                        return false;
                    }
                }
                
                kprintf(DEBUG, "[WASM] Host function args: ");
                for (uint32_t i = 0; i < arg_count; i++) {
                    kprintf(DEBUG, "%d ", args[i].i32);
                }
                kprintf(DEBUG, "\n");
                
                // Call the host function
                wasm_host_function_t host_func = ctx->instance->host_functions[0];  // We only have one host function (proc_exit)
                wasm_value_t result;
                bool success = host_func(ctx->instance, args, arg_count, &result);
                kfree(args);
                if (!success) {
                    kprintf(ERROR, "Host function call failed\n");
                    return false;
                }
                
                kprintf(DEBUG, "[WASM] Host function call returned: %d\n", result.i32);
                // Push result onto stack if function returns a value
                if (ctx->instance->module->types[ctx->instance->module->imports[func_idx].type_index].result_count > 0) {
                    return stack_push(ctx, result);
                }
                return true;
            }
            
            // Regular function call
            if (func_idx >= ctx->instance->function_count) {
                kprintf(ERROR, "Invalid function index: %d (max: %d)\n", 
                        func_idx, ctx->instance->function_count - 1);
                return false;
            }
            
            kprintf(DEBUG, "[WASM] Regular function call: idx=%u\n", func_idx);
            
            // Get the function to call
            wasm_function_t* callee = &ctx->instance->functions[func_idx];
            kprintf(DEBUG, "[WASM] Function type: param_count=%u, result_count=%u\n", 
                    callee->type->param_count, callee->type->result_count);
            
            // Check if we have enough arguments on the stack
            if (ctx->stack_size < callee->type->param_count) {
                kprintf(ERROR, "Stack underflow: need %d arguments but only have %d\n", 
                       callee->type->param_count, ctx->stack_size);
                return false;
            }
            
            // Prepare arguments from stack
            wasm_value_t* args = kmalloc(sizeof(wasm_value_t) * callee->type->param_count);
            if (!args) {
                kprintf(ERROR, "Failed to allocate arguments for function call\n");
                return false;
            }
            
            // Pop arguments from stack in reverse order
            for (int32_t i = callee->type->param_count - 1; i >= 0; i--) {
                if (!stack_pop(ctx, &args[i])) {
                    kprintf(ERROR, "Failed to pop argument %d for function call\n", i);
                    kfree(args);
                    return false;
                }
            }
            
            kprintf(DEBUG, "[WASM] Regular function args: ");
            for (uint32_t i = 0; i < callee->type->param_count; i++) {
                kprintf(DEBUG, "%d ", args[i].i32);
            }
            kprintf(DEBUG, "\n");
            
            // Call the function
            wasm_value_t result;
            bool success = wasm_execute_function(callee, args, callee->type->param_count, &result);
            kfree(args);
            if (!success) {
                kprintf(ERROR, "Failed to execute called function\n");
                return false;
            }
            
            kprintf(DEBUG, "[WASM] Regular function call returned: %d\n", result.i32);
            // Push result onto stack if function returns a value
            if (callee->type->result_count > 0) {
                return stack_push(ctx, result);
            }
            return true;
        }
        
        default:
            kprintf(ERROR, "Unsupported WebAssembly opcode: 0x%02x\n", opcode);
            return false;
    }
}

// Execute a WebAssembly function
bool wasm_execute_function(wasm_function_t* function, wasm_value_t* args, uint32_t arg_count, wasm_value_t* result) {
    if (!function || !function->code || !result) {
        kprintf(ERROR, "Invalid WebAssembly function call\n");
        return false;
    }
    
    // Initialize execution context with total locals
    wasm_exec_context_t ctx;
    memset(&ctx, 0, sizeof(wasm_exec_context_t));  // Zero out the context first
    wasm_exec_context_init(&ctx, function->module, function->local_count);
    
    // Set up program counter
    ctx.pc = function->code;
    
    // Copy arguments to locals
    for (uint32_t i = 0; i < arg_count && i < function->type->param_count; i++) {
        ctx.locals[i] = args[i].i32;
    }
    
    // Initialize remaining locals to 0
    for (uint32_t i = arg_count; i < function->local_count; i++) {
        ctx.locals[i] = 0;
    }
    
    // Execute instructions until we hit an end or return
    bool success = true;
    while (success && ctx.pc) {
        const uint8_t* instruction_start = ctx.pc;  // Save start of instruction
        uint8_t current_opcode = *ctx.pc;  // Save opcode before execution
        kprintf(DEBUG, "Executing instruction 0x%x at offset %d\n", 
                current_opcode,
                (int)(instruction_start - (uint8_t*)function->code));
        success = wasm_execute_instruction(&ctx);
        if (ctx.instance->should_exit) {
            break;
        }
        if (!success) {
            kprintf(ERROR, "Failed to execute instruction 0x%x at offset %d\n", 
                    current_opcode,
                    (int)(instruction_start - (uint8_t*)function->code));
        }
    }
    
    // Get result from stack if available
    if (success && ctx.stack_size > 0) {
        *result = ctx.stack[ctx.stack_size - 1];
    } else {
        result->i32 = 0;  // Default result if no value on stack
    }
    
    // Clean up execution context
    wasm_exec_context_cleanup(&ctx);
    
    return success;
}

// Add i32_le_s implementation
static bool execute_i32_le_s(wasm_exec_context_t* ctx) {
    wasm_value_t b, a;
    if (!stack_pop(ctx, &b) || !stack_pop(ctx, &a)) {
        return false;
    }
    wasm_value_t result = { .i32 = a.i32 <= b.i32 };
    return stack_push(ctx, result);
} 