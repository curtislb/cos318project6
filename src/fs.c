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
#define ERROR_MSG(m) printf("%s\n", m);
#else
#define ERROR_MSG(m)
#endif

/* Static helper functions ***************************************************/

static int ceil_div(int m, int n) {
    ASSERT(m > 0);
    ASSERT(n > 0);

    return ((m - 1) / n) + 1;
}

static int min(int x, int y) {
    return (x < y) ? x : y;
}

static void str_copy(char *src, char *dest) {
    bcopy((unsigned char *)src, (unsigned char *)dest, strlen(src) + 1);
}

/* Super block ***************************************************************/

static sblock_t *sblock;
static char sblock_buf[BLOCK_SIZE];

static void sblock_init(sblock_t *sblock) {
    ASSERT(sblock != NULL);

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
    ASSERT(block_buf != NULL);

    block_read(SUPER_BLOCK, block_buf);
    return (sblock_t *)block_buf;
}

static void sblock_write(char *block_buf) {
    ASSERT(block_buf != NULL);

    block_write(SUPER_BLOCK, block_buf);
}

/* Block allocation map ******************************************************/

static int bamap_block(int index) {
    ASSERT(index >= 0 && index < MAX_FILE_COUNT);

    return sblock->bamap_start + (index * sizeof(uint8_t) / BLOCK_SIZE);
}

static uint8_t *bamap_read(int index, char *block_buf) {
    ASSERT(index >= 0 && index < MAX_FILE_COUNT);
    ASSERT(block_buf != NULL);

    block_read(bamap_block(index), block_buf);
    return (uint8_t *)&block_buf[index % BLOCK_SIZE];
}

static void bamap_write(int index, char *block_buf) {
    ASSERT(index >= 0 && index < MAX_FILE_COUNT);
    ASSERT(block_buf != NULL);

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
    ERROR_MSG("block_alloc: No free blocks found");
    return FAILURE;
}

static void block_free(int index) {
    uint8_t *in_use;
    char block_buf[BLOCK_SIZE];

    ASSERT(index >= 0 && index < MAX_FILE_COUNT);

    in_use = bamap_read(index, block_buf);
    ASSERT(*in_use);

    *in_use = FALSE;
    bamap_write(index, block_buf);
}

/* Data blocks ***************************************************************/

static void data_read(int index, char *block_buf) {
    ASSERT(index >= 0 && index < MAX_FILE_COUNT);
    ASSERT(block_buf != NULL);

    block_read(sblock->data_start + index, block_buf);
}

static void data_write(int index, char *block_buf) {
    ASSERT(index >= 0 && index < MAX_FILE_COUNT);
    ASSERT(block_buf != NULL);

    block_write(sblock->data_start + index, block_buf);
}

/* i-Nodes *******************************************************************/

static void inode_init(inode_t *inode, int type) {
    ASSERT(inode != NULL);
    ASSERT(type == FILE_TYPE || type == DIRECTORY);

    inode->type = type;
    inode->links = 1;
    inode->fd_count = 0;
    inode->size = 0;
    bzero((char *)inode->blocks, sizeof(inode->blocks));
    inode->used_blocks = 0;
}

static int inode_block(int index) {
    ASSERT(index >= 0 && index < MAX_FILE_COUNT);

    int block_offset = index / (BLOCK_SIZE / sizeof(inode_t));
    return sblock->inode_start + block_offset;
}

static inode_t *inode_read(int index, char *block_buf) {
    inode_t *inodes;

    ASSERT(index >= 0 && index < MAX_FILE_COUNT);
    ASSERT(block_buf != NULL);

    // Read block containing inode from disk
    block_read(inode_block(index), block_buf);

    // Return pointer to inode struct in data buffer
    inodes = (inode_t *)block_buf;
    return &inodes[index % (BLOCK_SIZE / sizeof(inode_t))];
}

static void inode_write(int index, char *block_buf) {
    ASSERT(index >= 0 && index < MAX_FILE_COUNT);
    ASSERT(block_buf != NULL);

    block_write(inode_block(index), block_buf);
}

static int inode_create(int type) {
    int i, j;
    int block_inodes;
    char block_buf[BLOCK_SIZE];
    inode_t *inodes;

    ASSERT(type == FILE_TYPE || type == DIRECTORY);

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

    // No free inode entry found
    ERROR_MSG("inode_create: No free inode entry found");
    return FAILURE;
}

