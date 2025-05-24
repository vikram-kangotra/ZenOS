#include "drivers/ata.h"
#include "kernel/kprintf.h"
#include "kernel/mm/kmalloc.h"
#include "string.h"
#include "stdbool.h"

// Test buffer size
#define TEST_BUFFER_SIZE 512

// Test patterns
static const uint8_t PATTERN_1[TEST_BUFFER_SIZE] = {0xAA};  // Alternating bits
static const uint8_t PATTERN_2[TEST_BUFFER_SIZE] = {0x55};  // Alternating bits (inverse)
static const uint8_t PATTERN_3[TEST_BUFFER_SIZE] = {0xFF};  // All ones
static const uint8_t PATTERN_4[TEST_BUFFER_SIZE] = {0x00};  // All zeros

// Test sectors
#define TEST_SECTOR_START 100  // Start testing from sector 100 to avoid overwriting important data
#define TEST_SECTOR_COUNT 4    // Test 4 sectors

// Helper function to compare buffers
static bool compare_buffers(const uint8_t* buf1, const uint8_t* buf2, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (buf1[i] != buf2[i]) {
            kprintf(ERROR, "Buffer mismatch at offset %d: expected 0x%02x, got 0x%02x\n", 
                    i, buf1[i], buf2[i]);
            return false;
        }
    }
    return true;
}

// Test single sector write and read
static bool test_single_sector(uint32_t sector, const uint8_t* pattern) {
    uint8_t write_buffer[TEST_BUFFER_SIZE];
    uint8_t read_buffer[TEST_BUFFER_SIZE];
    
    // Fill write buffer with pattern
    for (int i = 0; i < TEST_BUFFER_SIZE; i++) {
        write_buffer[i] = pattern[i % sizeof(pattern)];
    }
    
    // Write the pattern
    kprintf(INFO, "Writing pattern to sector %d...\n", sector);
    if (!ata_write_sector(sector, write_buffer)) {
        kprintf(ERROR, "Failed to write sector %d\n", sector);
        return false;
    }
    
    // Read it back
    kprintf(INFO, "Reading sector %d...\n", sector);
    if (!ata_read_sector(sector, read_buffer)) {
        kprintf(ERROR, "Failed to read sector %d\n", sector);
        return false;
    }
    
    // Compare buffers
    if (!compare_buffers(write_buffer, read_buffer, TEST_BUFFER_SIZE)) {
        kprintf(ERROR, "Data verification failed for sector %d\n", sector);
        return false;
    }
    
    kprintf(INFO, "Sector %d test passed\n", sector);
    return true;
}

// Test multiple sectors
static bool test_multiple_sectors(uint32_t start_sector, uint32_t count, const uint8_t* pattern) {
    uint8_t* write_buffer = kmalloc(TEST_BUFFER_SIZE * count);
    uint8_t* read_buffer = kmalloc(TEST_BUFFER_SIZE * count);
    
    if (!write_buffer || !read_buffer) {
        kprintf(ERROR, "Failed to allocate test buffers\n");
        if (write_buffer) kfree(write_buffer);
        if (read_buffer) kfree(read_buffer);
        return false;
    }
    
    // Fill write buffer with pattern
    for (uint32_t i = 0; i < TEST_BUFFER_SIZE * count; i++) {
        write_buffer[i] = pattern[i % sizeof(pattern)];
    }
    
    // Write the pattern
    kprintf(INFO, "Writing pattern to sectors %d-%d...\n", start_sector, start_sector + count - 1);
    if (!ata_write_sectors(start_sector, count, write_buffer)) {
        kprintf(ERROR, "Failed to write sectors %d-%d\n", start_sector, start_sector + count - 1);
        kfree(write_buffer);
        kfree(read_buffer);
        return false;
    }
    
    // Read it back
    kprintf(INFO, "Reading sectors %d-%d...\n", start_sector, start_sector + count - 1);
    if (!ata_read_sectors(start_sector, count, read_buffer)) {
        kprintf(ERROR, "Failed to read sectors %d-%d\n", start_sector, start_sector + count - 1);
        kfree(write_buffer);
        kfree(read_buffer);
        return false;
    }
    
    // Compare buffers
    if (!compare_buffers(write_buffer, read_buffer, TEST_BUFFER_SIZE * count)) {
        kprintf(ERROR, "Data verification failed for sectors %d-%d\n", 
                start_sector, start_sector + count - 1);
        kfree(write_buffer);
        kfree(read_buffer);
        return false;
    }
    
    kprintf(INFO, "Sectors %d-%d test passed\n", start_sector, start_sector + count - 1);
    
    kfree(write_buffer);
    kfree(read_buffer);
    return true;
}

// Run all ATA tests
void run_ata_tests(void) {
    kprintf(INFO, "Starting ATA driver tests...\n");
    
    // Test single sector operations with different patterns
    kprintf(INFO, "\nTesting single sector operations...\n");
    if (!test_single_sector(TEST_SECTOR_START, PATTERN_1)) return;
    if (!test_single_sector(TEST_SECTOR_START + 1, PATTERN_2)) return;
    if (!test_single_sector(TEST_SECTOR_START + 2, PATTERN_3)) return;
    if (!test_single_sector(TEST_SECTOR_START + 3, PATTERN_4)) return;
    
    // Test multiple sector operations
    kprintf(INFO, "\nTesting multiple sector operations...\n");
    if (!test_multiple_sectors(TEST_SECTOR_START, TEST_SECTOR_COUNT, PATTERN_1)) return;
    
    // Test sector boundary conditions
    kprintf(INFO, "\nTesting sector boundary conditions...\n");
    if (!test_single_sector(0, PATTERN_1)) return;  // First sector
    if (!test_single_sector(1, PATTERN_2)) return;  // Second sector
    
    kprintf(INFO, "\nAll ATA tests passed successfully!\n");
} 