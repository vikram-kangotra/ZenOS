#include "fs/fat32.h"
#include "fs/vfs.h"
#include "drivers/block.h"
#include "string.h"
#include "kernel/mm/kmalloc.h"
#include "kernel/kprintf.h"

char toupper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - 32;
    }
    return c;
}

// Private data structure for FAT32 filesystem
struct fat32_private {
    struct block_device* dev;
    struct fat32_boot_sector boot_sector;
    uint32_t fat_start;
    uint32_t data_start;
    uint32_t root_dir_cluster;
    uint32_t bytes_per_cluster;
    uint32_t* fat_cache;
    uint32_t fat_cache_size;
};

// Global filesystem data
static struct fat32_private* fs_private = NULL;
static struct vfs_node* fat32_root_node = NULL;

// Helper functions
static uint32_t cluster_to_lba(struct fat32_private* priv, uint32_t cluster) {
    return priv->data_start + (cluster - 2) * priv->boot_sector.sectors_per_cluster;
}

static uint32_t get_next_cluster(struct fat32_private* priv, uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = priv->fat_start + (fat_offset / priv->boot_sector.bytes_per_sector);
    uint32_t sector_offset = fat_offset % priv->boot_sector.bytes_per_sector;
    
    uint8_t sector_buf[512];
    if (!block_device_read(priv->dev, fat_sector, 1, sector_buf)) {
        kprintf(ERROR, "get_next_cluster: Failed to read FAT sector\n");
        return 0;
    }
    
    uint32_t next_cluster = *(uint32_t*)(sector_buf + sector_offset) & 0x0FFFFFFF;
    return next_cluster;
}

static bool read_cluster(struct fat32_private* priv, uint32_t cluster, void* buffer) {
    if (cluster < 2 || cluster >= 0x0FFFFFF8) {
        kprintf(ERROR, "read_cluster: Invalid cluster number %d\n", cluster);
        return false;
    }

    uint32_t sector = cluster_to_lba(priv, cluster);
    
    if (!block_device_read(priv->dev, sector, priv->boot_sector.sectors_per_cluster, buffer)) {
        kprintf(ERROR, "read_cluster: Failed to read cluster\n");
        return false;
    }
    
    return true;
}

static bool write_cluster(struct fat32_private* priv, uint32_t cluster, const void* buffer) {
    uint32_t sector = cluster_to_lba(priv, cluster);
    return block_device_write(priv->dev, sector, priv->boot_sector.sectors_per_cluster, buffer);
}

// Helper function to convert name to 8.3 format
static void convert_to_83_name(const char* name, char* name83) {
    memset(name83, ' ', 11);
    
    // Find the last dot for extension
    const char* dot = strrchr(name, '.');
    const char* ext = dot ? dot + 1 : NULL;
    size_t name_len = dot ? (size_t)(dot - name) : strlen(name);
    
    // Convert name part (up to 8 chars)
    size_t i;
    for (i = 0; i < name_len && i < 8; i++) {
        char c = toupper((unsigned char)name[i]);
        // Skip invalid characters
        if (c == ' ' || c == '.' || c == '/' || c == '\\' || c == ':' || 
            c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            continue;
        }
        name83[i] = c;
    }
    
    // Convert extension part (up to 3 chars)
    if (ext) {
        for (i = 0; i < 3 && ext[i]; i++) {
            char c = toupper((unsigned char)ext[i]);
            // Skip invalid characters
            if (c == ' ' || c == '.' || c == '/' || c == '\\' || c == ':' || 
                c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                continue;
            }
            name83[8 + i] = c;
        }
    }
}

// Helper to write a FAT sector to all FATs
static bool write_fat_sector_all(struct fat32_private* priv, uint32_t fat_sector_offset, const void* buffer) {
    bool ok = true;
    for (uint8_t fat = 0; fat < priv->boot_sector.fat_count; fat++) {
        uint32_t sector = priv->fat_start + fat * priv->boot_sector.sectors_per_fat_32 + fat_sector_offset;
        if (!block_device_write(priv->dev, sector, 1, buffer)) {
            ok = false;
        }
    }
    return ok;
}