static void inode_free(int index) {
    int i;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];

    ASSERT(index >= 0 && index < MAX_FILE_COUNT);

    // Read inode from disk
    inode = inode_read(index, inode_buf);
    ASSERT(inode->type != FREE_INODE);

    // Free all data blocks used by inode
    for (i = 0; i < inode->used_blocks; i++) {
        block_free(inode->blocks[i]);
    }

    // Mark inode as free on disk
    inode->type = FREE_INODE;
    inode_write(index, inode_buf);
}

/* Directories ***************************************************************/

// Current working directory inode
static int wdir;

static void entry_init(entry_t *entry, int inode, char *name) {
    ASSERT(entry != NULL);
    ASSERT(inode >= 0 && inode < MAX_FILE_COUNT);
    ASSERT(name != NULL);

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

    ASSERT(dir_inode >= 0 && dir_inode < MAX_FILE_COUNT);
    ASSERT(entry_inode >= 0 && entry_inode < MAX_FILE_COUNT);
    ASSERT(name != NULL);

    // Read directory inode from disk
    inode = inode_read(dir_inode, inode_buf);

    // Fail if too many entries in directory
    block_entries = BLOCK_SIZE / sizeof(entry_t);
    curr_entries = inode->size / sizeof(entry_t);
    if (curr_entries >= block_entries * INODE_ADDRS) {
        ERROR_MSG("dir_add_entry: Too many entries in directory");
        return FAILURE;
    }

    // Initialize new directory entry
    entry_init(&new_entry, entry_inode, name);

    // Search for free entry in used blocks
    for (i = 0; i < inode->used_blocks; i++) {
        data_read(inode->blocks[i], data_buf);
        entries = (entry_t *)data_buf;
        for (j = 0; j < block_entries; j++) {
            //printf("add: entries[%d].in_use = %d\n", j, entries[j].in_use);
            //printf("add: entries[%d].name = %s\n", j, entries[j].name);
            if (!entries[j].in_use) {
                // Add new entry to directory
                //printf("add: new_entry.name = %s\n", new_entry.name);
                entries[j] = new_entry;
                //printf("add: entries[%d].name = %s\n", j, entries[j].name);
                data_write(inode->blocks[i], data_buf);

                // Write changes to inode on disk
                inode->size += sizeof(entry_t);
                inode_write(dir_inode, inode_buf);

                return SUCCESS;
            }
        }
    }

    //printf("Allocating new data block...\n");

    // All entries used, so try to allocate new block
    new_block = block_alloc();
    //printf("new_block = %d\n", new_block);
    if (new_block == FAILURE) {
        ERROR_MSG("dir_add_entry: Failed to allocate new data block");
        return FAILURE;
    }
    inode->blocks[inode->used_blocks] = new_block;
    inode->used_blocks++;

    // Add entry to newly allocated block
    bzero_block(data_buf);
    entries = (entry_t *)data_buf;
    entries[0] = new_entry;
    data_write(new_block, data_buf);

    // Write changes to inode on disk
    inode->size += sizeof(entry_t);
    inode_write(dir_inode, inode_buf);

    return SUCCESS;
}

static int dir_remove_entry(int dir_inode, char *name) {
    int i, j;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    char data_buf[BLOCK_SIZE];
    int block_entries;
    entry_t *entries;

    ASSERT(dir_inode >= 0 && dir_inode < MAX_FILE_COUNT);
    ASSERT(name != NULL);

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
                entries[j].in_use = FALSE;
                data_write(inode->blocks[i], data_buf);

                return SUCCESS;
            }
        }
    }

    // No matching entry found
    ERROR_MSG("dir_remove_entry: No matching entry found");
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
            //printf("find: entries[%d].in_use = %d\n", j, entries[j].in_use);
            //printf("find: entries[%d].name = %s\n", j, entries[j].name);
            if (entries[j].in_use && same_string(entries[j].name, name)) {
                return entries[j].inode;
            }
        }
    }

    // No matching entry found
    ERROR_MSG("dir_find_entry: No matching entry found")
    return FAILURE;
}

// static void dir_defragment(int dir_inode) {
//     // TODO: fill this in
// }

/* File descriptor table *****************************************************/

static file_t fd_table[MAX_FILE_COUNT];

