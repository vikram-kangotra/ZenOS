#include "drivers/ata.h"
#include "kernel/kprintf.h"
#include "arch/x86_64/io.h"
#include <string.h>

// Global ATA devices
static struct ata_device devices[4];  // Primary master, primary slave, secondary master, secondary slave

// Wait for drive to be ready
static bool ata_wait_ready(struct ata_device* dev) {
    uint8_t status;
    int timeout = 100000;  // Add timeout to prevent infinite loop
    
    do {
        status = inb(dev->base_port + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) {
            uint8_t error = inb(dev->base_port + ATA_REG_ERROR);
            kprintf(ERROR, "ATA Error: 0x%x (Status: 0x%x)\n", error, status);
            if (error & ATA_ER_ABRT) kprintf(ERROR, "  Command aborted\n");
            if (error & ATA_ER_IDNF) kprintf(ERROR, "  Sector not found\n");
            if (error & ATA_ER_UNC) kprintf(ERROR, "  Uncorrectable data error\n");
            return false;
        }
        timeout--;
    } while ((status & ATA_SR_BSY) && timeout > 0);
    
    if (timeout == 0) {
        kprintf(ERROR, "ATA Timeout waiting for drive\n");
        return false;
    }
    
    return true;
}

// Wait for data request
static bool ata_wait_data(struct ata_device* dev) {
    uint8_t status;
    do {
        status = inb(dev->base_port + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) {
            uint8_t error = inb(dev->base_port + ATA_REG_ERROR);
            kprintf(ERROR, "ATA Error: 0x%x (Status: 0x%x)\n", error, status);
            if (error & ATA_ER_ABRT) kprintf(ERROR, "  Command aborted\n");
            if (error & ATA_ER_IDNF) kprintf(ERROR, "  Sector not found\n");
            if (error & ATA_ER_UNC) kprintf(ERROR, "  Uncorrectable data error\n");
            return false;
        }
    } while (!(status & ATA_SR_DRQ));
    return true;
}

// Initialize ATA device
static bool ata_init_device(struct ata_device* dev, uint16_t base_port, uint16_t control_port, bool is_master) {
    dev->base_port = base_port;
    dev->control_port = control_port;
    dev->is_master = is_master;
    dev->exists = false;

    // Reset controller
    outb(dev->control_port, 0x04);  // Set SRST bit
    for (int i = 0; i < 1000; i++) inb(dev->control_port);  // Wait
    outb(dev->control_port, 0x00);  // Clear SRST bit
    for (int i = 0; i < 1000; i++) inb(dev->control_port);  // Wait

    // Select drive
    outb(dev->base_port + ATA_REG_DRIVE, is_master ? ATA_DRIVE_MASTER : ATA_DRIVE_SLAVE);
    
    // Wait for drive to be ready
    if (!ata_wait_ready(dev)) {
        return false;
    }

    // Send IDENTIFY command
    outb(dev->base_port + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    
    // Check if drive exists
    uint8_t status = inb(dev->base_port + ATA_REG_STATUS);
    if (status == 0) {
        return false;
    }

    // Wait for data
    if (!ata_wait_data(dev)) {
        return false;
    }

    // Read identify data
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(dev->base_port + ATA_REG_DATA);
    }

    // Extract information
    uint32_t sectors;
    memcpy(&sectors, &identify_data[60], sizeof(uint32_t));
    dev->sectors = sectors;
    dev->sector_size = 512;  // Standard sector size

    // Extract strings
    for (int i = 0; i < 20; i++) {
        dev->model[i*2] = identify_data[27+i] >> 8;
        dev->model[i*2+1] = identify_data[27+i] & 0xFF;
    }
    dev->model[40] = '\0';

    for (int i = 0; i < 10; i++) {
        dev->serial[i*2] = identify_data[10+i] >> 8;
        dev->serial[i*2+1] = identify_data[10+i] & 0xFF;
    }
    dev->serial[20] = '\0';

    for (int i = 0; i < 4; i++) {
        dev->firmware[i*2] = identify_data[23+i] >> 8;
        dev->firmware[i*2+1] = identify_data[23+i] & 0xFF;
    }
    dev->firmware[8] = '\0';

    dev->exists = true;
    return true;
}

// Initialize ATA controller
bool ata_init(void) {
    kprintf(INFO, "Initializing ATA controller...\n");
    bool found = false;

    // Initialize primary bus
    if (ata_init_device(&devices[0], ATA_PRIMARY_BASE, ATA_PRIMARY_CONTROL, true)) {
        kprintf(INFO, "Primary Master: %s (Serial: %s, Firmware: %s)\n", 
                devices[0].model, devices[0].serial, devices[0].firmware);
        found = true;
    }
    if (ata_init_device(&devices[1], ATA_PRIMARY_BASE, ATA_PRIMARY_CONTROL, false)) {
        kprintf(INFO, "Primary Slave: %s (Serial: %s, Firmware: %s)\n", 
                devices[1].model, devices[1].serial, devices[1].firmware);
        found = true;
    }

    // Initialize secondary bus
    if (ata_init_device(&devices[2], ATA_SECONDARY_BASE, ATA_SECONDARY_CONTROL, true)) {
        kprintf(INFO, "Secondary Master: %s (Serial: %s, Firmware: %s)\n", 
                devices[2].model, devices[2].serial, devices[2].firmware);
        found = true;
    }
    if (ata_init_device(&devices[3], ATA_SECONDARY_BASE, ATA_SECONDARY_CONTROL, false)) {
        kprintf(INFO, "Secondary Slave: %s (Serial: %s, Firmware: %s)\n", 
                devices[3].model, devices[3].serial, devices[3].firmware);
        found = true;
    }

    return found;
}

