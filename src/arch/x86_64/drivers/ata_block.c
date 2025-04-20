#include "drivers/ata.h"
#include "drivers/block.h"
#include "kernel/mm/kmalloc.h"
#include "kernel/kprintf.h"

// ATA block device private data
struct ata_block_private {
    struct ata_device* ata_dev;
};

// ATA block device operations
static bool ata_block_read(void* dev, uint64_t lba, uint32_t count, void* buffer) {
    struct ata_block_private* priv = (struct ata_block_private*)dev;
    
    // Check if LBA is too large for 28-bit addressing
    if (lba > 0x0FFFFFFF) {
        kprintf(ERROR, "ATA block read: LBA too large for 28-bit addressing\n");
        return false;
    }
    
    // Check if count is too large
    if (count > 256) {
        kprintf(ERROR, "ATA block read: Sector count too large\n");
        return false;
    }
    
    return ata_read_sectors(priv->ata_dev, lba, count, buffer);
}

static bool ata_block_write(void* dev, uint64_t lba, uint32_t count, const void* buffer) {
    struct ata_block_private* priv = (struct ata_block_private*)dev;
    return ata_write_sectors(priv->ata_dev, lba, count, buffer);
}

static uint32_t ata_block_get_sector_size(void* dev) {
    struct ata_block_private* priv = (struct ata_block_private*)dev;
    return priv->ata_dev->sector_size;
}

static uint64_t ata_block_get_sector_count(void* dev) {
    struct ata_block_private* priv = (struct ata_block_private*)dev;
    return priv->ata_dev->sectors;
}

static const struct block_device_ops ata_block_ops = {
    .read = ata_block_read,
    .write = ata_block_write,
    .get_sector_size = ata_block_get_sector_size,
    .get_sector_count = ata_block_get_sector_count,
};

// Initialize ATA block devices
void ata_block_init(void) {
    // Initialize primary bus devices
    struct ata_device* primary_master = ata_get_device(0, 0);
    if (primary_master && primary_master->exists) {
        struct block_device* dev = kmalloc(sizeof(struct block_device));
        struct ata_block_private* priv = kmalloc(sizeof(struct ata_block_private));
        
        priv->ata_dev = primary_master;
        dev->name = "ata0";
        dev->private_data = priv;
        dev->ops = &ata_block_ops;
        dev->mounted = false;
        
        block_device_register(dev);
    }

    struct ata_device* primary_slave = ata_get_device(0, 1);
    if (primary_slave && primary_slave->exists) {
        struct block_device* dev = kmalloc(sizeof(struct block_device));
        struct ata_block_private* priv = kmalloc(sizeof(struct ata_block_private));
        
        priv->ata_dev = primary_slave;
        dev->name = "ata1";
        dev->private_data = priv;
        dev->ops = &ata_block_ops;
        dev->mounted = false;
        
        block_device_register(dev);
    }

    // Initialize secondary bus devices
    struct ata_device* secondary_master = ata_get_device(1, 0);
    if (secondary_master && secondary_master->exists) {
        struct block_device* dev = kmalloc(sizeof(struct block_device));
        struct ata_block_private* priv = kmalloc(sizeof(struct ata_block_private));
        
        priv->ata_dev = secondary_master;
        dev->name = "ata2";
        dev->private_data = priv;
        dev->ops = &ata_block_ops;
        dev->mounted = false;
        
        block_device_register(dev);
    }

    struct ata_device* secondary_slave = ata_get_device(1, 1);
    if (secondary_slave && secondary_slave->exists) {
        struct block_device* dev = kmalloc(sizeof(struct block_device));
        struct ata_block_private* priv = kmalloc(sizeof(struct ata_block_private));
        
        priv->ata_dev = secondary_slave;
        dev->name = "ata3";
        dev->private_data = priv;
        dev->ops = &ata_block_ops;
        dev->mounted = false;
        
        block_device_register(dev);
    }
} 