static int fd_open(int inode, int mode) {
    int i;

    ASSERT(inode >= 0 && inode < MAX_FILE_COUNT);
    ASSERT(mode == FS_O_RDONLY || mode == FS_O_WRONLY || mode == FS_O_RDWR);

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
    ERROR_MSG("fd_open: No free entries found");
    return FAILURE;
}

static void fd_close(int fd) {
    ASSERT(fd >= 0 && fd < MAX_FILE_COUNT);
    ASSERT(fd_table[fd].is_open);

    fd_table[fd].is_open = FALSE;
}

/* File system operations ****************************************************/

void fs_init(void) {
    int i;
    int result;

    // Initialize block device
    block_init();

    // Format disk if necessary
    sblock = sblock_read(sblock_buf);
    if (sblock->magic_num != SUPER_MAGIC_NUM) {
        result = fs_mkfs();
        if (result == FAILURE) {
            ERROR_MSG("fs_init: Failed to format disk");
        }
    }

    // Set up working directory and file descriptor table
    else {
        // Mount root as current working directory
        wdir = ROOT_DIR;

        // Mark all file descriptor table entries as free
        for (i = 0; i < MAX_FILE_COUNT; i++) {
            fd_table[i].is_open = FALSE;
        }
    }
}

int fs_mkfs(void) {
    int i;
    char block_buf[BLOCK_SIZE];
    inode_t *inode;
    int result;

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
    inode_write(ROOT_DIR, block_buf);

    // Add "." self link meta-directory to root
    //printf("Adding '.' to root dir...\n");
    result = dir_add_entry(ROOT_DIR, ROOT_DIR, ".");
    if (result == FAILURE) {
        ERROR_MSG("fs_mkfs: Failed to add '.' to root directory");
        inode_free(ROOT_DIR);
        return FAILURE;
    }

    // Add ".." parent (self) link meta-directory to root
    //printf("Adding '..' to root dir...\n");
    result = dir_add_entry(ROOT_DIR, ROOT_DIR, "..");
    if (result == FAILURE) {
        ERROR_MSG("fs_mkfs: Failed to add '..' to root directory");
        inode_free(ROOT_DIR);
        return FAILURE;
    }

    // Mount root as current working directory
    wdir = ROOT_DIR;

    // Mark all file descriptor table entries as free
    for (i = 0; i < MAX_FILE_COUNT; i++) {
        fd_table[i].is_open = FALSE;
    }

    return SUCCESS;
}

int fs_open(char *fileName, int flags) {
    int entry_inode;
    int is_new_file = FALSE;
    int result;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    int fd;

    // Fail if file name is NULL
    if (fileName == NULL) {
        ERROR_MSG("fs_open: fileName cannot be NULL");
        return FAILURE;
    }

    // Fail if flags is not valid
    if (flags != FS_O_RDONLY && flags != FS_O_WRONLY && flags != FS_O_RDWR) {
        ERROR_MSG("fs_open: Received invalid flags argument");
        return FAILURE;
    }

    // Search for entry in working directory
    entry_inode = dir_find_entry(wdir, fileName);

    // If entry does not exist, attempt to create it
    if (entry_inode == FAILURE) {
        // Fail if trying to open non-existent file read-only
        if (flags == FS_O_RDONLY) {
            ERROR_MSG("fs_open: File does not exist and could not be created");
            return FAILURE;
        }

        // Create new inode for file
        entry_inode = inode_create(FILE_TYPE);
        if (entry_inode == FAILURE) {
            ERROR_MSG("fs_open: File does not exist and could not be created");
            return FAILURE;
        }

        // Add new file entry to working directory
        result = dir_add_entry(wdir, entry_inode, fileName);
        if (result == FAILURE) {
            ERROR_MSG("fs_open: File does not exist and could not be created");
            inode_free(entry_inode);
            return FAILURE;
        }

        // Indicate newly created file
        is_new_file = TRUE;
    }
    
    // Read inode from disk
    inode = inode_read(entry_inode, inode_buf);

    // Open entry in file descriptor table
    fd = fd_open(entry_inode, flags);
    if (fd == FAILURE) {
        ERROR_MSG("fs_open: Failed to create file descriptor table entry");

        // If new inode was allocated, then free it
        if (is_new_file) {
            inode_free(entry_inode);
        }

        return FAILURE;
    }

    // Increment open fd count for inode
    inode->fd_count++;
    inode_write(entry_inode, inode_buf);

    return fd;
}

