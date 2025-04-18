#include "kernel/fs/vfs.h"
#include "kernel/fs/memfs.h"
#include "kernel/mm/kmalloc.h"
#include "kernel/sync.h"
#include "string.h"

// Current working directory
static struct vfs_node* current_dir = NULL;

// VFS mutex for protecting operations
static mutex_t vfs_mutex;

// Initialize VFS
void vfs_init(void) {
    // Initialize mutex
    mutex_init(&vfs_mutex, "vfs_mutex");
    
    // Initialize memory filesystem
    memfs_init();
    
    // Set root directory
    current_dir = memfs_get_root();
}

// Create a new VFS node
struct vfs_node* vfs_create_node(const char* name, uint32_t flags) {
    // Get parent directory first without holding mutex
    struct vfs_node *parent = vfs_getcwd();
    if (!parent) return NULL;
    
    // Create the node
    struct vfs_node *node = memfs_create_node(name, flags);
    if (!node) return NULL;
    
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
        struct memfs_node* mem_node = (struct memfs_node*)node->impl;
        if (mem_node->data) {
            kfree(mem_node->data);
        }
        kfree(mem_node);
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
    current = (*path == '/') ? memfs_get_root() : current_dir;
    mutex_release(&vfs_mutex);
    
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
    if (!node || !node->readdir) return NULL;
    
    // Only hold mutex for the actual readdir operation
    mutex_acquire(&vfs_mutex);
    struct vfs_node* result = node->readdir(node, index);
    mutex_release(&vfs_mutex);
    
    return result;
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
    struct vfs_node* new_dir = vfs_open(path, 0);
    if (!new_dir || new_dir->flags != FS_DIRECTORY) {
        return false;
    }
    
    // Only hold mutex for the actual directory change
    mutex_acquire(&vfs_mutex);
    current_dir = new_dir;
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