// Initialize FAT32 Filesystem
bool fat32_init(struct block_device* dev) {
    if (!dev) {
        kprintf(ERROR, "Invalid block device\n");
        return false;
    }

    // Allocate private data
    fs_private = kmalloc(sizeof(struct fat32_private));
    if (!fs_private) {
        kprintf(ERROR, "Failed to allocate FAT32 private data\n");
        return false;
    }

    fs_private->dev = dev;

    // Read boot sector
    if (!block_device_read(dev, 0, 1, &fs_private->boot_sector)) {
        kprintf(ERROR, "Failed to read boot sector\n");
        kfree(fs_private);
        return false;
    }

    // Verify FAT32 signature
    if (strncmp((char*)fs_private->boot_sector.fs_type, "FAT32", 5) != 0) {
        kprintf(ERROR, "Not a FAT32 filesystem\n");
        kfree(fs_private);
        return false;
    }

    // Calculate important offsets
    fs_private->fat_start = fs_private->boot_sector.reserved_sectors;
    fs_private->data_start = fs_private->fat_start + 
                           (fs_private->boot_sector.fat_count * fs_private->boot_sector.sectors_per_fat_32);
    fs_private->root_dir_cluster = fs_private->boot_sector.root_cluster;
    fs_private->bytes_per_cluster = fs_private->boot_sector.bytes_per_sector * 
                              fs_private->boot_sector.sectors_per_cluster;

    // Cache FAT
    fs_private->fat_cache_size = fs_private->boot_sector.sectors_per_fat_32 * fs_private->boot_sector.bytes_per_sector;
    fs_private->fat_cache = kmalloc(fs_private->fat_cache_size);
    if (!fs_private->fat_cache) {
        kfree(fs_private);
        return false;
    }
    
    // Read FAT into cache
    for (uint32_t i = 0; i < fs_private->boot_sector.sectors_per_fat_32; i++) {
        if (!block_device_read(dev, fs_private->fat_start + i, 1, 
                             (uint8_t*)fs_private->fat_cache + (i * fs_private->boot_sector.bytes_per_sector))) {
            kfree(fs_private->fat_cache);
            kfree(fs_private);
            return false;
        }
    }

    kprintf(INFO, "FAT32 filesystem initialized:\n");
    kprintf(INFO, "  Volume Label: %s\n", fs_private->boot_sector.volume_label);
    kprintf(INFO, "  Cluster Size: %d bytes\n", fs_private->bytes_per_cluster);
    kprintf(INFO, "  Total Clusters: %d\n", 
            (fs_private->boot_sector.total_sectors_32 - fs_private->data_start) / 
            fs_private->boot_sector.sectors_per_cluster);

    // Create persistent root node and impl
    if (fat32_root_node) {
        // Free previous root node if re-initializing
        if (fat32_root_node->impl) kfree(fat32_root_node->impl);
        kfree(fat32_root_node);
    }
    fat32_root_node = kmalloc(sizeof(struct vfs_node));
    memset(fat32_root_node, 0, sizeof(struct vfs_node));
    strcpy(fat32_root_node->name, "/");
    fat32_root_node->flags = FS_DIRECTORY;
    fat32_root_node->open = fat32_vfs_open;
    fat32_root_node->close = fat32_vfs_close;
    fat32_root_node->read = fat32_vfs_read;
    fat32_root_node->write = fat32_vfs_write;
    fat32_root_node->readdir = fat32_vfs_readdir;
    fat32_root_node->finddir = fat32_vfs_finddir;
    // Set up impl
    struct fat32_file* root_file = kmalloc(sizeof(struct fat32_file));
    memset(root_file, 0, sizeof(struct fat32_file));
    root_file->dev = dev;
    root_file->first_cluster = fs_private->root_dir_cluster;
    root_file->current_cluster = root_file->first_cluster;
    root_file->position = 0;
    root_file->size = 0;
    root_file->is_directory = true;
    fat32_root_node->impl = root_file;

    return true;
}

// Parse path and find file
struct fat32_file* fat32_open(struct block_device* dev, const char* path) {
    if (!fs_private || !path) {
        kprintf(ERROR, "fat32_open: Invalid parameters\n");
        return NULL;
    }

    // Special case for root directory
    if (strcmp(path, "/") == 0) {
        struct fat32_file* root = kmalloc(sizeof(struct fat32_file));
        if (!root) {
            kprintf(ERROR, "fat32_open: Failed to allocate root file\n");
            return NULL;
        }
        
        memset(root, 0, sizeof(struct fat32_file));
        root->dev = dev;
        root->first_cluster = fs_private->root_dir_cluster;
        root->current_cluster = root->first_cluster;
        root->position = 0;
        root->size = 0;
        root->is_directory = true;
        
        return root;
    }

    // Start from root directory
    struct fat32_file* current = kmalloc(sizeof(struct fat32_file));
    if (!current) {
        kprintf(ERROR, "fat32_open: Failed to allocate current file\n");
        return NULL;
    }
    
    memset(current, 0, sizeof(struct fat32_file));
    current->dev = dev;
    current->first_cluster = fs_private->root_dir_cluster;
    current->current_cluster = current->first_cluster;
    current->position = 0;
    current->size = 0;
    current->is_directory = true;

    // Skip leading slash
    const char* path_ptr = path;
    if (*path_ptr == '/') path_ptr++;

    // Traverse path
    char* path_copy = strdup(path_ptr);
    if (!path_copy) {
        kprintf(ERROR, "fat32_open: Failed to allocate path copy\n");
        kfree(current);
        return NULL;
    }
    
    char* token = strtok(path_copy, "/");
    