int fs_close(int fd) {
    int inode_index;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];

    // Fail if given bad file descriptor
    if (fd < 0 || fd >= MAX_FILE_COUNT) {
        ERROR_MSG("fs_close: Received invalid file descriptor");
        return FAILURE;
    }

    // Fail if fd table entry is already closed
    if (!fd_table[fd].is_open) {
        ERROR_MSG("fs_close: Specified file descriptor is not open");
        return FAILURE;
    }

    // Read corresponding inode from disk
    inode_index = fd_table[fd].inode;
    inode = inode_read(inode_index, inode_buf);
    ASSERT(inode->type == FILE_TYPE);
    ASSERT(inode->fd_count > 0);

    // Close fd table entry
    fd_close(fd);

    // Decrement open fd count and delete file if necessary
    inode->fd_count--;
    if (inode->links == 0 && inode->fd_count == 0) {
        inode_free(inode_index);
    } else {
        // Write updated inode to disk
        inode_write(inode_index, inode_buf);
    }

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

    // If count is 0, return 0 immediately
    if (count == 0) {
        return 0;
    }

    // Fail if given bad file descriptor
    if (fd < 0 || fd >= MAX_FILE_COUNT) {
        ERROR_MSG("fs_read: Received invalid file descriptor");
        return FAILURE;
    }

    // Fail if buf is NULL
    if (buf == NULL) {
        ERROR_MSG("fs_read: buf cannot be NULL");
        return FAILURE;
    }

    // Fail if attempting to read negative number of bytes
    if (count < 0) {
        ERROR_MSG("fs_read: count cannot be negative");
        return FAILURE;
    }

    // Cannot read from file if fd entry not open
    file = &fd_table[fd];
    if (!file->is_open) {
        ERROR_MSG("fs_read: Specified file descriptor is not open");
        return FAILURE;
    }

    // Cannot read from file if opened write-only
    if (file->mode == FS_O_WRONLY) {
        ERROR_MSG("fs_read: Cannot read from write-only file");
        return FAILURE;
    }

    // Read file inode from disk
    inode = inode_read(file->inode, inode_buf);
    ASSERT(inode->type != FREE_INODE);

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
    int i, j;
    file_t *file;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    char data_buf[BLOCK_SIZE];
    int index_start;
    int old_used_blocks;
    int bytes_written;
    int block_offset;
    int block_bytes;
    int to_write;

    // If count is 0, return 0 immediately
    if (count == 0) {
        return 0;
    }

    // Fail if given bad file descriptor
    if (fd < 0 || fd >= MAX_FILE_COUNT) {
        ERROR_MSG("fs_write: Received invalid file descriptor");
        return FAILURE;
    }

    // Fail if buf is NULL
    if (buf == NULL) {
        ERROR_MSG("fs_write: buf cannot be NULL");
        return FAILURE;
    }

    // Fail if attempting to write negative number of bytes
    if (count < 0) {
        ERROR_MSG("fs_write: count cannot be negative");
        return FAILURE;
    }

    // Cannot write to file if fd entry not open
    file = &fd_table[fd];
    if (!file->is_open) {
        ERROR_MSG("fs_write: Specified file descriptor is not open");
        return FAILURE;
    }

    // Cannot write to file if opened read-only
    if (file->mode == FS_O_RDONLY) {
        ERROR_MSG("fs_write: Cannot write to read-only file");
        return FAILURE;
    }

    // Fail if cursor set after end of last data block
    if (file->cursor >= INODE_ADDRS * BLOCK_SIZE) {
        ERROR_MSG("fs_write: Cannot write beyond max file size");
        return FAILURE;
    }

    // Read file inode from disk
    inode = inode_read(file->inode, inode_buf);
    ASSERT(inode->type != FREE_INODE);

    // If cursor after end of file, pad with zeros up to cursor
    index_start = inode->size / BLOCK_SIZE;
    old_used_blocks = inode->used_blocks;
    for (i = index_start; inode->size < file->cursor; i++) {
        // Allocate new data block if necessary
        if (i >= inode->used_blocks) {
            inode->blocks[i] = block_alloc();
            if (inode->blocks[i] == FAILURE) {
                ERROR_MSG("fs_write: Failed to allocate new data block");

                // Free any newly allocated blocks on failure
                for (j = old_used_blocks; j < inode->used_blocks; j++) {
                    block_free(inode->blocks[j]);
                }

                return FAILURE;
            }
            inode->used_blocks++;
        }

        // Read file data block from disk
        data_read(inode->blocks[i], data_buf);

        // Determine offset and bytes to write in block
        block_offset = inode->size % BLOCK_SIZE;
        block_bytes = BLOCK_SIZE - block_offset;
        to_write = min(file->cursor - inode->size, block_bytes);

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
            if (inode->blocks[i] == FAILURE) {
                ERROR_MSG("fs_write: Failed to allocate new data block");

                // Free any newly allocated blocks on failure
                for (j = old_used_blocks; j < inode->used_blocks; j++) {
                    block_free(inode->blocks[j]);
                }

                return FAILURE;
            }
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

    // Fail if given bad file descriptor
    if (fd < 0 || fd >= MAX_FILE_COUNT) {
        ERROR_MSG("fs_lseek: Received invalid file descriptor");
        return FAILURE;
    }

    // Fail if offset is negative
    if (offset < 0) {
        ERROR_MSG("fs_lseek: offset cannot be negative");
        return FAILURE;
    }

    // Cannot set cursor if fd entry not open
    file = &fd_table[fd];
    if (!file->is_open) {
        ERROR_MSG("fs_lseek: Specified file descriptor is not open");
        return FAILURE;
    }

    // Set the cursor position and return it
    file->cursor = offset;
    return offset;
}

