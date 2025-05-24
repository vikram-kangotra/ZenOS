#include "drivers/ata.h"
#include "kernel/kprintf.h"
#include "kernel/mm/kmalloc.h"
#include "string.h"
#include "stdbool.h"

// Test patterns
static const uint8_t PATTERN_1[512] = {[0 ... 511] = 0xAA};  // Alternating bits
static const uint8_t PATTERN_2[512] = {[0 ... 511] = 0x55};  // Alternating bits (inverse)
static const uint8_t PATTERN_3[512] = {[0 ... 511] = 0xFF};  // All ones
static const uint8_t PATTERN_4[512] = {[0 ... 511] = 0x00};  // All zeros

// Test sectors - start from sector 100 to avoid overwriting important data
#define TEST_SECTOR_START 100
#define TEST_SECTOR_COUNT 4

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

// Test single sector operations
static bool test_single_sector(struct ata_device* dev, uint32_t sector, const uint8_t* pattern) {
    uint8_t* write_buffer = kmalloc(512);
    uint8_t* read_buffer = kmalloc(512);
    
    if (!write_buffer || !read_buffer) {
        kprintf(ERROR, "Failed to allocate test buffers\n");
        if (write_buffer) kfree(write_buffer);
        if (read_buffer) kfree(read_buffer);
        return false;
    }
    
    // Fill write buffer with pattern
    memcpy(write_buffer, pattern, 512);
    
    // Write the pattern
    kprintf(INFO, "Writing pattern to sector %d...\n", sector);
    if (!ata_write_sectors(dev, sector, 1, write_buffer)) {
        kprintf(ERROR, "Failed to write sector %d\n", sector);
        kfree(write_buffer);
        kfree(read_buffer);
        return false;
    }
    
    // Read it back
    kprintf(INFO, "Reading sector %d...\n", sector);
    if (!ata_read_sectors(dev, sector, 1, read_buffer)) {
        kprintf(ERROR, "Failed to read sector %d\n", sector);
        kfree(write_buffer);
        kfree(read_buffer);
        return false;
    }
    
    // Compare buffers
    bool result = compare_buffers(write_buffer, read_buffer, 512);
    if (result) {
        kprintf(INFO, "Sector %d test passed\n", sector);
    } else {
        kprintf(ERROR, "Data verification failed for sector %d\n", sector);
    }
    
    kfree(write_buffer);
    kfree(read_buffer);
    return result;
}

// Test multiple sector operations
static bool test_multiple_sectors(struct ata_device* dev, uint32_t start_sector, uint8_t count, const uint8_t* pattern) {
    uint8_t* write_buffer = kmalloc(512 * count);
    uint8_t* read_buffer = kmalloc(512 * count);
    
    if (!write_buffer || !read_buffer) {
        kprintf(ERROR, "Failed to allocate test buffers\n");
        if (write_buffer) kfree(write_buffer);
        if (read_buffer) kfree(read_buffer);
        return false;
    }
    
    // Fill write buffer with pattern
    for (uint8_t i = 0; i < count; i++) {
        memcpy(write_buffer + (i * 512), pattern, 512);
    }
    
    // Write the pattern
    kprintf(INFO, "Writing pattern to sectors %d-%d...\n", start_sector, start_sector + count - 1);
    if (!ata_write_sectors(dev, start_sector, count, write_buffer)) {
        kprintf(ERROR, "Failed to write sectors %d-%d\n", start_sector, start_sector + count - 1);
        kfree(write_buffer);
        kfree(read_buffer);
        return false;
    }
    
    // Read it back
    kprintf(INFO, "Reading sectors %d-%d...\n", start_sector, start_sector + count - 1);
    if (!ata_read_sectors(dev, start_sector, count, read_buffer)) {
        kprintf(ERROR, "Failed to read sectors %d-%d\n", start_sector, start_sector + count - 1);
        kfree(write_buffer);
        kfree(read_buffer);
        return false;
    }
    
    // Compare buffers
    bool result = compare_buffers(write_buffer, read_buffer, 512 * count);
    if (result) {
        kprintf(INFO, "Sectors %d-%d test passed\n", start_sector, start_sector + count - 1);
    } else {
        kprintf(ERROR, "Data verification failed for sectors %d-%d\n", 
                start_sector, start_sector + count - 1);
    }
    
    kfree(write_buffer);
    kfree(read_buffer);
    return result;
}

// Test boundary conditions
static bool test_boundary_conditions(struct ata_device* dev) {
    kprintf(INFO, "\nTesting boundary conditions...\n");
    
    // Test first writable sector (sector 1) instead of sector 0
    kprintf(INFO, "Testing first writable sector (sector 1)...\n");
    if (!test_single_sector(dev, 1, PATTERN_1)) {
        kprintf(ERROR, "First writable sector test failed\n");
        return false;
    }
    
    // Test last sector
    uint32_t last_sector = dev->sectors - 1;
    kprintf(INFO, "Testing last sector (%d)...\n", last_sector);
    if (!test_single_sector(dev, last_sector, PATTERN_2)) {
        kprintf(ERROR, "Last sector test failed\n");
        return false;
    }
    
    // Test invalid sector
    kprintf(INFO, "Testing invalid sector access (beyond last sector)...\n");
    uint8_t buffer[512];
    bool invalid_read_succeeded = ata_read_sectors(dev, dev->sectors + 1, 1, buffer);
    if (invalid_read_succeeded) {
        kprintf(ERROR, "Invalid sector read should have failed\n");
        return false;
    }
    kprintf(INFO, "Invalid sector access test passed (correctly rejected)\n");
    
    return true;
}

// Run all ATA tests
void run_ata_tests(void) {
    kprintf(INFO, "Starting ATA driver tests...\n");
    
    // Get primary master device
    struct ata_device* dev = ata_get_device(0, 0);
    if (!dev || !dev->exists) {
        kprintf(ERROR, "No ATA device found for testing\n");
        return;
    }
    
    kprintf(INFO, "Testing device: %s (Serial: %s)\n", dev->model, dev->serial);
    
    bool all_tests_passed = true;
    
    // Test single sector operations with different patterns
    kprintf(INFO, "\nTesting single sector operations...\n");
    if (!test_single_sector(dev, TEST_SECTOR_START, PATTERN_1)) all_tests_passed = false;
    if (!test_single_sector(dev, TEST_SECTOR_START + 1, PATTERN_2)) all_tests_passed = false;
    if (!test_single_sector(dev, TEST_SECTOR_START + 2, PATTERN_3)) all_tests_passed = false;
    if (!test_single_sector(dev, TEST_SECTOR_START + 3, PATTERN_4)) all_tests_passed = false;
    
    // Test multiple sector operations
    kprintf(INFO, "\nTesting multiple sector operations...\n");
    if (!test_multiple_sectors(dev, TEST_SECTOR_START, TEST_SECTOR_COUNT, PATTERN_1)) all_tests_passed = false;
    
    // Test boundary conditions
    if (!test_boundary_conditions(dev)) all_tests_passed = false;
    
    if (all_tests_passed) {
        kprintf(INFO, "\nAll ATA tests completed successfully!\n");
    } else {
        kprintf(ERROR, "\nSome ATA tests failed!\n");
    }
} 