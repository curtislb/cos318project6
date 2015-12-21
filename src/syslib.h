/*	syslib.h
	Best viewed with tabs set to 4 spaces.
*/
#ifndef SYSLIB_H
	#define SYSLIB_H

//	Includes
	#include "common.h"

//	Constants
enum {
	IGNORE		= 0
};	

//	Prototypes for exported system calls
	void	yield(void);
	void	exit(void);
	int		getpid(void);
	int 	getpriority(void);
	void	setpriority(int);
	int		cpuspeed(void);
	int		mbox_open(int key);
	int 	mbox_close(int q);
	int		mbox_stat(int q, int *count, int *space);
	int		mbox_recv(int q, msg_t *m);
	int		mbox_send(int q, msg_t *m);
	int		getchar(int *c);
	void	readdir(unsigned char *buf);
	void	loadproc(int location, int size);
        void	write_serial(int character);

int fs_mkfs( void);
int fs_open( char *filename, int flags);
int fs_close( int fd);
int fs_read( int fd, char *buf, int count);
int fs_write( int fd, char *buf, int count);
int fs_lseek( int fd, int offset);
int fs_mkdir( char *fileName);
int fs_rmdir( char *fileName);
int fs_cd( char *pathName);
int fs_link( char *pathName, char *fileName);
int fs_unlink( char *fileName);
int fs_stat( char *fileName, fileStat *buf);

#endif