int fs_mkdir(char *fileName) {
    int inode_index;
    int result;

    // Fail if fileName is NULL
    if (fileName == NULL) {
        ERROR_MSG("fs_mkdir: fileName cannot be NULL");
        return FAILURE;
    }

    // Fail if directory already exists
    if (dir_find_entry(wdir, fileName) != FAILURE) {
        ERROR_MSG("fs_mkdir: Directory with given name already exists");
        return FAILURE;
    }

    // Create inode for new directory if possible
    inode_index = inode_create(DIRECTORY);
    if (inode_index == FAILURE) {
        ERROR_MSG("fs_mkdir: Failed to create new inode");
        return FAILURE;
    }

    // Add self link to new directory
    result = dir_add_entry(inode_index, inode_index, ".");
    if (result == FAILURE) {
        ERROR_MSG("fs_mkdir: Failed to add '.' to new directory");
        inode_free(inode_index);
        return FAILURE;
    }

    // Add parent link to new directory
    result = dir_add_entry(inode_index, wdir, "..");
    if (result == FAILURE) {
        ERROR_MSG("fs_mkdir: Failed to add '..' to new directory");
        inode_free(inode_index);
        return FAILURE;
    }

    // Link to new directory from working directory
    result = dir_add_entry(wdir, inode_index, fileName);
    if (result == FAILURE) {
        ERROR_MSG("fs_mkdir: Failed to add entry to working directory");
        inode_free(inode_index);
        return FAILURE;
    }

    return SUCCESS;
}

int fs_rmdir(char *fileName) {
    int inode_index;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];

    // Fail if fileName is NULL
    if (fileName == NULL) {
        ERROR_MSG("fs_rmdir: fileName cannot be NULL");
        return FAILURE;
    }

    // Cannot remove self or parent meta-directory entries
    if (same_string(fileName, ".") || same_string(fileName, "..")) {
        ERROR_MSG("fs_rmdir: Specified directory cannot be removed");
        return FAILURE;
    }

    // Attempt to find entry in working directory
    inode_index = dir_find_entry(wdir, fileName);
    if (inode_index == FAILURE) {
        ERROR_MSG("fs_rmdir: Specified directory does not exist");
        return FAILURE;
    }

    // Fail if entry is not a directory
    inode = inode_read(inode_index, inode_buf);
    if (inode->type != DIRECTORY) {
        ERROR_MSG("fs_rmdir: Specified file is not a directory");
        return FAILURE;
    }

    // Remove entry from working directory
    dir_remove_entry(wdir, fileName);

    // Decrement link count and delete directory if necessary
    inode->links--;
    if (inode->links == 0 && inode->fd_count == 0) {
        inode_free(inode_index);
    } else {
        // Write updated inode to disk
        inode_write(inode_index, inode_buf);
    }

    return SUCCESS;
}

