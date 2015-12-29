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

/* Data buffers and typed pointers *******************************************/

static char super_buf[BLOCK_SIZE];
static char inode_buf[CEIL_DIV(MAX_FILE_COUNT * INODE_SIZE, BLOCK_SIZE)];
static char bamap_buf[CEIL_DIV(MAX_FILE_COUNT, BLOCK_SIZE)];

static superblock_t *superblock;
static inode_t *inodes;
static uint8_t *ba_map;

/* Data R/W helper functions *************************************************/

static void block_read_n(int block, char *mem, int n) {
    int i;
    for (i = 0; i < n; i++) {
        block_read(block + i, mem + i*BLOCK_SIZE);
    }
}

static void block_write_n(int block, char *mem, int n) {
    int i;
    for (i = 0; i < n; i++) {
        block_write(block + i, mem + i*BLOCK_SIZE);
    }
}

static void str_copy(char *src, char *dest) {
    bcopy((uint8_t *)src, (uint8_t *)dest, strlen(src));
}

static void str_append(char *src, char *dest) {
    bcopy((uint8_t *)src, (uint8_t *)&dest[strlen(dest)], strlen(src));
}

/* Super block ***************************************************************/

static void superblock_init(void) {
    bzero_block(super_buf);

    sblock->magic_num = SUPER_MAGIC_NUM;
    sblock->fs_size = FS_SIZE;

    sblock->inode_start = SUPER_BLOCK + 1;
    sblock->inode_count = MAX_FILE_COUNT;
    sblock->inode_blocks = CEIL_DIV(MAX_FILE_COUNT * INODE_SIZE, BLOCK_SIZE);

    sblock->bamap_start = sblock->inode_start + sblock->inode_blocks;
    sblock->bamap_blocks = CEIL_DIV(MAX_FILE_COUNT, BLOCK_SIZE);

    sblock->data_start = sblock->bamap_start + bamap_blocks;
    sblock->data_blocks = MAX_FILE_COUNT;
}

static void superblock_read(void) {
    block_read(SUPER_BLOCK, super_buf);
}

static void superblock_write(void) {
    block_write(SUPER_BLOCK, super_buf);
}

/* i-Nodes *******************************************************************/

static void inodes_init(void) {
    bzero(inode_buf, sizeof(inode_buf));
}

static void inodes_read(void) {
    block_read_n(superblock->inode_start, inode_buf, superblock->inode_blocks);
}

static void inodes_write(void) {
    block_write_n(superblock->inode_start, inode_buf, superblock->inode_blocks);
}

static void inode_init(inode_t *inode, short type) {
    inode->type = type;
    inode->links = 0;
    inode->size = 0;
    bzero(inode->blocks, sizeof(inode->blocks)); // GRUMP sizeof struct field
}

/* Block allocation map ******************************************************/

static void ba_map_init(void) {
    bzero(bamap_buf, sizeof(bamap_buf));
}

static void ba_map_read(void) {
    block_read_n(superblock->bamap_start, bamap_buf, superblock->bamap_blocks);
}

static void ba_map_write(void) {
    block_write_n(superblock->bamap_start, bamap_buf, superblock->bamap_blocks);
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

static void wdir_path_append(char *dirname) {
    str_append(dirname, wdir.path);
    str_append(DIR_SEP, wdir.path);
}

/* File system operations ****************************************************/

void fs_init(void) {
    block_init();

    // Make data pointers refer to corresponding data buffers
    superblock = (superblock_t *)super_buf;
    inodes = (inode_t *)inode_buf;
    ba_map = (uint8_t *)bamap_buf;

    superblock_read();

    // Format disk if not already formatted
    if (superblock->magic_num != SUPER_MAGIC_NUM) {
        // Set up super block
        superblock_init();
        superblock_write();

        // Initialize root directory
        inodes_init();
        inode_init(&inodes[ROOT_DIR]);
        inodes_write();

        // Allocate space for root directory in block alloc map
        ba_map_init();
        ba_map[ROOT_DIR] = TRUE;
        ba_map_write();
    }

    // Otherwise, read inodes and block alloc map from disk
    else {
        inodes_read();
        ba_map_read();
    }

    // Mount root as current working directory
    wdir_set(ROOT_DIR);
    wdir_set_name("");
    wdir_set_path("/");
}

int fs_mkfs(void) {
    return -1;
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

