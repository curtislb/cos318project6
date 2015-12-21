/*	syslib.c
	
	Implementation of the system call library for the user
	processes. Each function implements a trap to the kernel. 
	
	Best viewed with tabs set to 4 spaces.
*/
#include "common.h"
#include "syslib.h"
#include "util.h"

/*	1.	Place system call number (i) in eax, arg1 in ebx, arg2 in ecx and 
		arg 3 in edx.
	2.	Trigger interrupt 48 (system call).
	3.	Return value is in eax after returning from interrupt.
*/
static int invoke_syscall(int i, int arg1, int arg2, int arg3) {
    int ret;

    asm volatile("int $48"   /* 48 = 0x30 */
		  : "=a" (ret) 
		  : "%0" (i), "b" (arg1), "c" (arg2), "d" (arg3));
    return ret;
}

void yield(void) { 
    invoke_syscall(SYSCALL_YIELD, IGNORE, IGNORE, IGNORE); 
}

void exit(void) { 
    invoke_syscall(SYSCALL_EXIT, IGNORE, IGNORE, IGNORE); 
}

int getpid(void) { 
    return invoke_syscall(SYSCALL_GETPID, IGNORE, IGNORE, IGNORE); 
}

int getpriority(void) { 
    return invoke_syscall(SYSCALL_GETPRIORITY, IGNORE, IGNORE, IGNORE); 
}

void setpriority(int p) 
{ 
    invoke_syscall(SYSCALL_SETPRIORITY, p, IGNORE, IGNORE); 
}

int	cpuspeed(void) {
	return invoke_syscall(SYSCALL_CPUSPEED, IGNORE, IGNORE, IGNORE);
}


int mbox_open(int key) {
    return invoke_syscall(SYSCALL_MBOX_OPEN, key, IGNORE, IGNORE); 
}

int mbox_close(int q) {
    return invoke_syscall(SYSCALL_MBOX_CLOSE, q, IGNORE, IGNORE); 
}

int mbox_stat(int q, int *count, int *space) {
    return invoke_syscall(SYSCALL_MBOX_STAT, q, (int)count, (int)space); 
}

int mbox_recv(int q, msg_t *m) {
    return invoke_syscall(SYSCALL_MBOX_RECV, q, (int)m, IGNORE);
}

int mbox_send(int q, msg_t *m) {
    return invoke_syscall(SYSCALL_MBOX_SEND, q, (int)m, IGNORE);
}

int getchar(int *c) {
    return invoke_syscall(SYSCALL_GETCHAR, (int)c, IGNORE, IGNORE);
}

int fs_mkfs( void) {
    return invoke_syscall( SYSCALL_MKFS, IGNORE, IGNORE, IGNORE); 
}

int fs_open( char *filename, int flags) {
    return invoke_syscall( SYSCALL_OPEN, ( int)filename, flags, IGNORE); 
}

int fs_close( int fd) {
    return invoke_syscall( SYSCALL_CLOSE, fd, IGNORE, IGNORE); 
}

int fs_read( int fd, char *buf, int count) {
    return invoke_syscall( SYSCALL_READ, fd, ( int)buf, count); 
}

int fs_write( int fd, char *buf, int count) {
    return invoke_syscall( SYSCALL_WRITE, fd, ( int)buf, count); 
}

int fs_lseek( int fd, int offset) {
    return invoke_syscall( SYSCALL_LSEEK, fd, offset, IGNORE); 
}

int fs_mkdir( char *fileName) {
    return invoke_syscall( SYSCALL_MKDIR, ( int)fileName, IGNORE, IGNORE); 
}

int fs_rmdir( char *fileName) {
    return invoke_syscall( SYSCALL_RMDIR, ( int)fileName, IGNORE, IGNORE); 
}

int fs_cd( char *pathName) {
    return invoke_syscall( SYSCALL_CD, ( int)pathName, IGNORE, IGNORE); 
}

int fs_link( char *pathName, char *fileName) {
    return invoke_syscall( SYSCALL_LINK, ( int)pathName, (int)fileName, IGNORE); 
}

int fs_unlink( char *fileName) {
    return invoke_syscall( SYSCALL_UNLINK, ( int)fileName, IGNORE, IGNORE); 
}

int fs_stat( char *fileName, fileStat *buf) {
    return invoke_syscall( SYSCALL_STAT, ( int)fileName, ( int)buf, IGNORE); 
}

void readdir (unsigned char *buf) {
    invoke_syscall (SYSCALL_READDIR, (int)buf, IGNORE, IGNORE);
}

void loadproc (int location, int size) {
    invoke_syscall (SYSCALL_LOADPROC, location, size, IGNORE);
}

void write_serial(int character)
{
    invoke_syscall(SYSCALL_WRITE_SERIAL, (int)character, IGNORE, IGNORE);
}
