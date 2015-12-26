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

static superblock_t superblock;

void fs_init(void) {
    block_init();
    
    /* Format disk if is not already */
    block_read(SUPER_BLOCK, &superblock);
    if (superblock.magic_num != SUPER_MAGIC_NUM) {
        /* Intialize the superblock */
        bzero_block(&superblock);
        superblock_init(&superblock);
        block_write(SUPER_BLOCK, &superblock);


    }
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

