#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <stdbool.h>

// Block device operations
struct block_device_ops {
    bool (*read)(void* dev, uint64_t lba, uint32_t count, void* buffer);
    bool (*write)(void* dev, uint64_t lba, uint32_t count, const void* buffer);
    uint32_t (*get_sector_size)(void* dev);
    uint64_t (*get_sector_count)(void* dev);
    bool (*sync)(void* dev);
};

// Block device structure
struct block_device {
    const char* name;
    void* private_data;
    const struct block_device_ops* ops;
    bool mounted;
};

// Function declarations
void block_device_register(struct block_device* dev);
struct block_device* block_device_get(const char* name);
bool block_device_read(struct block_device* dev, uint64_t lba, uint32_t count, void* buffer);
bool block_device_write(struct block_device* dev, uint64_t lba, uint32_t count, const void* buffer);
uint32_t block_device_get_sector_size(struct block_device* dev);
uint64_t block_device_get_sector_count(struct block_device* dev);
bool block_device_sync(struct block_device* dev);

#endif // BLOCK_H 