    while (token) {
        // Convert token to uppercase for comparison
        char* token_upper = strdup(token);
        if (!token_upper) {
            kprintf(ERROR, "fat32_open: Failed to allocate token copy\n");
            fat32_close(current);
            kfree(path_copy);
            return NULL;
        }
        
        for (char* p = token_upper; *p; p++) {
            *p = toupper(*p);
        }
        
        // Reset directory position
        current->position = 0;
        current->current_cluster = current->first_cluster;
        
        struct fat32_dir_entry entry;
        bool found = false;
        
        while (fat32_readdir(current, &entry)) {
            // Convert entry name to uppercase for comparison
            char entry_name[13];
            memset(entry_name, 0, sizeof(entry_name));
            strncpy(entry_name, (char*)entry.name, 8);
            entry_name[8] = '\0';
            
            // Trim trailing spaces
            char* end = entry_name + strlen(entry_name) - 1;
            while (end >= entry_name && *end == ' ') {
                *end-- = '\0';
            }
            
            // Add extension if present
            if (entry.name[8] != ' ') {
                strcat(entry_name, ".");
                strncat(entry_name, (char*)entry.name + 8, 3);
                
                // Trim trailing spaces from extension
                end = entry_name + strlen(entry_name) - 1;
                while (end >= entry_name && *end == ' ') {
                    *end-- = '\0';
                }
            }
            
            // Convert to uppercase
            for (char* p = entry_name; *p; p++) {
                *p = toupper(*p);
            }
            
            if (strcmp(entry_name, token_upper) == 0) {
                found = true;
                break;
            }
        }
        
        kfree(token_upper);
        
        if (!found) {
            kprintf(ERROR, "fat32_open: Component not found: %s\n", token);
            fat32_close(current);
            kfree(path_copy);
            return NULL;
        }
        
        // Create new file structure for the found entry
        struct fat32_file* next = kmalloc(sizeof(struct fat32_file));
        if (!next) {
            kprintf(ERROR, "fat32_open: Failed to allocate next file\n");
            fat32_close(current);
            kfree(path_copy);
            return NULL;
        }
        
        memset(next, 0, sizeof(struct fat32_file));
        next->dev = dev;
        next->first_cluster = entry.first_cluster_low | (entry.first_cluster_high << 16);
        next->current_cluster = next->first_cluster;
        next->position = 0;
        next->size = entry.file_size;
        next->is_directory = (entry.attributes & 0x10) != 0;
        
        fat32_close(current);
        current = next;
        
        token = strtok(NULL, "/");
    }
    
    kfree(path_copy);
    return current;
}

// Close a file
bool fat32_close(struct fat32_file* file) {
    if (!file) {
        return false;
    }

    kfree(file);
    return true;
}

// Read from a file
uint32_t fat32_read(struct fat32_file* file, void* buffer, uint32_t size) {
    if (!file || !buffer || size == 0) {
        kprintf(ERROR, "fat32_read: Invalid parameters\n");
        return 0;
    }

    uint32_t bytes_read = 0;
    uint8_t* buf = (uint8_t*)buffer;
    uint8_t cluster_buffer[fs_private->bytes_per_cluster];

    while (bytes_read < size && file->position < file->size) {
        // Calculate position within cluster
        uint32_t cluster_offset = file->position % fs_private->bytes_per_cluster;
        
        // If at start of cluster, read it
        if (cluster_offset == 0) {
            if (!read_cluster(fs_private, file->current_cluster, cluster_buffer)) {
                kprintf(ERROR, "fat32_read: Failed to read cluster %d\n", file->current_cluster);
                break;
            }
        }

        // Calculate how much to read
        uint32_t to_read = fs_private->bytes_per_cluster - cluster_offset;
        if (to_read > size - bytes_read) {
            to_read = size - bytes_read;
        }
        if (to_read > file->size - file->position) {
            to_read = file->size - file->position;
        }

        // Copy data
        memcpy(buf + bytes_read, cluster_buffer + cluster_offset, to_read);
        bytes_read += to_read;
        file->position += to_read;

        // Move to next cluster if needed
        if (file->position % fs_private->bytes_per_cluster == 0) {
            uint32_t next_cluster = get_next_cluster(fs_private, file->current_cluster);
            if (next_cluster >= 0x0FFFFFF8) {
                break;  // End of file
            }
            file->current_cluster = next_cluster;
        }
    }

    return bytes_read;
}

// Write to a file
uint32_t fat32_write(struct fat32_file* file, const void* buffer, uint32_t size) {
    if (!file || !buffer || size == 0) {
        return 0;
    }

    uint32_t bytes_written = 0;
    const uint8_t* buf = (const uint8_t*)buffer;
    uint8_t cluster_buffer[fs_private->bytes_per_cluster];

    while (bytes_written < size) {
        // Calculate position within cluster
        uint32_t cluster_offset = file->position % fs_private->bytes_per_cluster;
        
        // If at start of cluster, read it first
        if (cluster_offset == 0) {
            if (!read_cluster(fs_private, file->current_cluster, cluster_buffer)) {
                break;
            }
        }

        // Calculate how much to write
        uint32_t to_write = fs_private->bytes_per_cluster - cluster_offset;
        if (to_write > size - bytes_written) {
            to_write = size - bytes_written;
        }

        // Copy data
        memcpy(cluster_buffer + cluster_offset, buf + bytes_written, to_write);
        
        // Write the cluster
        if (!write_cluster(fs_private, file->current_cluster, cluster_buffer)) {
            break;
        }

        bytes_written += to_write;
        file->position += to_write;

        // Update file size if needed
        if (file->position > file->size) {
            file->size = file->position;
        }

        // Move to next cluster if needed
        if (file->position % fs_private->bytes_per_cluster == 0) {
            uint32_t next_cluster = get_next_cluster(fs_private, file->current_cluster);
            if (next_cluster >= 0x0FFFFFF8) {
                // Allocate new cluster
                // TODO: Implement cluster allocation
                break;
            }
            file->current_cluster = next_cluster;
        }
    }

    // Sync the device after write
    if (bytes_written > 0) {
        block_device_sync(fs_private->dev);
    }

    return bytes_written;
}

// Seek in a file
bool fat32_seek(struct fat32_file* file, uint32_t position) {
    if (!file || position > file->size) {
        return false;
    }

    // Calculate target cluster
    uint32_t target_cluster = file->first_cluster;
    uint32_t cluster_count = position / fs_private->bytes_per_cluster;

    for (uint32_t i = 0; i < cluster_count; i++) {
        target_cluster = get_next_cluster(fs_private, target_cluster);
        if (target_cluster >= 0x0FFFFFF8) {
            return false;
        }
    }

    file->current_cluster = target_cluster;
    file->position = position;
    return true;
}