int fs_cd(char *dirName) {
    int inode_index;

    // Don't change directory if attempting to cd to "."
    if (same_string(dirName, ".")) {
        return SUCCESS;
    }

    // Move to parent directory if attempting to cd to ".."
    if (same_string(dirName, "..")) {
        // Find inode of parent directory
        inode_index = dir_find_entry(wdir, "..");
        ASSERT(inode_index != FAILURE);

        // Set working directory to parent
        wdir = inode_index;

        return SUCCESS;
    }

    // Attempt to find entry in working directory
    inode_index = dir_find_entry(wdir, dirName);
    if (inode_index == FAILURE) {
        ERROR_MSG("fs_cd: Specified directory does not exist");
        return FAILURE;
    }

    // Update working directory
    wdir = inode_index;

    return SUCCESS;
}

int fs_link(char *old_fileName, char *new_fileName) {
    int inode_index;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    int result;

    // Fail if old_fileName is NULL
    if (old_fileName == NULL) {
        ERROR_MSG("fs_link: old_fileName cannot be NULL");
        return FAILURE;
    }

    // Fail if new_fileName is NULL
    if (new_fileName == NULL) {
        ERROR_MSG("fs_link: new_fileName cannot be NULL");
        return FAILURE;
    }

    // Fail if directory has file with same name as new link
    if (dir_find_entry(wdir, new_fileName) != FAILURE) {
        ERROR_MSG("fs_link: An entry with the given link name already exists");
        return FAILURE;
    }

    // Attempt to find old file in working directory
    inode_index = dir_find_entry(wdir, old_fileName);
    if (inode_index == FAILURE) {
        ERROR_MSG("fs_link: Specified file does not exist");
        return FAILURE;
    }

    // Read old file inode from disk, fail if not a file
    inode = inode_read(inode_index, inode_buf);
    ASSERT(inode->type != FREE_INODE);
    if (inode->type == DIRECTORY) {
        ERROR_MSG("fs_link: Cannot link a directory");
        return FAILURE;
    }

    // Attempt to add new link to working directory
    result = dir_add_entry(wdir, inode_index, new_fileName);
    if (result == FAILURE) {
        ERROR_MSG("fs_link: Failed to add link to working directory");
        return FAILURE;
    }

    // Increment link count of old file inode on disk
    inode->links++;
    inode_write(inode_index, inode_buf);
    
    return SUCCESS;
}

int fs_unlink(char *fileName) {
    int inode_index;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];

    // Fail if fileName is NULL
    if (fileName == NULL) {
        ERROR_MSG("fs_unlink: fileName cannot be NULL");
        return FAILURE;
    }

    // Attempt to find file in working directory
    inode_index = dir_find_entry(wdir, fileName);
    if (inode_index == FAILURE) {
        ERROR_MSG("fs_unlink: Specified file does not exist");
        return FAILURE;
    }

    // Read file inode from disk, fail if not a file
    inode = inode_read(inode_index, inode_buf);
    ASSERT(inode->type != FREE_INODE);
    if (inode->type == DIRECTORY) {
        ERROR_MSG("fs_unlink: Cannot unlink a directory");
        return FAILURE;
    }

    // Decrement link count and delete file if necessary
    inode->links--;
    if (inode->links == 0 && inode->fd_count == 0) {
        inode_free(inode_index);
    } else {
        // Write updated inode to disk
        inode_write(inode_index, inode_buf);
    }

    return SUCCESS;
}

int fs_stat(char *fileName, fileStat *buf) {
    int inode_index;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];

    // Fail if fileName is NULL
    if (fileName == NULL) {
        ERROR_MSG("fs_stat: fileName cannot be NULL");
        return FAILURE;
    }

    // Fail if buf is NULL
    if (buf == NULL) {
        ERROR_MSG("fs_stat: buf cannot be NULL");
        return FAILURE;
    }

    // Search for file in working directory
    inode_index = dir_find_entry(wdir, fileName);
    if (inode_index == FAILURE) {
        ERROR_MSG("fs_stat: Specified file does not exist");
        return FAILURE;
    }

    // Read inode from disk
    inode = inode_read(inode_index, inode_buf);
    ASSERT(inode->type != FREE_INODE);

    // Copy fields from inode to fileStat
    buf->inodeNo = inode_index;
    buf->type = inode->type;
    buf->links = inode->links;
    buf->size = inode->size;
    buf->numBlocks = inode->used_blocks;

    return SUCCESS;
}

int fs_ls_one(int index, char *buf) {
    return -1;
}
