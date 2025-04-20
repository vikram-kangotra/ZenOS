#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <stdbool.h>

// ATA Commands
#define ATA_CMD_READ_SECTORS    0x20
#define ATA_CMD_WRITE_SECTORS   0x30
#define ATA_CMD_IDENTIFY       0xEC

// ATA Status Register Bits
#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRDY    0x40    // Drive ready
#define ATA_SR_DF      0x20    // Drive write fault
#define ATA_SR_DSC     0x10    // Drive seek complete
#define ATA_SR_DRQ     0x08    // Data request ready
#define ATA_SR_CORR    0x04    // Corrected data
#define ATA_SR_IDX     0x02    // Index
#define ATA_SR_ERR     0x01    // Error

// ATA Error Register Bits
#define ATA_ER_BBK     0x80    // Bad block
#define ATA_ER_UNC     0x40    // Uncorrectable data
#define ATA_ER_MC      0x20    // Media changed
#define ATA_ER_IDNF    0x10    // ID not found
#define ATA_ER_MCR     0x08    // Media change request
#define ATA_ER_ABRT    0x04    // Command aborted
#define ATA_ER_TK0NF   0x02    // Track 0 not found
#define ATA_ER_AMNF    0x01    // No address mark

// ATA Ports
#define ATA_PRIMARY_BASE       0x1F0
#define ATA_PRIMARY_CONTROL    0x3F6
#define ATA_SECONDARY_BASE     0x170
#define ATA_SECONDARY_CONTROL  0x376

// ATA Register Offsets
#define ATA_REG_DATA          0x00
#define ATA_REG_ERROR         0x01
#define ATA_REG_FEATURES      0x01
#define ATA_REG_SECTOR_COUNT  0x02
#define ATA_REG_LBA_LOW       0x03
#define ATA_REG_LBA_MID       0x04
#define ATA_REG_LBA_HIGH      0x05
#define ATA_REG_DRIVE         0x06
#define ATA_REG_STATUS        0x07
#define ATA_REG_COMMAND       0x07

// Drive Selection
#define ATA_DRIVE_MASTER      0xA0
#define ATA_DRIVE_SLAVE       0xB0
#define ATA_DRIVE_LBA         0x40    // LBA mode bit

// ATA Device Structure
struct ata_device {
    bool exists;
    bool is_master;
    uint16_t base_port;
    uint16_t control_port;
    uint32_t sectors;
    uint32_t sector_size;
    char model[41];
    char serial[21];
    char firmware[9];
};

// Function Declarations
void ata_init(void);
bool ata_read_sectors(struct ata_device* dev, uint32_t lba, uint8_t count, void* buffer);
bool ata_write_sectors(struct ata_device* dev, uint32_t lba, uint8_t count, const void* buffer);
struct ata_device* ata_get_device(uint8_t bus, uint8_t drive);

#endif // ATA_H 