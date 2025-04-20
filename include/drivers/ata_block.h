#ifndef ATA_BLOCK_H
#define ATA_BLOCK_H

#include "drivers/ata.h"
#include "drivers/block.h"

// Initialize ATA block devices
void ata_block_init(void);

// ATA block device operations
extern const struct block_device_ops ata_block_ops;

#endif // ATA_BLOCK_H 