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

static void block_free(int index) {
    uint8_t *in_use;
    char block_buf[BLOCK_SIZE];



    in_use = bamap_read(index, block_buf);


    *in_use = FALSE;
    bamap_write(index, block_buf);
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
    inode->links = 1;
    inode->fd_count = 0;
    inode->size = 0;
    bzero((char *)inode->blocks, sizeof(inode->blocks));
    inode->used_blocks = 0;
}

static int inode_block(int index) {


    int block_offset = index / (BLOCK_SIZE / sizeof(inode_t));
    return sblock->inode_start + block_offset;
}

static inode_t *inode_read(int index, char *block_buf) {
    inode_t *inodes;




    // Read block containing inode from disk
    block_read(inode_block(index), block_buf);

    // Return pointer to inode struct in data buffer
    inodes = (inode_t *)block_buf;
    return &inodes[index % (BLOCK_SIZE / sizeof(inode_t))];
}

static void inode_write(int index, char *block_buf) {



    block_write(inode_block(index), block_buf);
}

static int inode_create(int type) {
    int block_inodes;
    int block;
    int inode;
    char block_buf[BLOCK_SIZE];
    inode_t *inodes;



    // Search for free inode entry
    block_inodes = BLOCK_SIZE / sizeof(inode_t);
    inodes = (inode_t *)block_buf;
    for (block = 0; block < sblock->inode_blocks; block++) {
        block_read(sblock->inode_start + block, block_buf);
        for (inode = 0; inode < block_inodes; inode++) {
            if (inodes[inode].type == FREE_INODE) {
                // Write the new inode to disk
                inode_init(&inodes[inode], type);
                block_write(sblock->inode_start + block, block_buf);

                // Return index of newly created inode
                return (block * block_inodes) + inode;
            }
        }
    }

    // No free inode entry found

    return FAILURE;
}

static void inode_free(int index) {
    int i;
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];



    // Read inode from disk
    inode = inode_read(index, inode_buf);


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

static int dir_add_entry(int dir_inode, int entry_inode, char *name) {
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    char data_buf[BLOCK_SIZE];
    int block_entries;
    int curr_entries;
    entry_t *entries;
    int block_index;
    int entry_offset;
    int new_block;





    // Read directory inode from disk
    inode = inode_read(dir_inode, inode_buf);


    // Fail if too many entries in directory
    block_entries = BLOCK_SIZE / sizeof(entry_t);
    curr_entries = inode->size / sizeof(entry_t);
    if (curr_entries >= block_entries * INODE_ADDRS) {

        return FAILURE;
    }

    // Determine index of data block and offset within block
    block_index = curr_entries / block_entries;
    entry_offset = curr_entries % block_entries;

    // Allocate new data block if necessary
    if (block_index >= inode->used_blocks) {
        new_block = block_alloc();
        if (new_block == FAILURE) {

            return FAILURE;
        }
        inode->blocks[block_index] = new_block;
        inode->used_blocks++;
    }

    // Add entry to data block
    data_read(inode->blocks[block_index], data_buf);
    entries = (entry_t *)data_buf;
    entries[entry_offset].inode = entry_inode;
    str_copy(name, entries[entry_offset].name);
    data_write(inode->blocks[block_index], data_buf);

    // Write changes to inode on disk
    inode->size += sizeof(entry_t);
    inode_write(dir_inode, inode_buf);

    return SUCCESS;
}