// Read directory entry
bool fat32_readdir(struct fat32_file* dir, struct fat32_dir_entry* entry) {
    if (!dir || !entry || !dir->is_directory) {
        kprintf(ERROR, "fat32_readdir: Invalid parameters - dir=%p, entry=%p, is_directory=%d\n", 
                dir, entry, dir ? dir->is_directory : 0);
        return false;
    }
    
    if (dir->current_cluster == 0) {
        kprintf(ERROR, "fat32_readdir: Invalid cluster number: %d\n", dir->current_cluster);
        return false;
    }
    
    
    uint8_t* cluster_buffer = kmalloc(fs_private->bytes_per_cluster);
    if (!cluster_buffer) {
        kprintf(ERROR, "fat32_readdir: Failed to allocate cluster buffer of size %d\n", 
                fs_private->bytes_per_cluster);
        return false;
    }
    
    while (true) {
        // Read the current cluster
        uint32_t sector = cluster_to_lba(fs_private, dir->current_cluster);
        
        if (!block_device_read(dir->dev, sector, fs_private->boot_sector.sectors_per_cluster, cluster_buffer)) {
            kprintf(ERROR, "fat32_readdir: Failed to read cluster %d\n", dir->current_cluster);
            kfree(cluster_buffer);
            return false;
        }
        
        // Calculate entry offset within the cluster
        uint32_t entry_offset = (dir->position % fs_private->bytes_per_cluster) / sizeof(struct fat32_dir_entry);
        uint32_t entries_per_cluster = fs_private->bytes_per_cluster / sizeof(struct fat32_dir_entry);
        
        
        // Read entries in the current cluster
        for (; entry_offset < entries_per_cluster; entry_offset++) {
            struct fat32_dir_entry* current_entry = (struct fat32_dir_entry*)(cluster_buffer + entry_offset * sizeof(struct fat32_dir_entry));
            
            
            // Check for end of directory
            if (current_entry->name[0] == 0) {
                kfree(cluster_buffer);
                return false;
            }
            
            // Skip deleted entries and volume labels
            if (current_entry->name[0] == 0xE5) {
                dir->position += sizeof(struct fat32_dir_entry);
                continue;
            }
            
            if (current_entry->attributes & 0x08) {
                dir->position += sizeof(struct fat32_dir_entry);
                continue;
            }
            
            // Found a valid entry
            memcpy(entry, current_entry, sizeof(struct fat32_dir_entry));
            dir->position += sizeof(struct fat32_dir_entry);
            
            kfree(cluster_buffer);
            return true;
        }
        
        // Move to the next cluster
        uint32_t next_cluster = get_next_cluster(fs_private, dir->current_cluster);
        
        if (next_cluster >= 0x0FFFFFF8) {
            kprintf(ERROR, "fat32_readdir: End of directory chain reached at cluster %d\n", 
                    dir->current_cluster);
            kfree(cluster_buffer);
            return false;
        }
        
        dir->current_cluster = next_cluster;
        dir->position = 0;  // Reset position for new cluster
        
        kfree(cluster_buffer);
        cluster_buffer = kmalloc(fs_private->bytes_per_cluster);
        if (!cluster_buffer) {
            kprintf(ERROR, "fat32_readdir: Failed to allocate cluster buffer for next cluster\n");
            return false;
        }
    }
}

