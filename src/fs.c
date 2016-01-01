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

static char sblock_buf[BLOCK_SIZE];

static sblock_t *sblock;

static void sblock_init(sblock_t *sblock) {
    sblock->fs_size = FS_SIZE;

    sblock->inode_start = SUPER_BLOCK + 1;
    sblock->inode_count = MAX_FILE_COUNT;
    sblock->inode_blocks = ceil_div(MAX_FILE_COUNT*sizeof(inode), BLOCK_SIZE);
    
    sblock->bamap_start = sblock->inode_start + sblock->inode_blocks;
    sblock->bamap_blocks = ceil_div(MAX_FILE_COUNT*sizeof(uint8_t), BLOCK_SIZE);
    
    sblock->data_start = sblock->bamap_start + sblock->bamap_blocks;
    sblock->data_blocks = MAX_FILE_COUNT;
}

static sblock_t *sblock_read(void) {
    block_read(SUPER_BLOCK, sblock_buf);
}

static void sblock_write(void) {
    block_write(SUPER_BLOCK, sblock_buf);
}

/* i-Nodes *******************************************************************/

static char inode_buf[2 * BLOCK_SIZE]; // inode may cross block boundary

static void inode_init(inode_t *inode, short type, short first_block) {
    int i;

    inode->type = type;
    inode->links = 0;
    inode->fd_count = 0;
    inode->size = 0;

    bzero(inode->blocks, sizeof(inode->blocks)); // GRUMP sizeof struct field
    inode->blocks[0] = first_block;
    inode->used_blocks = 1;
}

static int inode_block(int index) {
    int byte_offset, block_offset;
    byte_offset = index * sizeof(inode_t);
    block_offset = byte_offset / BLOCK_SIZE;
    return sblock->inode_start + block_offset;
}

static inode_t *inode_read(int index) {
    int block_index, inode_offset;
    inode_t *inode;

    block_index = inode_block(index);
    block_read(block_index, inode_buf);
    block_read(block_index + 1, &inode_buf[BLOCK_SIZE]); // GRUMP 2nd block

    inode_offset = (index * sizeof(inode_t)) % BLOCK_SIZE;
    inode = (inode_t *)&inode_buf[inode_offset];
    return inode;
}

static void inode_write(int index) {
    int block_index = inode_block(index);
    block_write(block_index, inode_buf);
    block_write(block_index + 1, &inode_buf[BLOCK_SIZE]); // GRUMP 2nd block
}

/* Block allocation map ******************************************************/

static char bamap_buf[BLOCK_SIZE];

static int bamap_block(int index) {
    return sblock->bamap_start + (index / BLOCK_SIZE);
}

static uint8_t *bamap_read(int index) {
    block_read(bamap_block(index), bamap_buf);
    return (uint8_t *)&bamap_buf[index % BLOCK_SIZE];
}

static void bamap_write(int index) {
    block_write(bamap_block(index), bamap_buf);
}

static int block_alloc(void) {
    int i, j;
    char block_buf[BLOCK_SIZE];
    for (i = 0; i < sblock->bamap_blocks; i++) {
        block_read(sblock->bamap_start + i, block_buf);
        for (j = 0; j < BLOCK_SIZE; j++) {
            if (!block_buf[j]) {
                block_buf[j] = TRUE;
                block_write(sblock->bamap_start + i, block_buf);
                return (i * BLOCK_SIZE) + j;
            }
        }
    }
    return FAILURE;
}

static void block_free(int index) {
    uint8_t *in_use;
    in_use = bamap_read(index);
    *in_use = FALSE;
    bamap_write(index);
}

/* Working directory *********************************************************/

static wdir_t wdir;

static void wdir_set(int inode) {
    wdir.inode = inode;
}

static void wdir_set_name(char *name) {
    str_copy(name, wdir.name);
}

static void wdir_set_path(char *path) {
    str_copy(path, wdir.path);
}

static void wdir_append_path(char *dirname) {
    str_append(dirname, wdir.path);
    str_append("/", wdir.path);
}