static int dir_remove_entry(int dir_inode, char *name) {
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    char data_buf[BLOCK_SIZE], last_data_buf[BLOCK_SIZE];
    entry_t *entries, *last_entries;
    int block, last_block;
    int entry, last_entry;
    int block_entries;
    int curr_entries;
    int entry_limit;




    // Read directory inode from disk
    inode = inode_read(dir_inode, inode_buf);


    // Search for matching entry in used blocks
    entries = (entry_t *)data_buf;
    block_entries = BLOCK_SIZE / sizeof(entry_t);
    curr_entries = inode->size / sizeof(entry_t);
    for (block = 0; block < inode->used_blocks; block++) {
        data_read(inode->blocks[block], data_buf);
        entry_limit = min(block_entries, curr_entries - block*block_entries);
        for (entry = 0; entry < entry_limit; entry++) {
            if (same_string(entries[entry].name, name)) {
                // Determine block index and offset of last entry
                last_block = inode->used_blocks - 1;
                last_entry = (curr_entries - 1) % block_entries;

                // Replace removed entry with last entry from disk
                data_read(inode->blocks[last_block], last_data_buf);
                last_entries = (entry_t *)last_data_buf;
                entries[entry] = last_entries[last_entry];
                data_write(inode->blocks[block], data_buf);

                // Free last data block if necessary
                if (last_entry == 0) {
                    block_free(inode->blocks[last_block]);
                    inode->used_blocks--;
                }

                // Update size of inode and write it to disk
                inode->size -= sizeof(entry_t);
                inode_write(dir_inode, inode_buf);

                return SUCCESS;
            }
        }
    }

    // No matching entry found

    return FAILURE;
}

static int dir_find_entry(int dir_inode, char *name) {
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    char data_buf[BLOCK_SIZE];
    entry_t *entries;
    int block_entries;
    int curr_entries;
    int block;
    int entry;
    int entry_limit;

    // Read directory inode from disk
    inode = inode_read(dir_inode, inode_buf);


    // Search for matching entry in used blocks
    entries = (entry_t *)data_buf;
    block_entries = BLOCK_SIZE / sizeof(entry_t);
    curr_entries = inode->size / sizeof(entry_t);
    for (block = 0; block < inode->used_blocks; block++) {
        data_read(inode->blocks[block], data_buf);
        entry_limit = min(block_entries, curr_entries - block*block_entries);
        for (entry = 0; entry < entry_limit; entry++) {
            // If entry matches, return its inode number
            if (same_string(entries[entry].name, name)) {
                return entries[entry].inode;
            }
        }
    }

    // No matching entry found

    return FAILURE;
}

/* File descriptor table *****************************************************/

static file_t fd_table[MAX_FD_ENTRIES];