// Create directory
bool fat32_mkdir(struct block_device* dev, const char* path) {
    if (!fs_private || !path) {
        kprintf(ERROR, "fat32_mkdir: Invalid parameters\n");
        return false;
    }

    // Find parent directory
    char* path_copy = strdup(path);
    if (!path_copy) {
        kprintf(ERROR, "fat32_mkdir: Failed to allocate path copy\n");
        return false;
    }

    char* last_slash = strrchr(path_copy, '/');
    char* dir_name;
    struct fat32_file* parent;
    
    if (!last_slash) {
        // No slash found, create in root
        dir_name = path_copy;
        parent = fat32_open(dev, "/");
    } else if (last_slash == path_copy) {
        // Path starts with slash, create in root
        dir_name = last_slash + 1;
        parent = fat32_open(dev, "/");
    } else {
        // Create in subdirectory
        *last_slash = '\0';
        dir_name = last_slash + 1;
        parent = fat32_open(dev, path_copy);
    }
    
    if (!parent || !parent->is_directory) {
        kprintf(ERROR, "fat32_mkdir: Failed to open parent directory\n");
        kfree(path_copy);
        return false;
    }
    
    // Create directory entry
    struct fat32_dir_entry entry;
    memset(&entry, 0, sizeof(entry));
    
    // Convert name to 8.3 format
    char name83[11];
    convert_to_83_name(dir_name, name83);
    
    // Copy name to entry
    memcpy(entry.name, name83, 11);
    entry.attributes = 0x10;  // Directory attribute
    
    // Allocate new cluster for directory
    uint32_t new_cluster = 2;  // Start searching from cluster 2
    while (new_cluster < 0x0FFFFFF8) {
        uint32_t next_cluster = get_next_cluster(fs_private, new_cluster);
        if (next_cluster == 0) {  // Free cluster found
            break;
        }
        new_cluster++;
    }
    
    if (new_cluster >= 0x0FFFFFF8) {
        kprintf(ERROR, "fat32_mkdir: No free clusters found\n");
        fat32_close(parent);
        kfree(path_copy);
        return false;
    }
    
    // Mark cluster as used
    uint32_t fat_offset = new_cluster * 4;
    uint32_t fat_sector_offset = (fat_offset / fs_private->boot_sector.bytes_per_sector);
    uint32_t sector_offset = fat_offset % fs_private->boot_sector.bytes_per_sector;
    
    // Update FAT cache in memory
    fs_private->fat_cache[new_cluster] = 0x0FFFFFFF; // End of chain
    
    // Write to all FATs on disk
    uint8_t sector_buf[fs_private->boot_sector.bytes_per_sector];
    // Read the sector first (to preserve other entries)
    if (!block_device_read(dev, fs_private->fat_start + fat_sector_offset, 1, sector_buf)) {
        kprintf(ERROR, "fat32_mkdir: Failed to read FAT sector\n");
        fat32_close(parent);
        kfree(path_copy);
        return false;
    }
    *(uint32_t*)(sector_buf + sector_offset) = 0x0FFFFFFF; // End of chain
    if (!write_fat_sector_all(fs_private, fat_sector_offset, sector_buf)) {
        kprintf(ERROR, "fat32_mkdir: Failed to mark cluster as used in all FATs\n");
        fat32_close(parent);
        kfree(path_copy);
        return false;
    }
    
    // Initialize directory with "." and ".." entries
    uint8_t* cluster_buffer = kmalloc(fs_private->bytes_per_cluster);
    if (!cluster_buffer) {
        kprintf(ERROR, "fat32_mkdir: Failed to allocate cluster buffer\n");
        fat32_close(parent);
        kfree(path_copy);
        return false;
    }
    memset(cluster_buffer, 0, fs_private->bytes_per_cluster);
    
    // Create "." entry
    struct fat32_dir_entry* dot_entry = (struct fat32_dir_entry*)cluster_buffer;
    memset(dot_entry, 0, sizeof(struct fat32_dir_entry));
    memcpy(dot_entry->name, ".          ", 11);
    dot_entry->attributes = 0x10;
    dot_entry->first_cluster_low = new_cluster & 0xFFFF;
    dot_entry->first_cluster_high = (new_cluster >> 16) & 0xFFFF;
    
    // Create ".." entry
    struct fat32_dir_entry* dotdot_entry = (struct fat32_dir_entry*)(cluster_buffer + sizeof(struct fat32_dir_entry));
    memset(dotdot_entry, 0, sizeof(struct fat32_dir_entry));
    memcpy(dotdot_entry->name, "..         ", 11);
    dotdot_entry->attributes = 0x10;
    dotdot_entry->first_cluster_low = parent->first_cluster & 0xFFFF;
    dotdot_entry->first_cluster_high = (parent->first_cluster >> 16) & 0xFFFF;
    
    // Write the new directory cluster
    uint32_t sector = cluster_to_lba(fs_private, new_cluster);
    if (!block_device_write(dev, sector, fs_private->boot_sector.sectors_per_cluster, cluster_buffer)) {
        kprintf(ERROR, "fat32_mkdir: Failed to write directory cluster\n");
        kfree(cluster_buffer);
        fat32_close(parent);
        kfree(path_copy);
        return false;
    }
    
    kfree(cluster_buffer);
    
    // Update entry with new cluster
    entry.first_cluster_low = new_cluster & 0xFFFF;
    entry.first_cluster_high = (new_cluster >> 16) & 0xFFFF;
    
    // Write entry to parent directory
    uint8_t* parent_sector = kmalloc(fs_private->bytes_per_cluster);
    if (!parent_sector) {
        kprintf(ERROR, "fat32_mkdir: Failed to allocate parent sector buffer\n");
        fat32_close(parent);
        kfree(path_copy);
        return false;
    }
    
    if (!read_cluster(fs_private, parent->current_cluster, parent_sector)) {
        kprintf(ERROR, "fat32_mkdir: Failed to read parent directory\n");
        kfree(parent_sector);
        fat32_close(parent);
        kfree(path_copy);
        return false;
    }
    
    struct fat32_dir_entry* entries = (struct fat32_dir_entry*)parent_sector;
    uint32_t entries_per_sector = fs_private->bytes_per_cluster / sizeof(struct fat32_dir_entry);
    
    // Find empty entry
    uint32_t i;
    for (i = 0; i < entries_per_sector; i++) {
        if (entries[i].name[0] == 0 || entries[i].name[0] == 0xE5) {
            break;
        }
    }
    
    if (i >= entries_per_sector) {
        kprintf(ERROR, "fat32_mkdir: No free entries in parent directory\n");
        kfree(parent_sector);
        fat32_close(parent);
        kfree(path_copy);
        return false;
    }
    
    // Write new entry
    memcpy(&entries[i], &entry, sizeof(struct fat32_dir_entry));
    if (!write_cluster(fs_private, parent->current_cluster, parent_sector)) {
        kprintf(ERROR, "fat32_mkdir: Failed to write parent directory\n");
        kfree(parent_sector);
        fat32_close(parent);
        kfree(path_copy);
        return false;
    }
    
    kfree(parent_sector);
    fat32_close(parent);
    kfree(path_copy);

    // Sync the device after directory creation
    block_device_sync(dev);
    
    return true;
}

