#pragma once

#include "kernel/fs/vfs.h"

// Memory filesystem node structure
struct memfs_node {
    uint8_t* data;         // File data
    uint32_t capacity;     // Allocated capacity
    struct vfs_node* node; // Associated VFS node
};

// Memory filesystem operations
struct vfs_node* memfs_create_node(const char* name, uint32_t flags);
void memfs_init(void);
struct vfs_node* memfs_get_root(void); 