#pragma once

#include "stdint.h"
#include "stdbool.h"
#include "stddef.h"

// File types
#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04
#define FS_PIPE        0x05
#define FS_SYMLINK     0x06
#define FS_MOUNTPOINT  0x08

// File open modes
#define FS_READ        0x01
#define FS_WRITE       0x02
#define FS_APPEND      0x04
#define FS_CREATE      0x08

// File descriptor structure
struct file_descriptor {
    uint32_t inode;        // Inode number
    uint32_t position;     // Current position in file
    uint32_t flags;        // File flags
    struct vfs_node* node; // Pointer to the VFS node
};

// VFS node structure
struct vfs_node {
    char name[128];        // Filename
    uint32_t mask;         // Permission mask
    uint32_t uid;          // User ID
    uint32_t gid;          // Group ID
    uint32_t flags;        // File type flags
    uint32_t inode;        // Inode number
    uint32_t length;       // File size
    void* impl;         // Implementation specific data
    
    // File operations
    uint32_t (*read)(struct vfs_node*, uint32_t, uint32_t, uint8_t*);
    uint32_t (*write)(struct vfs_node*, uint32_t, uint32_t, uint8_t*);
    void (*open)(struct vfs_node*);
    void (*close)(struct vfs_node*);
    struct vfs_node* (*readdir)(struct vfs_node*, uint32_t);
    struct vfs_node* (*finddir)(struct vfs_node*, const char* name);
    
    struct vfs_node* parent;    // Parent directory
    struct vfs_node* children;  // Child nodes
    struct vfs_node* next;      // Next sibling
};

// Filesystem operations
void vfs_init(void);
void vfs_shutdown(void);
struct vfs_node* vfs_create_node(const char* name, uint32_t flags);
void vfs_destroy_node(struct vfs_node* node);
struct vfs_node* vfs_mount(const char* path, struct vfs_node* node);
struct vfs_node* vfs_open(const char* path, uint32_t flags);
void vfs_close(struct vfs_node* node);
uint32_t vfs_read(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
struct vfs_node* vfs_readdir(struct vfs_node* node, uint32_t index);
struct vfs_node* vfs_finddir(struct vfs_node* node, const char* name);
bool vfs_chdir(const char* path);
struct vfs_node* vfs_getcwd(void); 