// Remove directory
bool fat32_rmdir(struct block_device* dev, const char* path) {
    if (!fs_private || !path) {
        return false;
    }

    // Open directory
    struct fat32_file* dir = fat32_open(dev, path);
    if (!dir || !dir->is_directory) {
        return false;
    }
    
    // Check if directory is empty
    struct fat32_dir_entry entry;
    if (fat32_readdir(dir, &entry)) {
        fat32_close(dir);
        return false;  // Directory not empty
    }
    
    // Mark directory entry as deleted
    uint8_t sector[512];
    if (!read_cluster(fs_private, dir->current_cluster, sector)) {
        fat32_close(dir);
        return false;
    }
    
    struct fat32_dir_entry* entries = (struct fat32_dir_entry*)sector;
    uint32_t entries_per_sector = 512 / sizeof(struct fat32_dir_entry);
    
    // Find directory entry
    uint32_t i;
    for (i = 0; i < entries_per_sector; i++) {
        if (strncmp((char*)entries[i].name, path, 11) == 0) {
            break;
        }
    }
    
    if (i >= entries_per_sector) {
        fat32_close(dir);
        return false;
    }
    
    // Mark as deleted
    entries[i].name[0] = 0xE5;
    if (!write_cluster(fs_private, dir->current_cluster, sector)) {
        fat32_close(dir);
        return false;
    }
    
    fat32_close(dir);
    return true;
}

// Delete file
bool fat32_unlink(struct block_device* dev, const char* path) {
    if (!fs_private || !path) {
        return false;
    }

    // Open file
    struct fat32_file* file = fat32_open(dev, path);
    if (!file || file->is_directory) {
        return false;
    }
    
    // Mark file entry as deleted
    uint8_t sector[512];
    if (!read_cluster(fs_private, file->current_cluster, sector)) {
        fat32_close(file);
        return false;
    }
    
    struct fat32_dir_entry* entries = (struct fat32_dir_entry*)sector;
    uint32_t entries_per_sector = 512 / sizeof(struct fat32_dir_entry);
    
    // Find file entry
    uint32_t i;
    for (i = 0; i < entries_per_sector; i++) {
        if (strncmp((char*)entries[i].name, path, 11) == 0) {
            break;
        }
    }
    
    if (i >= entries_per_sector) {
        fat32_close(file);
        return false;
    }
    
    // Mark as deleted
    entries[i].name[0] = 0xE5;
    if (!write_cluster(fs_private, file->current_cluster, sector)) {
        fat32_close(file);
        return false;
    }
    
    fat32_close(file);
    return true;
}

// VFS interface functions
void fat32_vfs_open(struct vfs_node* node) {
    struct fat32_file* file = node->impl;
    if (!file) return;
    // Implementation specific open operations
}

void fat32_vfs_close(struct vfs_node* node) {
    struct fat32_file* file = node->impl;
    if (!file) return;
    fat32_close(file);
}

uint32_t fat32_vfs_read(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    struct fat32_file* file = node->impl;
    if (!file) {
        kprintf(ERROR, "fat32_vfs_read: No file structure\n");
        return 0;
    }
    
    // If we're at or past the end of file, return 0
    if (offset >= file->size) {
        return 0;
    }
    
    // Calculate how much we can actually read
    uint32_t remaining = file->size - offset;
    if (size > remaining) {
        size = remaining;
    }
    
    // Always seek to the correct position
    if (!fat32_seek(file, offset)) {
        kprintf(ERROR, "fat32_vfs_read: Failed to seek to position %d\n", offset);
        return 0;
    }
    
    uint32_t bytes_read = fat32_read(file, buffer, size);
            
    return bytes_read;
}

uint32_t fat32_vfs_write(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    struct fat32_file* file = node->impl;
    if (!file) return 0;
    
    if (offset != file->position) {
        if (!fat32_seek(file, offset)) {
            return 0;
        }
    }
    
    return fat32_write(file, buffer, size);
}

struct vfs_node* fat32_vfs_readdir(struct vfs_node* node, uint32_t index) {
    struct fat32_file* file = node->impl;
    if (!file || !file->is_directory) {
        return NULL;
    }
    
    file->position = 0;
    file->current_cluster = file->first_cluster;

    struct fat32_dir_entry entry;
    uint32_t valid_index = 0;
    
    // Read entries until we find the one at the requested index
    while (fat32_readdir(file, &entry)) {
        // Skip volume labels and deleted entries
        if ((unsigned char)entry.name[0] == 0xE5 || entry.name[0] == 0x05 || 
            (entry.name[0] == '.' && entry.name[1] == '\0') ||
            (entry.name[0] == '.' && entry.name[1] == '.' && entry.name[2] == '\0')) {
            continue;
        }
        
        if (valid_index == index) {
            // Convert 8.3 name to readable format
            char entry_name[13];
            memset(entry_name, 0, sizeof(entry_name));
            strncpy(entry_name, (char*)entry.name, 8);
            entry_name[8] = '\0';
            
            // Trim trailing spaces
            char* end = entry_name + strlen(entry_name) - 1;
            while (end >= entry_name && *end == ' ') {
                *end-- = '\0';
            }
            
            // Add extension if present
            if (entry.name[8] != ' ') {
                strcat(entry_name, ".");
                strncat(entry_name, (char*)entry.name + 8, 3);
                
                // Trim trailing spaces from extension
                end = entry_name + strlen(entry_name) - 1;
                while (end >= entry_name && *end == ' ') {
                    *end-- = '\0';
                }
            }
            
            struct vfs_node* result = kmalloc(sizeof(struct vfs_node));
            if (!result) {
                return NULL;
            }
            
            memset(result, 0, sizeof(struct vfs_node));
            strncpy(result->name, entry_name, 127);
            result->name[127] = '\0';
            result->flags = (entry.attributes & 0x10) ? FS_DIRECTORY : FS_FILE;
            result->length = entry.file_size;
            
            // Create a new file structure for the entry
            struct fat32_file* entry_file = kmalloc(sizeof(struct fat32_file));
            if (!entry_file) {
                kfree(result);
                return NULL;
            }
            
            memset(entry_file, 0, sizeof(struct fat32_file));
            entry_file->dev = file->dev;
            entry_file->first_cluster = entry.first_cluster_low | (entry.first_cluster_high << 16);
            entry_file->current_cluster = entry_file->first_cluster;
            entry_file->position = 0;
            entry_file->size = entry.file_size;
            entry_file->is_directory = (entry.attributes & 0x10) != 0;
            
            result->impl = entry_file;
            return result;
        }
        valid_index++;
    }
    
