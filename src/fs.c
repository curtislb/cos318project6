/*
 * Author(s): Curtis Belmonte
 * COS 318, Fall 2015: Project 6 File System.
 * Implementation of a Unix-like file system.
 */
#include "util.h"
#include "common.h"
#include "block.h"
#include "fs.h"

#ifdef FAKE
#include <stdio.h>
#define ERROR_MSG(m) printf m;
#else
#define ERROR_MSG(m)
#endif

/* Static helper functions ***************************************************/

static int ceil_div(int m, int n) {
    return ((m - 1) / n) + 1;
}

static int min(int x, int y) {
    return (x < y) ? x : y;
}

static void str_copy(char *src, char *dest) {
    bcopy((unsigned char *)src, (unsigned char *)dest, strlen(src));
}

static void str_append(char *src, char *dest) {
    bcopy(
        (unsigned char *)src,
        (unsigned char *)&dest[strlen(dest)],
        strlen(src)
    );
}

/* Super block ***************************************************************/

static sblock_t *sblock;
static char sblock_buf[BLOCK_SIZE];

static void sblock_init(sblock_t *sblock) {
    sblock->fs_size = FS_SIZE;

    sblock->inode_start = SUPER_BLOCK + 1;
    sblock->inode_count = MAX_FILE_COUNT;
    sblock->inode_blocks = ceil_div(MAX_FILE_COUNT, BLOCK_SIZE/sizeof(inode_t));
    
    sblock->bamap_start = sblock->inode_start + sblock->inode_blocks;
    sblock->bamap_blocks = ceil_div(MAX_FILE_COUNT*sizeof(uint8_t), BLOCK_SIZE);
    
    sblock->data_start = sblock->bamap_start + sblock->bamap_blocks;
    sblock->data_blocks = MAX_FILE_COUNT;
}

static sblock_t *sblock_read(char *block_buf) {
    block_read(SUPER_BLOCK, block_buf);
    return (sblock_t *)block_buf;
}

static void sblock_write(char *block_buf) {
    block_write(SUPER_BLOCK, block_buf);
}

/* Block allocation map ******************************************************/

static int bamap_block(int index) {
    return sblock->bamap_start + (index * sizeof(uint8_t) / BLOCK_SIZE);
}

static uint8_t *bamap_read(int index, char *block_buf) {
    block_read(bamap_block(index), block_buf);
    return (uint8_t *)&block_buf[index % BLOCK_SIZE];
}

static void bamap_write(int index, char *block_buf) {
    block_write(bamap_block(index), block_buf);
}

static int block_alloc(void) {
    int i, j;
    char block_buf[BLOCK_SIZE];

    // Search for flag indicating free block
    for (i = 0; i < sblock->bamap_blocks; i++) {
        block_read(sblock->bamap_start + i, block_buf);
        for (j = 0; j < BLOCK_SIZE; j++) {
            if (!block_buf[j]) {
                // Mark block as used on disk
                block_buf[j] = TRUE;
                block_write(sblock->bamap_start + i, block_buf);

                // Return index of newly allocated block
                return (i * BLOCK_SIZE) + j;
            }
        }
    }

    // No free blocks found
    return FAILURE;
}

static int block_free(int index) {
    uint8_t *in_use;
    char block_buf[BLOCK_SIZE];

    // Read block usage flag, fail if not in use
    in_use = bamap_read(index, block_buf);
    if (!(*in_use)) {
        return FAILURE;
    }

    // Mark block as free on disk
    *in_use = FALSE;
    bamap_write(index, block_buf);

    return SUCCESS;
}

/* Data blocks ***************************************************************/

static void data_read(int index, char *block_buf) {
    block_read(sblock->data_start + index, block_buf);
}

static void data_write(int index, char *block_buf) {
    block_write(sblock->data_start + index, block_buf);
}

/* i-Nodes *******************************************************************/

static void inode_init(inode_t *inode, int type) {
    inode->type = type;
    inode->links = 0;
    inode->fd_count = 0;
    inode->size = 0;
    bzero(inode->blocks, sizeof(inode->blocks)); // GRUMP sizeof struct field
    inode->used_blocks = 0;
}

static inode_t *inode_read(int index, char *block_buf) {
    int block_inodes;
    int block_offset;
    int block_index;
    inode_t *inodes;

    // Read block containing inode from disk
    block_inodes = BLOCK_SIZE / sizeof(inode_t);
    block_offset = index / block_inodes;
    block_index = sblock->inode_start + block_offset;
    block_read(block_index, block_buf);

    // Return pointer to inode struct in data buffer
    inodes = (inode_t *)block_buf;
    return &inodes[index % block_inodes];
}

