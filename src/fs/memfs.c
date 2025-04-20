#include "fs/vfs.h"
#include "kernel/mm/kmalloc.h"
#include "string.h"

// Memory filesystem node structure
struct memfs_node {
    uint8_t* data;         // File data
    uint32_t capacity;     // Allocated capacity
    struct vfs_node* node; // Associated VFS node
};

// Root directory node
static struct vfs_node* root_node = NULL;

// Memory filesystem operations
static uint32_t memfs_read(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    struct memfs_node* mem_node = (struct memfs_node*)node->impl;
    if (!mem_node || !mem_node->data) return 0;
    
    if (offset >= node->length) return 0;
    if (offset + size > node->length) size = node->length - offset;
    
    memcpy(buffer, mem_node->data + offset, size);
    return size;
}

static uint32_t memfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    struct memfs_node* mem_node = (struct memfs_node*)node->impl;
    if (!mem_node) return 0;
    
    // Resize if needed
    if (offset + size > mem_node->capacity) {
        uint32_t new_capacity = (offset + size + 4095) & ~4095; // Align to 4KB
        uint8_t* new_data = kmalloc(new_capacity);
        if (!new_data) return 0;
        
        if (mem_node->data) {
            memcpy(new_data, mem_node->data, node->length);
            kfree(mem_node->data);
        }
        
        mem_node->data = new_data;
        mem_node->capacity = new_capacity;
    }
    
    memcpy(mem_node->data + offset, buffer, size);
    if (offset + size > node->length) {
        node->length = offset + size;
    }
    return size;
}

static void memfs_open(struct vfs_node* node) {
    (void)node;
    // Nothing to do for memory filesystem
}

static void memfs_close(struct vfs_node* node) {
    (void)node;
    // Nothing to do for memory filesystem
}

static struct vfs_node* memfs_readdir(struct vfs_node* node, uint32_t index) {
    if (node->flags != FS_DIRECTORY) return NULL;
    
    struct vfs_node* child = node->children;
    for (uint32_t i = 0; i < index && child; i++) {
        child = child->next;
    }
    return child;
}

static struct vfs_node* memfs_finddir(struct vfs_node* node, const char* name) {
    if (node->flags != FS_DIRECTORY) return NULL;
    
    struct vfs_node* child = node->children;
    while (child) {
        if (strcmp(child->name, name) == 0) return child;
        child = child->next;
    }
    return NULL;
}

// Create a new memory filesystem node
struct vfs_node* memfs_create_node(const char* name, uint32_t flags) {
    struct vfs_node* node = kmalloc(sizeof(struct vfs_node));
    struct memfs_node* mem_node = kmalloc(sizeof(struct memfs_node));
    
    if (!node || !mem_node) {
        if (node) kfree(node);
        if (mem_node) kfree(mem_node);
        return NULL;
    }
    
    memset(node, 0, sizeof(struct vfs_node));
    memset(mem_node, 0, sizeof(struct memfs_node));
    
    strncpy(node->name, name, sizeof(node->name) - 1);
    node->flags = flags;
    node->impl = (void*)mem_node;
    mem_node->node = node;
    
    // Set up operations
    node->read = memfs_read;
    node->write = memfs_write;
    node->open = memfs_open;
    node->close = memfs_close;
    node->readdir = memfs_readdir;
    node->finddir = memfs_finddir;
    
    return node;
}

// Initialize memory filesystem
void memfs_init(void) {
    root_node = memfs_create_node("", FS_DIRECTORY);
    if (!root_node) return;
    
    // Create some initial directories
    struct vfs_node* dev = memfs_create_node("dev", FS_DIRECTORY);
    struct vfs_node* proc = memfs_create_node("proc", FS_DIRECTORY);
    
    if (dev) {
        dev->parent = root_node;
        dev->next = root_node->children;
        root_node->children = dev;
    }
    
    if (proc) {
        proc->parent = root_node;
        proc->next = root_node->children;
        root_node->children = proc;
    }
}

// Get root node
struct vfs_node* memfs_get_root(void) {
    return root_node;
} 