// Read sectors from ATA device
bool ata_read_sectors(struct ata_device* dev, uint32_t lba, uint8_t count, void* buffer) {
    if (!dev || !dev->exists || !buffer || count == 0) {
        kprintf(ERROR, "ATA read: Invalid parameters\n");
        return false;
    }

    // Wait for drive to be ready
    if (!ata_wait_ready(dev)) {
        kprintf(ERROR, "ATA read: Drive not ready\n");
        return false;
    }

    // Select drive and set LBA mode
    uint8_t drive_select = (dev->is_master ? ATA_DRIVE_MASTER : ATA_DRIVE_SLAVE) | ATA_DRIVE_LBA;
    outb(dev->base_port + ATA_REG_DRIVE, drive_select);
    
    // Wait for drive to be ready again after selection
    if (!ata_wait_ready(dev)) {
        kprintf(ERROR, "ATA read: Drive not ready after selection\n");
        return false;
    }

    // Send sector count
    outb(dev->base_port + ATA_REG_SECTOR_COUNT, count);
    
    // Send LBA
    outb(dev->base_port + ATA_REG_LBA_LOW, lba & 0xFF);
    outb(dev->base_port + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(dev->base_port + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(dev->base_port + ATA_REG_DRIVE, drive_select | ((lba >> 24) & 0x0F));
    
    // Send READ command
    outb(dev->base_port + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
    
    uint8_t* buf = (uint8_t*)buffer;
    for (uint8_t i = 0; i < count; i++) {
        // Wait for data
        if (!ata_wait_data(dev)) {
            kprintf(ERROR, "ATA read: No data ready for sector %d\n", i);
            return false;
        }
        // Read sector into a local uint16_t buffer
        uint16_t sector_buf[256];
        for (int j = 0; j < 256; j++) {
            sector_buf[j] = inw(dev->base_port + ATA_REG_DATA);
        }
        // Copy to output buffer as bytes
        memcpy(buf, sector_buf, 512);
        buf += 512;
    }
    return true;
}

// Write sectors to ATA device
bool ata_write_sectors(struct ata_device* dev, uint32_t lba, uint8_t count, const void* buffer) {
    if (!dev->exists) {
        kprintf(ERROR, "ATA device does not exist\n");
        return false;
    }

    // Wait for drive to be ready
    if (!ata_wait_ready(dev)) {
        return false;
    }

    // Select drive and set LBA mode
    uint8_t drive_select = (dev->is_master ? ATA_DRIVE_MASTER : ATA_DRIVE_SLAVE) | ATA_DRIVE_LBA;
    outb(dev->base_port + ATA_REG_DRIVE, drive_select);
    io_wait();  // Add delay after drive selection
    
    // Wait for drive to be ready again after selection
    if (!ata_wait_ready(dev)) {
        kprintf(ERROR, "ATA write: Drive not ready after selection\n");
        return false;
    }

    // Set sector count
    outb(dev->base_port + ATA_REG_SECTOR_COUNT, count);

    // Set LBA
    outb(dev->base_port + ATA_REG_LBA_LOW, lba & 0xFF);
    outb(dev->base_port + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(dev->base_port + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(dev->base_port + ATA_REG_DRIVE, drive_select | ((lba >> 24) & 0x0F));

    // Send write command
    outb(dev->base_port + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);

    const uint8_t* buf = (const uint8_t*)buffer;
    for (uint8_t i = 0; i < count; i++) {
        if (!ata_wait_data(dev)) {
            kprintf(ERROR, "ATA write: No data ready for sector %d\n", i);
            return false;
        }
        // Copy from input buffer to a local uint16_t buffer
        uint16_t sector_buf[256];
        memcpy(sector_buf, buf, 512);
        for (int j = 0; j < 256; j++) {
            outw(dev->base_port + ATA_REG_DATA, sector_buf[j]);
        }
        buf += 512;
    }

    // Flush the drive's cache
    if (!ata_wait_ready(dev)) {
        kprintf(ERROR, "ATA write: Drive not ready for flush\n");
        return false;
    }
    outb(dev->base_port + ATA_REG_COMMAND, ATA_CMD_FLUSH_CACHE);
    if (!ata_wait_ready(dev)) {
        kprintf(ERROR, "ATA write: Flush failed\n");
        return false;
    }

    return true;
}

// Get ATA device
struct ata_device* ata_get_device(uint8_t bus, uint8_t drive) {
    if (bus > 1 || drive > 1) {
        return NULL;
    }
    return &devices[bus * 2 + drive];
} 