static void inode_write(int index, char *block_buf) {
    int block_inodes = BLOCK_SIZE / sizeof(inode_t);
    int block_offset = index / block_inodes;
    int block_index = sblock->inode_start + block_offset;
    block_write(block_index, block_buf);
}

static int inode_create(int type) {
    int i, j;
    int block_inodes;
    char block_buf[BLOCK_SIZE];
    inode_t *inodes;
    int new_block;

    // Search for free inode entry
    block_inodes = BLOCK_SIZE / sizeof(inode_t);
    for (i = 0; i < MAX_FILE_COUNT; i++) {
        block_read(sblock->inode_start + i, block_buf);
        inodes = (inode_t *)block_buf;
        for (j = 0; j < block_inodes; j++) {
            if (inodes[j].type == FREE_INODE) {
                // Write the new inode to disk
                inode_init(&inodes[j], type);
                block_write(sblock->inode_start + i, block_buf);

                // Return index of newly created inode
                return (i * block_inodes) + j;
            }
        }
    }

    return FAILURE;
}

static int inode_free(inode_t *inode) {
    int i;
    int result;

    // Fail if inode is already free
    if (inode->type == FREE_INODE) {
        return FAILURE;
    }

    // Free all data blocks used by inode
    for (i = 0; i < inode->used_blocks; i++) {
        result = block_free(inode->blocks[i]);
        ASSERT(result != FAILURE);
    }

    // Mark inode as free on disk
    inode->type = FREE_INODE;

    return SUCCESS;
}

/* Directories ***************************************************************/

static void entry_init(entry_t *entry, int inode, char *name) {
    entry->in_use = TRUE;
    entry->inode = inode;
    str_copy(name, entry->name);
}

static int dir_add_entry(int dir_inode, int entry_inode, char *name) {
    int i, j;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    char data_buf[BLOCK_SIZE];
    int block_entries;
    int curr_entries;
    entry_t *entries;
    entry_t new_entry;
    int new_block;

    // Read directory inode from disk
    inode = inode_read(dir_inode, inode_buf);

    // Fail if too many entries in directory
    block_entries = BLOCK_SIZE / sizeof(entry_t);
    curr_entries = inode->size / sizeof(entry_t);
    if (curr_entries >= block_entries * INODE_ADDRS) {
        return FAILURE;
    }

    // Initialize new directory entry
    entry_init(&new_entry, entry_inode, name);

    // Search for free entry in used blocks
    for (i = 0; i < inode->used_blocks; i++) {
        data_read(inode->blocks[i], data_buf);
        entries = (entry_t *)data_buf;
        for (j = 0; j < block_entries; j++) {
            if (!entries[j].in_use) {
                // Add new entry to directory
                entries[j] = new_entry;
                data_write(inode->blocks[i], data_buf);

                // Write changes to inode on disk
                inode->size += sizeof(entry_t);
                inode_write(dir_inode);

                return SUCCESS;
            }
        }
    }

    // All entries used, so allocate new block
    new_block = block_alloc();
    inode->blocks[used_blocks] = new_block;
    inode->used_blocks++;

    // Add entry to newly allocated block
    bzero_block(block_buf);
    entries = (entry_t *)block_buf;
    entries[0] = new_entry;
    data_write(new_block, block_buf);

    // Write changes to inode on disk
    inode->size += sizeof(entry_t);
    inode_write(dir_inode);

    return SUCCESS;
}

static int dir_remove_entry(int dir_inode, char *name) {
    int i, j;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    char data_buf[BLOCK_SIZE];
    entry_t *entries;

    // Read directory inode from disk
    inode = inode_read(dir_inode, inode_buf);

    // Search for matching entry in used blocks
    block_entries = BLOCK_SIZE / sizeof(entry_t);
    for (i = 0; i < inode->used_blocks; i++) {
        data_read(inode->blocks[i], data_buf);
        entries = (entry_t *)data_buf;
        for (j = 0; j < block_entries; j++) {
            if (entries[j].in_use && same_string(entries[j].name, name)) {
                // Mark entry as free on disk
                entries[j].in_use = (uint8_t)FALSE;
                data_write(inode->blocks[i], data_buf);

                return SUCCESS;
            }
        }
    }

    // No matching entry found
    return FAILURE;
}