/* Directories ***************************************************************/

static int dir_add_entry(int dir_inode, short entry_inode, char *name) {
    int i, j;
    inode_t *inode;
    int block_entries, curr_entries;
    char block_buf[BLOCK_SIZE];
    entry_t *entries;
    entry_t new_entry;
    short new_block;

    inode = inode_read(dir_inode);

    block_entries = BLOCK_SIZE / sizeof(entry_t);
    curr_entries = inode->size / sizeof(entry_t);
    if (curr_entries >= block_entries * INODE_ADDRS) {
        return FAILURE;
    }

    new_entry.inode = entry_inode;
    str_copy(name, new_entry.name);

    for (i = 0; i < inode->used_blocks; i++) {
        block_read(inode->blocks[i], block_buf);
        entries = (entry_t *)block_buf;
        for (j = 0; j < block_entries; j++) {
            if (!entries[j].in_use) {
                entries[j] = new_entry;
                block_write(inode->blocks[i], block_buf);

                inode->size += sizeof(entry_t);
                inode_write(dir_inode);
                return SUCCESS;
            }
        }
    }

    new_block = block_alloc();
    inode->blocks[used_blocks] = new_block;
    inode->used_blocks++;

    bzero_block(block_buf);
    entries = (entry_t *)block_buf;
    entries[0] = new_entry;
    block_write(new_block, block_buf);

    inode->size += sizeof(entry_t);
    inode_write(dir_inode);
    return SUCCESS;
}

/* File descriptor table *****************************************************/

static file_t fd_table[MAX_FILE_COUNT];

static int fd_open_count;

static void fd_init(int fd) {
    file_t *file = &fd_table[fd];
    file->is_open = FALSE;
}

/* File system operations ****************************************************/

void fs_init(void) {
    block_init();

    // TODO: wtf i don't even know, man
}

int fs_mkfs(void) {
    int i;
    char block_buf[BLOCK_SIZE];
    inode_t *root_inode;
    uint8_t *root_in_use;

    // Format disk if necessary
    sblock = sblock_read();
    if (sblock->magic_num != SUPER_MAGIC_NUM) {
        // Zero out all file system blocks
        bzero_block(block_buf);
        for (i = 0; i < FS_SIZE; i++) {
            block_write(i, block_buf);
        }

        // Write super block to disk
        bzero_block((char *)sblock);
        sblock_init(sblock);
        sblock_write();

        // Create inode for root directory
        root_inode = inode_read(ROOT_DIR);
        inode_init(root_inode, DIRECTORY, sblock->data_start);
        inode_write(ROOT_DIR);

        // Mark root directory data block as used
        root_in_use = bamap_read(ROOT_DIR);
        *root_in_use = TRUE;
        bamap_write(ROOT_DIR);

        // Set up root directory with '.' and '..' entries
        dir_add_entry(ROOT_INODE, ROOT_INODE, ".");
        dir_add_entry(ROOT_INODE, ROOT_INODE, "..");
    }

    // Mount root as current working directory
    wdir_set(ROOT_DIR);
    wdir_set_name("");
    wdir_set_path("/");

    // Initialize the file descriptor table
    fd_open_count = 0;
    for (i = 0; i < MAX_FILE_COUNT; i++) {
        fd_init(i);
    }
}

int fs_open(char *fileName, int flags) {
    return -1;
}

int fs_close(int fd) {
    return -1;
}

int fs_read(int fd, char *buf, int count) {
    return -1;
}
    
int fs_write(int fd, char *buf, int count) {
    return -1;
}

int fs_lseek(int fd, int offset) {
    return -1;
}

int fs_mkdir(char *fileName) {
    return -1;
}

int fs_rmdir(char *fileName) {
    if (same_string(fileName, ".") || same_string(fileName, "..")) {
        return FAILURE;
    }

    // TODO: more stuff

    return -1;
}

int fs_cd(char *dirName) {
    return -1;
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