static int fd_open(int inode, int mode) {
    int i;

    // Search for and open free fd table entry
    for (i = 0; i < MAX_FD_ENTRIES; i++) {
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

static void fd_close(int fd) {
    fd_table[fd].is_open = FALSE;
}

/* File system operations ****************************************************/

void fs_init(void) {
    // Initialize block device
    block_init();

    // Format disk if necessary
    sblock = sblock_read(sblock_buf);
    if (sblock->magic_num != SUPER_MAGIC_NUM) {
        fs_mkfs();
    }

    // Set up working directory and file descriptor table
    else {
        // Mount root as current working directory
        wdir = ROOT_DIR;

        // Initialize the file descriptor table
        bzero((char *)fd_table, sizeof(fd_table));
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
    result = dir_add_entry(ROOT_DIR, ROOT_DIR, ".");
    if (result == FAILURE) {
        inode_free(ROOT_DIR);
        return FAILURE;
    }

    // Add ".." parent (self) link meta-directory to root
    result = dir_add_entry(ROOT_DIR, ROOT_DIR, "..");
    if (result == FAILURE) {
        inode_free(ROOT_DIR);
        return FAILURE;
    }

    // Mount root as current working directory
    wdir = ROOT_DIR;

    // Initialize the file descriptor table
    bzero((char *)fd_table, sizeof(fd_table));

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
        return FAILURE;
    }

    // Fail if flags is not valid
    if (flags != FS_O_RDONLY && flags != FS_O_WRONLY && flags != FS_O_RDWR) {
        return FAILURE;
    }

    // Search for entry in working directory
    entry_inode = dir_find_entry(wdir, fileName);

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
        result = dir_add_entry(wdir, entry_inode, fileName);
        if (result == FAILURE) {
            inode_free(entry_inode);
            return FAILURE;
        }

        // Indicate newly created file
        is_new_file = TRUE;
    }
    
    // Read inode from disk
    inode = inode_read(entry_inode, inode_buf);

    // Fail if attempting to open directory in write mode
    if (inode->type == DIRECTORY && flags != FS_O_RDONLY) {
        return FAILURE;
    }

    // Open entry in file descriptor table
    fd = fd_open(entry_inode, flags);
    if (fd == FAILURE) {
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
        return FAILURE;
    }

    // Fail if fd table entry is already closed
    if (!fd_table[fd].is_open) {
        return FAILURE;
    }

    // Read corresponding inode from disk
    inode_index = fd_table[fd].inode;
    inode = inode_read(inode_index, inode_buf);

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
        return FAILURE;
    }

    // Fail if buf is NULL
    if (buf == NULL) {
        return FAILURE;
    }

    // Fail if attempting to read negative number of bytes
    if (count < 0) {
        return FAILURE;
    }

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
    int old_cursor;
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
        return FAILURE;
    }

    // Fail if buf is NULL
    if (buf == NULL) {
        return FAILURE;
    }

    // Fail if attempting to write negative number of bytes
    if (count < 0) {
        return FAILURE;
    }

    // Cannot write to file if fd entry not open
    file = &fd_table[fd];
    if (!file->is_open) {
        return FAILURE;
    }

    // Cannot write to file if opened read-only
    if (file->mode == FS_O_RDONLY) {
        return FAILURE;
    }

    // Fail if cursor set after end of last data block
    if (file->cursor >= INODE_ADDRS * BLOCK_SIZE) {
        return FAILURE;
    }

    // Read file inode from disk
    inode = inode_read(file->inode, inode_buf);


    // If cursor after end of file, pad with zeros up to cursor
    index_start = inode->size / BLOCK_SIZE;
    old_used_blocks = inode->used_blocks;
    for (i = index_start; inode->size < file->cursor; i++) {
        // Allocate new data block if necessary
        if (i >= inode->used_blocks) {
            inode->blocks[i] = block_alloc();
            if (inode->blocks[i] == FAILURE) {
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
    old_cursor = file->cursor;
    index_start = file->cursor / BLOCK_SIZE;
    for (i = index_start; bytes_written < count && i < INODE_ADDRS; i++) {
        // Allocate new data block if necessary
        if (i >= inode->used_blocks) {
            inode->blocks[i] = block_alloc();
            if (inode->blocks[i] == FAILURE) {
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
        // printf("block_offset = %d\n", block_offset);
        // printf("block_bytes = %d\n", block_bytes);
        // printf("to_write = %d\n", to_write);

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

    // Increase file size, write updated inode to disk
    inode->size += bytes_written - (inode->size - old_cursor);
    inode_write(file->inode, inode_buf);

    return bytes_written;
}

int fs_lseek(int fd, int offset) {
    file_t *file;

    // Fail if given bad file descriptor
    if (fd < 0 || fd >= MAX_FILE_COUNT) {
        return FAILURE;
    }

    // Fail if offset is negative
    if (offset < 0) {
        return FAILURE;
    }

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

    // Fail if fileName is NULL
    if (fileName == NULL) {
        return FAILURE;
    }

    // Fail if directory already exists
    if (dir_find_entry(wdir, fileName) != FAILURE) {
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
    result = dir_add_entry(inode_index, wdir, "..");
    if (result == FAILURE) {
        inode_free(inode_index);
        return FAILURE;
    }

    // Link to new directory from working directory
    result = dir_add_entry(wdir, inode_index, fileName);
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

    // Fail if fileName is NULL
    if (fileName == NULL) {
        return FAILURE;
    }

    // Cannot remove self or parent meta-directory entries
    if (same_string(fileName, ".") || same_string(fileName, "..")) {
        return FAILURE;
    }

    // Attempt to find entry in working directory
    inode_index = dir_find_entry(wdir, fileName);
    if (inode_index == FAILURE) {
        return FAILURE;
    }

    // Fail if entry is not a directory
    inode = inode_read(inode_index, inode_buf);
    if (inode->type != DIRECTORY) {
        return FAILURE;
    }

    // Fail if working directory contains additional entries
    if (inode->size > 2 * sizeof(entry_t)) {
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

        // Set working directory to parent
        wdir = inode_index;

        return SUCCESS;
    }

    // Attempt to find entry in working directory
    inode_index = dir_find_entry(wdir, dirName);
    if (inode_index == FAILURE) {
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
        return FAILURE;
    }

    // Fail if new_fileName is NULL
    if (new_fileName == NULL) {
        return FAILURE;
    }

    // Fail if directory has file with same name as new link
    if (dir_find_entry(wdir, new_fileName) != FAILURE) {
        return FAILURE;
    }

    // Attempt to find old file in working directory
    inode_index = dir_find_entry(wdir, old_fileName);
    if (inode_index == FAILURE) {
        return FAILURE;
    }

    // Read old file inode from disk, fail if not a file
    inode = inode_read(inode_index, inode_buf);

    if (inode->type == DIRECTORY) {
        return FAILURE;
    }

    // Attempt to add new link to working directory
    result = dir_add_entry(wdir, inode_index, new_fileName);
    if (result == FAILURE) {
        return FAILURE;
    }

    // Increment link count of old file inode on disk
    inode = inode_read(inode_index, inode_buf);
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
        return FAILURE;
    }

    // Attempt to find file in working directory
    inode_index = dir_find_entry(wdir, fileName);
    if (inode_index == FAILURE) {
        return FAILURE;
    }

    // Read file inode from disk, fail if not a file
    inode = inode_read(inode_index, inode_buf);

    if (inode->type == DIRECTORY) {
        return FAILURE;
    }

    // Remove file entry from directory
    dir_remove_entry(wdir, fileName);

    // Decrement link count and delete file if necessary
    inode = inode_read(inode_index, inode_buf);
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
        return FAILURE;
    }

    // Fail if buf is NULL
    if (buf == NULL) {
        return FAILURE;
    }

    // Search for file in working directory
    inode_index = dir_find_entry(wdir, fileName);
    if (inode_index == FAILURE) {

        return FAILURE;
    }

    // Read inode from disk
    inode = inode_read(inode_index, inode_buf);

    // Copy fields from inode to fileStat
    buf->inodeNo = inode_index;
    buf->type = inode->type;
    buf->links = inode->links;
    buf->size = inode->size;
    buf->numBlocks = inode->used_blocks;

    return SUCCESS;
}

int fs_ls_one(int index, char *buf) {
    inode_t *inode;
    char inode_buf[BLOCK_SIZE];
    char data_buf[BLOCK_SIZE];
    int block_entries;
    int block_index;
    int entry_offset;
    entry_t *entries;

    // Fail if index is negative
    if (index < 0) {
        return FAILURE;
    }

    // Fail if buf is NULL
    if (buf == NULL) {
        return FAILURE;
    }

    // Read working directory inode from disk
    inode = inode_read(wdir, inode_buf);

    // Fail if index is too large for directory
    if (index >= inode->size / sizeof(entry_t)) {
        return FAILURE;
    }

    // Determine index of data block and offset within block
    block_entries = BLOCK_SIZE / sizeof(entry_t);
    block_index = index / block_entries;
    entry_offset = index % block_entries;

    // Copy entry name from disk to string buffer
    data_read(inode->blocks[block_index], data_buf);
    entries = (entry_t *)data_buf;
    str_copy(entries[entry_offset].name, buf);

    return SUCCESS;
}