    return NULL;
}

struct vfs_node* fat32_vfs_finddir(struct vfs_node* node, const char* name) {
    struct fat32_file* file = node ? node->impl : NULL;
    if (!node) {
        kprintf(ERROR, "fat32_vfs_finddir: node is NULL\n");
        return NULL;
    }
    if (!file) {
        kprintf(ERROR, "fat32_vfs_finddir: node->impl (fat32_file) is NULL for node '%s'\n", node->name);
        return NULL;
    }
    if (!file->is_directory) {
        kprintf(ERROR, "fat32_vfs_finddir: node->impl is not a directory for node '%s'\n", node->name);
        return NULL;
    }
    
    // Handle special directory entries
    if (strcmp(name, ".") == 0) {
        // Return current directory
        struct vfs_node* result = kmalloc(sizeof(struct vfs_node));
        if (!result) {
            kprintf(ERROR, "fat32_vfs_finddir: Failed to allocate node\n");
            return NULL;
        }
        memset(result, 0, sizeof(struct vfs_node));
        strncpy(result->name, node->name, 127);
        result->name[127] = '\0';
        result->flags = node->flags;
        result->length = node->length;
        
        // Create a new file structure for the entry
        struct fat32_file* entry_file = kmalloc(sizeof(struct fat32_file));
        if (!entry_file) {
            kfree(result);
            return NULL;
        }
        memset(entry_file, 0, sizeof(struct fat32_file));
        entry_file->dev = file->dev;
        entry_file->first_cluster = file->first_cluster;
        entry_file->current_cluster = file->current_cluster;
        entry_file->position = file->position;
        entry_file->size = file->size;
        entry_file->is_directory = file->is_directory;
        
        result->impl = entry_file;
        result->parent = node->parent;
        
        // Set up VFS operations
        result->open = fat32_vfs_open;
        result->close = fat32_vfs_close;
        result->read = fat32_vfs_read;
        result->write = fat32_vfs_write;
        result->readdir = fat32_vfs_readdir;
        result->finddir = fat32_vfs_finddir;
        
        return result;
    }
    
