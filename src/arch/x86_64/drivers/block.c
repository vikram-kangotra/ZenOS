#include "drivers/block.h"
#include "kernel/kprintf.h"
#include "kernel/mm/kmalloc.h"
#include "string.h"

#define MAX_BLOCK_DEVICES 16

static struct block_device* devices[MAX_BLOCK_DEVICES];
static size_t num_devices = 0;

void block_device_register(struct block_device* dev) {
    if (num_devices >= MAX_BLOCK_DEVICES) {
        kprintf(ERROR, "Too many block devices\n");
        return;
    }

    // Check if device already exists
    for (size_t i = 0; i < num_devices; i++) {
        if (strcmp(devices[i]->name, dev->name) == 0) {
            kprintf(ERROR, "Block device %s already registered\n", dev->name);
            return;
        }
    }

    devices[num_devices++] = dev;
    kprintf(INFO, "Registered block device: %s\n", dev->name);
}

struct block_device* block_device_get(const char* name) {
    for (size_t i = 0; i < num_devices; i++) {
        if (strcmp(devices[i]->name, name) == 0) {
            return devices[i];
        }
    }
    return NULL;
}

bool block_device_read(struct block_device* dev, uint64_t lba, uint32_t count, void* buffer) {
    if (!dev || !dev->ops || !dev->ops->read) {
        kprintf(ERROR, "Invalid block device or operation\n");
        return false;
    }
    return dev->ops->read(dev->private_data, lba, count, buffer);
}

bool block_device_write(struct block_device* dev, uint64_t lba, uint32_t count, const void* buffer) {
    if (!dev || !dev->ops || !dev->ops->write) {
        kprintf(ERROR, "Invalid block device or operation\n");
        return false;
    }
    return dev->ops->write(dev->private_data, lba, count, buffer);
}

uint32_t block_device_get_sector_size(struct block_device* dev) {
    if (!dev || !dev->ops || !dev->ops->get_sector_size) {
        kprintf(ERROR, "Invalid block device or operation\n");
        return 0;
    }
    return dev->ops->get_sector_size(dev->private_data);
}

uint64_t block_device_get_sector_count(struct block_device* dev) {
    if (!dev || !dev->ops || !dev->ops->get_sector_count) {
        kprintf(ERROR, "Invalid block device or operation\n");
        return 0;
    }
    return dev->ops->get_sector_count(dev->private_data);
}

bool block_device_sync(struct block_device* dev) {
    if (!dev || !dev->ops || !dev->ops->sync) {
        kprintf(ERROR, "Invalid block device or operation\n");
        return false;
    }
    return dev->ops->sync(dev->private_data);
} 