static int dir_find_entry(int dir_inode, char *name) {
    int i, j;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    char data_buf[BLOCK_SIZE];
    int block_entries;
    entry_t *entries;

    // Read directory inode from disk
    inode = inode_read(dir_inode, inode_buf);

    // Search for matching entry in used blocks
    block_entries = BLOCK_SIZE / sizeof(entry_t);
    for (i = 0; i < inode->used_blocks; i++) {
        data_read(inode->blocks[i], data_buf);
        entries = (entry_t *)data_buf;
        for (j = 0; j < block_entries; j++) {
            // If entry matches, return its inode number
            if (entries[j].in_use && same_string(entries[j].name, name)) {
                return entries[j].inode;
            }
        }
    }

    // No matching entry found
    return FAILURE;
}

/* Working directory *********************************************************/

static wdir_t wdir;

static void wdir_set(int inode) {
    wdir.inode = inode;
}

static void wdir_set_path(char *path) {
    str_copy(path, wdir.path);
}

static void wdir_append_path(char *dirname) {
    str_append(dirname, wdir.path);
    str_append("/", wdir.path);
}

static void wdir_truncate_path(void) {
    int i;
    int path_len;

    // Cannot truncate path from root directory
    path_len = strlen(wdir.path);
    ASSERT(path_len > 0);
    ASSERT(path_len <= MAX_PATH_NAME);
    if (path_len == 1) {
        return;
    }

    // Terminate string after penultimate '/'
    for (i = strlen(wdir.path) - 2; i >= 0; i--) {
        if (wdir.path[i] == '/') {
            wdir.path[i + 1] = '\0';
            return;
        }
    }
}

/* File descriptor table *****************************************************/

static file_t fd_table[MAX_FILE_COUNT];

static void fd_init(int fd) {
    fd_table[fd].is_open = FALSE;
}

static int fd_open(int inode, int mode) {
    int i;

    // Search for and open free fd table entry
    for (i = 0; i < MAX_FILE_COUNT; i++) {
        if (!fd_table[i].is_open) {
            // Set up fd table entry
            fd_table[i].is_open = TRUE;
            fd_table[i].inode = inode;
            fd_table[i].mode = mode;
            fd_table[i].cursor = 0;

            // Return index of entry in fd table
            return i;
        }
    }

    // No free fd table entries found
    return FAILURE;
}

static int fd_close(int fd) {
    // Fail if fd table entry is not open
    if (!fd_table[i].is_open) {
        return FAILURE;
    }

    // Mark fd table entry as free
    fd_table[fd].is_open = FALSE;
    return SUCCESS;
}

/* File system operations ****************************************************/

void fs_init(void) {
    block_init();

    // TODO: wtf i don't even know, man
}

int fs_mkfs(void) {
    int i;
    char block_buf[BLOCK_SIZE];
    inode_t *inode;
    uint8_t *in_use;

    // Format disk if necessary
    sblock = sblock_read(sblock_buf);
    if (sblock->magic_num != SUPER_MAGIC_NUM) {
        // Zero out all file system blocks
        bzero_block(block_buf);
        for (i = 0; i < FS_SIZE; i++) {
            block_write(i, block_buf);
        }

        // Write super block to disk
        bzero_block(sblock_buf);
        sblock_init(sblock);
        sblock_write(sblock_buf);

        // Create inode for root directory
        inode = inode_read(ROOT_DIR, block_buf);
        inode_init(inode, DIRECTORY);
        inode->blocks[0] = ROOT_DIR;
        inode->used_blocks = 1;
        inode_write(ROOT_DIR, block_buf);

        // Mark root directory data block as used
        in_use = bamap_read(ROOT_DIR, block_buf);
        *in_use = TRUE;
        bamap_write(ROOT_DIR, block_buf);

        // Set up root directory with '.' and '..' entries
        dir_add_entry(ROOT_INODE, ROOT_INODE, ".");
        dir_add_entry(ROOT_INODE, ROOT_INODE, "..");
    }

    // Mount root as current working directory
    wdir_set(ROOT_DIR);
    wdir_set_path("/");

    // Initialize the file descriptor table
    for (i = 0; i < MAX_FILE_COUNT; i++) {
        fd_init(i);
    }

    return SUCCESS; // GRUMP when will this fail?
}

