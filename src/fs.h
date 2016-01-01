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

#define SUCCESS 0
#define FAILURE -1

/* Super block ***************************************************************/

#define SUPER_BLOCK 0
#define SUPER_MAGIC_NUM 0xa455

typedef struct {
    int magic_num; // Indicates that disk is formatted
    int fs_size; // Size of file system in blocks

    int inode_start; // First block where inodes are stored
    int inode_count; // Number of inodes that can be allocated
    int inode_blocks; // Number of blocks set aside for inodes

    int bamap_start; // First block of block allocation map
    int bamap_blocks; // Number of blocks set aside for block alloc map

    int data_start; // First data block
    int data_blocks; // Number of data blocks that can be allocated
} sblock_t;

/* i-Nodes *******************************************************************/

#define INODE_ADDRS 8

typedef struct {
    short type; // the file type (DIRECTORY, FILE_TYPE)
    char links; // number of links to the i-node
    char fd_count; // number of open file descriptors
    int size; // file size in bytes
    short blocks[INODE_ADDRS]; // file data blocks
    uint8_t block_used[INODE_ADDRS]; // map of in-use block addresses
} inode_t;

/* Files and directories *****************************************************/

#define ROOT_DIR 0

// Working directory
typedef struct {
    short inode;
    char name[MAX_FILE_NAME];
    char path[MAX_PATH_NAME];
} wdir_t;

// Directory entry
typedef struct {
    uint8_t in_use;
    short inode;
    char name[MAX_FILE_NAME];
} entry_t;

// File descriptor table entry
typedef struct {
    uint8_t is_open;
    short inode;
    short mode;
    int cursor;
} file_t;

#endif