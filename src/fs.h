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
int fs_ls_one(int index, char *buf);

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
#define INODE_PADDING 4

typedef struct {
    int size; // File size in bytes
    short type; // The file type (DIRECTORY, FILE_TYPE)
    short fd_count; // Number of open file descriptors
    short used_blocks; // Number of in-use data blocks
    short blocks[INODE_ADDRS]; // File data blocks
    char links; // Number of links to the i-node
    char _padding[INODE_PADDING];
} inode_t;

/* Directories ***************************************************************/

#define ROOT_DIR 0

#define ENTRY_PADDING 28

typedef struct {
    short inode; // Corresponding inode index on disk
    uint8_t in_use; // Is this entry currently in use?
    char name[MAX_FILE_NAME + 1]; // File name of the entry
    char _padding[ENTRY_PADDING];
} entry_t;

/* File descriptor table *****************************************************/

typedef struct {
    bool_t is_open; // Is this fd table entry open?
    int cursor; // Current r/w position in file (in bytes)
    short inode; // Corresponding inode index on disk
    short mode; // The file r/w mode (FS_O_RDONLY, FS_O_WRONLY, FS_ORDWR)
} file_t;

#endif