int fs_open(char *fileName, int flags) {
    int entry_inode;
    int result;
    char block_buf[BLOCK_SIZE];
    int fd;

    // Search for entry in working directory
    entry_inode = dir_find_entry(wdir.inode, fileName);

    // If entry does not exist, attempt to create it
    if (entry_inode == FAILURE) {
        // Fail if trying to open non-existent file read-only
        if (flags == FS_O_RDONLY) {
            return FAILURE;
        }

        // Create new inode for file
        entry_inode = inode_create(FILE_TYPE);
        if (entry_inode == FAILURE) {
            return FAILURE;
        }

        // Add new file entry to working directory
        result = dir_add_entry(wdir.inode, entry_inode, fileName);
        if (result == FAILURE) {
            return FAILURE;
        }
    }
    
    // Read inode, and fail if not a file
    inode = inode_read(entry_inode, block_buf);
    if (inode->type != FILE_TYPE) {
        return FAILURE;
    }

    // Open entry in file descriptor table
    fd = fd_open(entry_inode, flags);
    if (fd == FAILURE) {
        return FAILURE;
    }

    // Increment open fd count for inode
    inode->fd_count++;
    inode_write(file_inode, block_buf);

    return fd;
}

int fs_close(int fd) {
    int i;
    int inode_index;
    inode_t *inode;
    char block_buf[BLOCK_SIZE];
    int result;

    // Fail if given bad file descriptor
    if (fd < 0 || fd >= MAX_FILE_COUNT) {
        return FAILURE;
    }

    // Fail if fd table entry is already closed
    if (!fd_table[fd].is_open) {
        return FAILURE;
    }

    // Read corresponding inode from disk
    inode_index = fd_table[fd].inode;
    inode = inode_read(inode_index, block_buf);
    ASSERT(inode->type == FILE_TYPE);
    ASSERT(inode->fd_count > 0);

    // Close fd table entry
    result = fd_close(fd);
    if (result == FAILURE) {
        return FAILURE;
    }

    // Decrement open fd count of inode, free inode if necessary
    inode->fd_count--;
    if (inode->links == 0 && inode->fd_count == 0) {
        result = inode_free(inode);
        ASSERT(result != FAILURE);
    }
    inode_write(inode_index, block_buf);

    return SUCCESS;
}

int fs_read(int fd, char *buf, int count) {
    int i;
    file_t *file;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    char data_buf[BLOCK_SIZE];
    int avail_bytes;
    int index_start;
    int bytes_read;
    int block_offset;
    int block_bytes;
    int to_read;

    // Cannot read from file if fd entry not open
    file = &fd_table[fd];
    if (!file->is_open) {
        return FAILURE;
    }

    // Cannot read from file if opened write-only
    if (file->mode == FS_O_WRONLY) {
        return FAILURE;
    }

    // Read file inode from disk
    inode = inode_read(file->inode, inode_buf);
    ASSERT(inode->type == FILE_TYPE);

    // Read no more than remaining bytes in file
    avail_bytes = inode->size - file->cursor;
    if (count > avail_bytes) {
        count = avail_bytes;
    }

    // Read count bytes from file blocks to buffer
    bytes_read = 0;
    index_start = file->cursor / BLOCK_SIZE;
    for (i = index_start; bytes_read < count && i < INODE_ADDRS; i++) {
        // Read file data block from disk
        data_read(inode->blocks[i], data_buf);

        // Determine offset and bytes to read in block
        block_offset = file->cursor % BLOCK_SIZE;
        block_bytes = BLOCK_SIZE - block_offset;
        to_read = min(count - bytes_read, block_bytes);

        // Read bytes from data block to buffer
        bcopy(
            (unsigned char *)&data_buf[block_offset],
            (unsigned char *)&buf[bytes_read],
            to_read
        );

        // Update cursor and byte count
        file->cursor += to_read;
        bytes_read += to_read;
    }

    return bytes_read;
}
    