    if (strcmp(name, "..") == 0) {
        // Return parent directory
        if (!node->parent) {
            // If no parent, return root
            return fat32_get_root();
        }
        
        // Get the parent directory's file structure
        struct fat32_file* parent_file = node->parent->impl;
        if (!parent_file) {
            // If parent has no file structure, create one
            parent_file = kmalloc(sizeof(struct fat32_file));
            if (!parent_file) {
                kprintf(ERROR, "fat32_vfs_finddir: Failed to allocate parent file structure\n");
                return NULL;
            }
            memset(parent_file, 0, sizeof(struct fat32_file));
            
            // Read the ".." entry to get parent's cluster
            struct fat32_dir_entry dotdot;
            bool found = false;
            
            // Reset directory position
            file->position = 0;
            file->current_cluster = file->first_cluster;
            
            while (fat32_readdir(file, &dotdot)) {
                if (dotdot.name[0] == '.' && dotdot.name[1] == '.' && dotdot.name[2] == ' ') {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                kprintf(ERROR, "fat32_vfs_finddir: Could not find .. entry\n");
                kfree(parent_file);
                return NULL;
            }
            
            // Initialize parent file structure
            parent_file->dev = file->dev;
            uint32_t parent_cluster = dotdot.first_cluster_low | (dotdot.first_cluster_high << 16);
            
            // If parent cluster is 0, this is a root directory
            if (parent_cluster == 0) {
                parent_cluster = fs_private->root_dir_cluster;
            }
            
            parent_file->first_cluster = parent_cluster;
            parent_file->current_cluster = parent_cluster;
            parent_file->position = 0;
            parent_file->size = 0;
            parent_file->is_directory = true;
            
            // Set the parent's file structure
            node->parent->impl = parent_file;
            node->parent->flags = FS_DIRECTORY;  // Ensure parent is marked as directory
        }
        
        struct vfs_node* result = kmalloc(sizeof(struct vfs_node));
        if (!result) {
            kprintf(ERROR, "fat32_vfs_finddir: Failed to allocate node\n");
            return NULL;
        }
        memset(result, 0, sizeof(struct vfs_node));
        strncpy(result->name, node->parent->name, 127);
        result->name[127] = '\0';
        result->flags = FS_DIRECTORY;  // Ensure result is marked as directory
        result->length = node->parent->length;
        
        // Create a new file structure for the entry
        struct fat32_file* entry_file = kmalloc(sizeof(struct fat32_file));
        if (!entry_file) {
            kfree(result);
            return NULL;
        }
        memset(entry_file, 0, sizeof(struct fat32_file));
        entry_file->dev = parent_file->dev;
        entry_file->first_cluster = parent_file->first_cluster;
        entry_file->current_cluster = parent_file->first_cluster;  // Reset to first cluster
        entry_file->position = 0;  // Reset position
        entry_file->size = parent_file->size;
        entry_file->is_directory = true;  // Parent is always a directory
        
        result->impl = entry_file;
        result->parent = node->parent->parent;
        
        // Set up VFS operations
        result->open = fat32_vfs_open;
        result->close = fat32_vfs_close;
        result->read = fat32_vfs_read;
        result->write = fat32_vfs_write;
        result->readdir = fat32_vfs_readdir;
        result->finddir = fat32_vfs_finddir;
        
        return result;
    }
    
    // Reset directory position
    file->position = 0;
    file->current_cluster = file->first_cluster;
    
    // Convert search name to 8.3 format
    char search_name83[11];
    convert_to_83_name(name, search_name83);
    
    struct fat32_dir_entry entry;
    while (fat32_readdir(file, &entry)) {
        // Compare names in 8.3 format
        if (memcmp(entry.name, search_name83, 11) == 0) {
            // Convert 8.3 name to readable format for display
            char entry_name[13];
            memset(entry_name, 0, sizeof(entry_name));
            strncpy(entry_name, (char*)entry.name, 8);
            entry_name[8] = '\0';
            
            // Trim trailing spaces
            char* end = entry_name + strlen(entry_name) - 1;
            while (end >= entry_name && *end == ' ') {
                *end-- = '\0';
            }
            
            // Add extension if present
            if (entry.name[8] != ' ') {
                strcat(entry_name, ".");
                strncat(entry_name, (char*)entry.name + 8, 3);
                
                // Trim trailing spaces from extension
                end = entry_name + strlen(entry_name) - 1;
                while (end >= entry_name && *end == ' ') {
                    *end-- = '\0';
                }
            }
            
            struct vfs_node* result = kmalloc(sizeof(struct vfs_node));
            if (!result) {
                kprintf(ERROR, "fat32_vfs_finddir: Failed to allocate node\n");
                return NULL;
            }
            
            memset(result, 0, sizeof(struct vfs_node));
            strncpy(result->name, entry_name, 127);
            result->name[127] = '\0';
            result->flags = (entry.attributes & 0x10) ? FS_DIRECTORY : FS_FILE;
            result->length = entry.file_size;
            
            // Create a new file structure for the entry
            struct fat32_file* entry_file = kmalloc(sizeof(struct fat32_file));
            if (!entry_file) {
                kfree(result);
                return NULL;
            }
            
            memset(entry_file, 0, sizeof(struct fat32_file));
            entry_file->dev = file->dev;
            entry_file->first_cluster = entry.first_cluster_low | (entry.first_cluster_high << 16);
            entry_file->current_cluster = entry_file->first_cluster;
            entry_file->position = 0;
            entry_file->size = entry.file_size;
            entry_file->is_directory = (entry.attributes & 0x10) != 0;
            
            result->impl = entry_file;
            result->parent = node;
            
            // Set up VFS operations
            result->open = fat32_vfs_open;
            result->close = fat32_vfs_close;
            result->read = fat32_vfs_read;
            result->write = fat32_vfs_write;
            result->readdir = fat32_vfs_readdir;
            result->finddir = fat32_vfs_finddir;
            
            return result;
        }
    }
    
    kprintf(ERROR, "fat32_vfs_finddir: No matching entry found\n");
    return NULL;
}

// Get root directory
struct vfs_node* fat32_get_root(void) {
    if (!fs_private) {
        return NULL;
    }
    return fat32_root_node;
}

// Create a new node
struct vfs_node* fat32_create_node(const char* name, uint32_t flags) {
    struct vfs_node* node = kmalloc(sizeof(struct vfs_node));
    if (!node) return NULL;
    
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
    
    return node;
}

// Find directory entry
struct vfs_node* fat32_finddir(struct vfs_node* node, const char* name) {
    return fat32_vfs_finddir(node, name);
}

// Unmount FAT32 filesystem
bool fat32_unmount(void) {
    if (!fs_private) {
        kprintf(ERROR, "fat32_unmount: No filesystem mounted\n");
        return false;
    }
    
    kprintf(INFO, "Unmounting FAT32 filesystem...\n");
    
    // Flush FAT cache to disk
    for (uint32_t i = 0; i < fs_private->boot_sector.sectors_per_fat_32; i++) {
        uint8_t* sector_buffer = (uint8_t*)fs_private->fat_cache + (i * fs_private->boot_sector.bytes_per_sector);
        if (!write_fat_sector_all(fs_private, i, sector_buffer)) {
            kprintf(ERROR, "fat32_unmount: Failed to write FAT sector %d to all FATs\n", i);
            return false;
        }
    }
    
    // Clear dirty bit in boot sector
    fs_private->boot_sector.ext_flags &= ~0x80;  // Clear dirty bit
    if (!block_device_write(fs_private->dev, 0, 1, &fs_private->boot_sector)) {
        kprintf(ERROR, "fat32_unmount: Failed to write boot sector\n");
        return false;
    }
    
    // Free FAT cache
    kfree(fs_private->fat_cache);
    
    // Free private data
    kfree(fs_private);
    fs_private = NULL;
    
    kprintf(INFO, "FAT32 filesystem unmounted successfully\n");
    return true;
}