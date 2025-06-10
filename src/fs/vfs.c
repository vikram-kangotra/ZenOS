#include "fs/vfs.h"
#include "drivers/ata.h"
#include "drivers/block.h"
#include "drivers/ata_block.h"
#include "fs/fat32.h"
#include "kernel/mm/kmalloc.h"
#include "kernel/sync.h"
#include "kernel/kprintf.h"
#include "string.h"

// Current working directory
static struct vfs_node* current_dir = NULL;

// VFS mutex for protecting operations
static mutex_t vfs_mutex;

// Initialize VFS
void vfs_init(void) {
    // Initialize mutex
    mutex_init(&vfs_mutex, "vfs_mutex");
    
    // Initialize ATA driver and block devices
    ata_init();
    ata_block_init();
    
    // Get the first ATA block device
    struct block_device* blk_dev = block_device_get("ata0");
    if (!blk_dev) {
        kprintf(ERROR, "No ATA block device found\n");
        return;
    }
    
    // Initialize FAT32 filesystem
    if (!fat32_init(blk_dev)) {
        kprintf(ERROR, "Failed to initialize FAT32 filesystem\n");
        return;
    }
    
    // Get root directory
    current_dir = fat32_get_root();
    if (!current_dir) {
        kprintf(ERROR, "Failed to get root directory\n");
        return;
    }
    
    // Open root directory
    struct fat32_file* root_file = fat32_open(blk_dev, "/");
    if (!root_file) {
        kprintf(ERROR, "Failed to open root directory\n");
        vfs_destroy_node(current_dir);
        current_dir = NULL;
        return;
    }
    
    // Set implementation and operations
    current_dir->impl = root_file;
    current_dir->open = fat32_vfs_open;
    current_dir->close = fat32_vfs_close;
    current_dir->read = fat32_vfs_read;
    current_dir->write = fat32_vfs_write;
    current_dir->readdir = fat32_vfs_readdir;
    current_dir->finddir = fat32_vfs_finddir;
    
    kprintf(INFO, "FAT32 filesystem mounted successfully\n");
    // Defensive: Check current_dir and impl
    if (!current_dir || !(current_dir->flags & FS_DIRECTORY) || !current_dir->impl) {
        kprintf(ERROR, "vfs_init: current_dir or its impl is not a valid directory after init!\n");
    }
}

// Create a new VFS node
struct vfs_node* vfs_create_node(const char* name, uint32_t flags) {
    if (!name || !*name) {
        kprintf(ERROR, "vfs_create_node: Invalid name\n");
        return NULL;
    }
    
    // Get parent directory first without holding mutex
    struct vfs_node *parent = vfs_getcwd();
    if (!parent) {
        kprintf(ERROR, "vfs_create_node: No current directory\n");
        return NULL;
    }
    
    // Create the node
    struct vfs_node *node = kmalloc(sizeof(struct vfs_node));
    if (!node) {
        kprintf(ERROR, "vfs_create_node: Failed to allocate node\n");
        return NULL;
    }
    
    memset(node, 0, sizeof(struct vfs_node));
    strncpy(node->name, name, 127);
    node->name[127] = '\0';
    node->flags = flags;
    
    // Set up VFS operations
    node->open = fat32_vfs_open;
    node->close = fat32_vfs_close;
    node->read = fat32_vfs_read;
    node->write = fat32_vfs_write;
    node->readdir = fat32_vfs_readdir;
    node->finddir = fat32_vfs_finddir;
    
    // Only hold mutex for the actual linking operation
    mutex_acquire(&vfs_mutex);
    node->parent = parent;
    node->next = parent->children;
    parent->children = node;
    mutex_release(&vfs_mutex);
    
    return node;
}

// Destroy a VFS node
void vfs_destroy_node(struct vfs_node* node) {
    if (!node) return;
    
    // Free implementation-specific data first
    if (node->impl) {
        struct fat32_file* fat32_node = (struct fat32_file*)node->impl;
        if (fat32_node->data) {
            kfree(fat32_node->data);
        }
        kfree(fat32_node);
    }
    
    // Only hold mutex for the actual node removal
    mutex_acquire(&vfs_mutex);
    kfree(node);
    mutex_release(&vfs_mutex);
}

// Mount a filesystem at a given path
struct vfs_node* vfs_mount(const char* path, struct vfs_node* node) {
    (void)path; (void)node;
    // TODO: Implement mounting
    return NULL;
}

// Open a file
struct vfs_node* vfs_open(const char* path, uint32_t flags) {
    (void) flags;
    if (!path || !*path) return NULL;
    
    // Get starting directory without holding mutex
    struct vfs_node* current;
    mutex_acquire(&vfs_mutex);
    current = (*path == '/') ? fat32_get_root() : current_dir;
    mutex_release(&vfs_mutex);
    
    // Defensive: Ensure root node is valid and a directory
    if (!current) {
        kprintf(ERROR, "vfs_open: Root/current directory node is NULL!\n");
        return NULL;
    }
    if (!(current->flags & FS_DIRECTORY) || !current->impl) {
        kprintf(ERROR, "vfs_open: Root/current directory is not a valid directory or has no impl!\n");
        return NULL;
    }
    