int fs_write(int fd, char *buf, int count) {
    int i;
    file_t *file;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    char data_buf[BLOCK_SIZE];
    int index_start;
    int bytes_written;
    int block_offset;
    int block_bytes;
    int to_write;

    // Cannot write to file if fd entry not open
    file = &fd_table[fd];
    if (!file->is_open) {
        return FAILURE;
    }

    // Cannot write to file if opened read-only
    if (file->mode == FS_O_RDONLY) {
        return FAILURE;
    }

    // If writing no bytes, return 0 immediately
    if (count == 0) {
        return 0;
    }

    // Cannot write beyond end of last data block
    if (file->cursor >= INODE_ADDRS * BLOCK_SIZE) {
        return FAILURE;
    }

    // Read file inode from disk
    inode = inode_read(file->inode, inode_buf);
    ASSERT(inode->type == FILE_TYPE);

    // If cursor after end of file, pad with zeros up to cursor
    index_start = inode->size / BLOCK_SIZE;
    for (i = index_start; inode->size < file->cursor; i++) {
        // Allocate new data block if necessary
        if (i >= inode->used_blocks) {
            inode->blocks[i] = block_alloc();
            inode->used_blocks++;
        }

        // Read file data block from disk
        data_read(inode->blocks[i], data_buf);

        // Determine offset and bytes to write in block
        block_offset = file->size % BLOCK_SIZE;
        block_bytes = BLOCK_SIZE - block_offset;
        to_write = min(file->cursor - file->size, block_bytes);

        // Write zero padding bytes to block on disk
        bzero(&data_buf[block_offset], to_write);
        data_write(inode->blocks[i], data_buf);

        // Update file size
        inode->size += to_write;
    }

    // Write count bytes from buffer to file blocks on disk
    bytes_written = 0;
    index_start = file->cursor / BLOCK_SIZE;
    for (i = index_start; bytes_written < count && i < INODE_ADDRS; i++) {
        // Allocate new data block if necessary
        if (i >= inode->used_blocks) {
            inode->blocks[i] = block_alloc();
            inode->used_blocks++;
        }

        // Read file data block from disk
        data_read(inode->blocks[i], data_buf);

        // Determine offset and bytes to write in block
        block_offset = file->cursor % BLOCK_SIZE;
        block_bytes = BLOCK_SIZE - block_offset;
        to_write = min(count - bytes_written, block_bytes);

        // Write bytes to data block on disk
        bcopy(
            (unsigned char *)&buf[bytes_written],
            (unsigned char *)&data_buf[block_offset],
            to_write
        );
        data_write(inode->blocks[i], data_buf);

        // Update cursor and byte count
        file->cursor += to_write;
        bytes_written += to_write;
    }

    // Write updated inode to disk
    inode_write(file->inode, inode_buf);

    return bytes_written;
}

int fs_lseek(int fd, int offset) {
    file_t *file;

    // Cannot set cursor if fd entry not open
    file = &fd_table[fd];
    if (!file->is_open) {
        return FAILURE;
    }

    // Set the cursor position and return it
    file->cursor = offset;
    return offset;
}

int fs_mkdir(char *fileName) {
    int inode_index;
    int result;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];

    // Fail if directory already exists
    if (dir_find_entry(wdir.inode, fileName) != FAILURE) {
        return FAILURE;
    }

    // Create inode for new directory if possible
    inode_index = inode_create(DIRECTORY);
    if (inode_index == FAILURE) {
        return FAILURE;
    }

    // Add self link to new directory
    result = dir_add_entry(inode_index, inode_index, ".");
    if (result == FAILURE) {
        inode_free(inode_index);
        return FAILURE;
    }

    // Add parent link to new directory
    result = dir_add_entry(inode_index, wdir.inode, "..");
    if (result == FAILURE) {
        inode_free(inode_index);
        return FAILURE;
    }

    // Link to new directory from working directory
    result = dir_add_entry(wdir.inode, inode_index, fileName);
    if (result == FAILURE) {
        inode_free(inode_index);
        return FAILURE;
    }

    return SUCCESS;
}

int fs_rmdir(char *fileName) {
    int inode_index;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    int result;

    // Cannot remove self or parent meta-entries
    if (same_string(fileName, ".") || same_string(fileName, "..")) {
        return FAILURE;
    }

    // Attempt to find entry in working directory
    inode_index = dir_find_entry(wdir.inode, fileName);
    if (inode_index == FAILURE) {
        return FAILURE;
    }

    // Fail if entry is not a directory
    inode = inode_read(inode_index, inode_buf);
    if (inode->type != DIRECTORY) {
        return FAILURE;
    }

    // Free directory inode on disk
    result = inode_free(inode_index);
    ASSERT(result != FAILURE);

    // Remove entry from working directory
    result = dir_remove_entry(wdir.inode, fileName);
    ASSERT(result != FAILURE);

    return SUCCESS;
}

int fs_cd(char *dirName) {
    if (same_string(dirName, ".")) {
        return SUCCESS;
    }

    // TODO: more stuff
}

int fs_link(char *old_fileName, char *new_fileName) {
    return -1;
}

int fs_unlink(char *fileName) {
    return -1;
}

int fs_stat(char *fileName, fileStat *buf) {
    return -1;
}

