/*
 * Author(s): Curtis Belmonte
 * COS 318, Fall 2015: Project 6 File System.
 * Implementation of a Unix-like file system.
*/
#ifndef FS_INCLUDED
#define FS_INCLUDED

#define FS_SIZE 2048

void fs_init(void);
int fs_mkfs(void);
int fs_open(char *fileName, int flags);
int fs_close(int fd);
int fs_read(int fd, char *buf, int count);
int fs_write(int fd, char *buf, int count);
int fs_lseek(int fd, int offset);
int fs_mkdir(char *fileName);
int fs_rmdir(char *fileName);
int fs_cd(char *dirName);
int fs_link(char *old_fileName, char *new_fileName);
int fs_unlink(char *fileName);
int fs_stat(char *fileName, fileStat *buf);

#define MAX_FILE_NAME 32
#define MAX_PATH_NAME 256 /* This is the maximum supported "full" path len,
    eg: /foo/bar/test.txt, rather than the maximum individual filename len. */
#define MAX_FILE_COUNT 1000

/* Super block ***************************************************************/

#define SUPER_BLOCK 0
#define SUPER_MAGIC_NUM 0xa455

typedef struct {
    uint16_t magic_num; // Indicates that disk is formatted
    uint32_t fs_size; // Size of file system in blocks

    uint32_t inode_start; // First block where inodes are stored
    uint32_t inode_count; // Number of inodes that can be allocated
    uint32_t inode_blocks; // Number of blocks allocated to store inodes

    uint32_t bamap_start; // First block of block allocation map
    uint32_t bamap_blocks; // Size of block allocation map in blocks

    uint32_t data_start; // First data block
    uint32_t data_blocks; // Number of data blocks that can be allocated
} superblock_t;

/* i-Nodes *******************************************************************/

#define INODE_ADDRS 8
#define INODE_SIZE 24

typedef struct {
    uint16_t type; // the file type (DIRECTORY, FILE_TYPE)
    uint8_t links; // number of links to the i-node
    uint32_t size; // file size in bytes
    uint16_t blocks[INODE_ADDRS]; // file data blocks
} inode_t;

/* Directories ***************************************************************/

#define DIR_SEP "/"
#define ROOT_DIR 0

typedef struct {
    int inode;
    char name[MAX_FILE_NAME];
    char path[MAX_PATH_NAME];
} wdir_t;

#endif