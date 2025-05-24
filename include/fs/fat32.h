#pragma once

#include "drivers/block.h"
#include "fs/vfs.h"
#include "stdint.h"
#include "stdbool.h"

// FAT32 Boot Sector Structure
struct fat32_boot_sector {
    uint8_t  jump_boot[3];          // Jump instruction to boot code
    uint8_t  oem_name[8];          // OEM Name
    uint16_t bytes_per_sector;     // Bytes per sector
    uint8_t  sectors_per_cluster;  // Sectors per cluster
    uint16_t reserved_sectors;     // Reserved sectors count
    uint8_t  fat_count;           // Number of FATs
    uint16_t root_entry_count;    // Root directory entries (0 for FAT32)
    uint16_t total_sectors_16;    // Total sectors (16-bit) (0 for FAT32)
    uint8_t  media_type;         // Media type
    uint16_t sectors_per_fat_16; // Sectors per FAT (16-bit) (0 for FAT32)
    uint16_t sectors_per_track;  // Sectors per track
    uint16_t head_count;        // Number of heads
    uint32_t hidden_sectors;    // Hidden sectors count
    uint32_t total_sectors_32;  // Total sectors (32-bit)
    uint32_t sectors_per_fat_32; // Sectors per FAT (32-bit)
    uint16_t ext_flags;        // Extended flags
    uint16_t fs_version;       // Filesystem version
    uint32_t root_cluster;     // First cluster of root directory
    uint16_t fs_info;         // Filesystem information sector
    uint16_t backup_boot;     // Backup boot sector
    uint8_t  reserved[12];    // Reserved
    uint8_t  drive_number;    // Drive number
    uint8_t  reserved1;       // Reserved
    uint8_t  boot_signature; // Extended boot signature
    uint32_t volume_id;      // Volume serial number
    uint8_t  volume_label[11]; // Volume label
    uint8_t  fs_type[8];     // Filesystem type
    uint8_t  boot_code[420]; // Boot code
    uint16_t boot_signature2; // Boot signature
} __attribute__((packed));

// FAT32 Directory Entry Structure
struct fat32_dir_entry {
    uint8_t  name[11];           // 8.3 filename
    uint8_t  attributes;         // File attributes
    uint8_t  nt_reserved;        // Reserved for Windows NT
    uint8_t  creation_time_ms;   // Creation time (milliseconds)
    uint16_t creation_time;      // Creation time
    uint16_t creation_date;      // Creation date
    uint16_t last_access_date;   // Last access date
    uint16_t first_cluster_high; // High word of first cluster number
    uint16_t write_time;         // Last write time
    uint16_t write_date;         // Last write date
    uint16_t first_cluster_low;  // Low word of first cluster number
    uint32_t file_size;          // File size in bytes
} __attribute__((packed));

// FAT32 File Structure
struct fat32_file {
    struct block_device* dev;    // Block device
    uint32_t first_cluster;      // First cluster of file/directory
    uint32_t current_cluster;    // Current cluster being read/written
    uint32_t current_sector;     // Current sector within cluster
    uint32_t position;          // Current position in file
    uint32_t size;             // File size
    bool is_directory;         // Whether this is a directory
    void* data;               // Implementation specific data
};

// FAT32 Functions
bool fat32_init(struct block_device* dev);
struct fat32_file* fat32_open(struct block_device* dev, const char* path);
bool fat32_close(struct fat32_file* file);
uint32_t fat32_read(struct fat32_file* file, void* buffer, uint32_t size);
uint32_t fat32_write(struct fat32_file* file, const void* buffer, uint32_t size);
bool fat32_seek(struct fat32_file* file, uint32_t position);
bool fat32_readdir(struct fat32_file* dir, struct fat32_dir_entry* entry);
bool fat32_mkdir(struct block_device* dev, const char* path);
bool fat32_rmdir(struct block_device* dev, const char* path);
bool fat32_unlink(struct block_device* dev, const char* path);
bool fat32_unmount(void);

// VFS Interface Functions
struct vfs_node* fat32_get_root(void);
struct vfs_node* fat32_create_node(const char* name, uint32_t flags);
struct vfs_node* fat32_finddir(struct vfs_node* node, const char* name);

// VFS Interface Functions
void fat32_vfs_open(struct vfs_node* node);
void fat32_vfs_close(struct vfs_node* node);
uint32_t fat32_vfs_read(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t fat32_vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
struct vfs_node* fat32_vfs_readdir(struct vfs_node* node, uint32_t index);
struct vfs_node* fat32_vfs_finddir(struct vfs_node* node, const char* name); 