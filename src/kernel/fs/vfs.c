#include "kernel/fs/vfs.h"
#include "kernel/fs/memfs.h"
#include "kernel/mm/kmalloc.h"
#include "string.h"

// Current working directory
static struct vfs_node* current_dir = NULL;

// Initialize VFS
void vfs_init(void) {
    // Initialize memory filesystem
    memfs_init();
    
    // Set root directory
    current_dir = memfs_get_root();
}

// Create a new VFS node
struct vfs_node* vfs_create_node(const char* name, uint32_t flags) {
    return memfs_create_node(name, flags);
}

// Destroy a VFS node
void vfs_destroy_node(struct vfs_node* node) {
    if (!node) return;
    
    // Free implementation-specific data
    if (node->impl) {
        struct memfs_node* mem_node = (struct memfs_node*)node->impl;
        if (mem_node->data) {
            kfree(mem_node->data);
        }
        kfree(mem_node);
    }
    
    // Free the node itself
    kfree(node);
}

// Mount a filesystem at a given path
struct vfs_node* vfs_mount(const char* path, struct vfs_node* node) {
    (void)path; (void)node;
    // TODO: Implement mounting
    return NULL;
}

// Open a file
struct vfs_node* vfs_open(const char* path, uint32_t flags) {
    (void)flags;
    if (!path || !*path) return NULL;
    
    // Handle absolute paths
    struct vfs_node* current = (*path == '/') ? memfs_get_root() : current_dir;

    // Skip leading slash
    if (*path == '/') path++;
    
    // Traverse the path
    char* path_copy = strdup(path);
    char* token = strtok(path_copy, "/");
    
    while (token && token[0] != '\0') {
        current = current->finddir(current, token);
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
}

// Read from a file
uint32_t vfs_read(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !node->read) return 0;
    return node->read(node, offset, size, buffer);
}

// Write to a file
uint32_t vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !node->write) return 0;
    return node->write(node, offset, size, buffer);
}

// Read directory entry
struct vfs_node* vfs_readdir(struct vfs_node* node, uint32_t index) {
    if (!node || !node->readdir) return NULL;
    return node->readdir(node, index);
}

// Find directory entry
struct vfs_node* vfs_finddir(struct vfs_node* node, const char* name) {
    if (!node || !node->finddir) return NULL;
    return node->finddir(node, name);
}

// Change current directory
bool vfs_chdir(const char* path) {
    struct vfs_node* new_dir = vfs_open(path, 0);
    if (!new_dir || new_dir->flags != FS_DIRECTORY) {
        return false;
    }
    
    current_dir = new_dir;
    return true;
}

// Get current directory
struct vfs_node* vfs_getcwd(void) {
    return current_dir;
} 