    // Skip leading slash
    if (*path == '/') path++;
    
    // Traverse the path
    char* path_copy = strdup(path);
    char* token = strtok(path_copy, "/");
    
    while (token && token[0] != '\0') {
        mutex_acquire(&vfs_mutex);
        current = current->finddir(current, token);
        mutex_release(&vfs_mutex);
        
        if (!current) {
            kfree(path_copy);
            return NULL;
        }
        token = strtok(NULL, "/");
    }
    
    kfree(path_copy);
    
    // Open the node
    if (current->open) {
        current->open(current);
    }
    
    return current;
}

// Close a file
void vfs_close(struct vfs_node* node) {
    if (!node) return;
    
    if (node->close) {
        node->close(node);
    }

    // If this is a file (not a directory), sync the device
    if (node->flags == FS_FILE) {
        struct block_device* dev = ((struct fat32_file*)node->impl)->dev;
        if (dev) {
            block_device_sync(dev);
        }
    }
}

// Read from a file
uint32_t vfs_read(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !node->read) return 0;
    
    // Only hold mutex for the actual read operation
    mutex_acquire(&vfs_mutex);
    uint32_t result = node->read(node, offset, size, buffer);
    mutex_release(&vfs_mutex);
    
    return result;
}

// Write to a file
uint32_t vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !node->write) return 0;
    
    // Only hold mutex for the actual write operation
    mutex_acquire(&vfs_mutex);
    uint32_t result = node->write(node, offset, size, buffer);
    mutex_release(&vfs_mutex);
    
    return result;
}

// Read directory entry
struct vfs_node* vfs_readdir(struct vfs_node* node, uint32_t index) {
    if (!node || !(node->flags & FS_DIRECTORY)) {
        return NULL;
    }
    
    // Let the filesystem implementation handle the indexing
    return node->readdir(node, index);
}

// Find directory entry
struct vfs_node* vfs_finddir(struct vfs_node* node, const char* name) {
    if (!node || !node->finddir) return NULL;
    
    // Only hold mutex for the actual finddir operation
    mutex_acquire(&vfs_mutex);
    struct vfs_node* result = node->finddir(node, name);
    mutex_release(&vfs_mutex);
    
    return result;
}

// Change current directory
bool vfs_chdir(const char* path) {
    if (!path || !*path) {
        kprintf(ERROR, "vfs_chdir: Invalid path\n");
        return false;
    }

    // Get starting directory without holding mutex
    struct vfs_node* current;
    mutex_acquire(&vfs_mutex);
    current = (*path == '/') ? fat32_get_root() : current_dir;
    mutex_release(&vfs_mutex);
    
    if (!current) {
        kprintf(ERROR, "vfs_chdir: Failed to get starting directory\n");
        return false;
    }

    // Skip leading slash
    const char* path_ptr = path;
    if (*path_ptr == '/') path_ptr++;

    // Traverse the path
    char* path_copy = strdup(path_ptr);
    if (!path_copy) {
        kprintf(ERROR, "vfs_chdir: Failed to allocate path copy\n");
        return false;
    }

    char* token = strtok(path_copy, "/");
    
    while (token) {
        if (!*token) {  // Skip empty tokens
            token = strtok(NULL, "/");
            continue;
        }
        
        mutex_acquire(&vfs_mutex);
        struct vfs_node* next = current->finddir(current, token);
        mutex_release(&vfs_mutex);
        
        if (!next) {
            kprintf(ERROR, "vfs_chdir: Component not found: %s\n", token);
            if (current != current_dir) {
                vfs_destroy_node(current); 
            }
            kfree(path_copy);
            return false;
        }
        
        if (!(next->flags & FS_DIRECTORY)) {
            kprintf(ERROR, "vfs_chdir: Not a directory: %s\n", token);
            vfs_destroy_node(next);
            vfs_destroy_node(current);
            kfree(path_copy);
            return false;
        }
        
        // Open the directory
        if (next->open) {
            next->open(next);
        }
        
        vfs_destroy_node(current);
        current = next;
        token = strtok(NULL, "/");
    }
    
    kfree(path_copy);
    
    // Only hold mutex for the actual directory change
    mutex_acquire(&vfs_mutex);
    current_dir = current;
    mutex_release(&vfs_mutex);
    
    return true;
}

// Get current directory
struct vfs_node* vfs_getcwd(void) {
    // Only hold mutex for the actual directory access
    mutex_acquire(&vfs_mutex);
    struct vfs_node* result = current_dir;
    mutex_release(&vfs_mutex);
    return result;
}

// Shutdown VFS
void vfs_shutdown(void) {
    kprintf(INFO, "Shutting down VFS...\n");
    
    // Close current directory if it exists
    if (current_dir) {
        if (current_dir->close) {
            current_dir->close(current_dir);
        }
        vfs_destroy_node(current_dir);
        current_dir = NULL;
    }
    
    // Unmount FAT32 filesystem
    if (!fat32_unmount()) {
        kprintf(ERROR, "Failed to unmount FAT32 filesystem\n");
    }
    
    // Destroy VFS mutex
    mutex_release(&vfs_mutex);
    
    kprintf(INFO, "VFS shutdown complete\n");
} 
