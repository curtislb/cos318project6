/*
 * Author(s): Curtis Belmonte
 * COS 318, Fall 2015: Project 6 File System.
 * Implementation of a Unix-like file system.
*/
#include "util.h"
#include "common.h"
#include "shellutil.h"

#ifdef FAKE
#define START main
#include "fs.h"
#include <stdlib.h>
#else
#define START _start
#include "syslib.h"
#endif

char line[SIZEX+1];
char *argv[SIZEX];
int argc;


static void readLine( void);
static void parseLine( void);
static void usage( char *s);

static void shell_exit( void);
static void shell_fire( void);
static void shell_clearscreen( void);
static void shell_mkfs( void);
static void shell_open( void);
static void shell_read( void);
static void shell_write( void);
static void shell_lseek( void);
static void shell_close( void);
static void shell_mkdir( void);
static void shell_rmdir( void);
static void shell_cd( void);
static void shell_link( void);
static void shell_unlink( void);
static void shell_stat( void);

static void shell_ls( void);
static void shell_create( void);
static void shell_cat( void);

static void shell_listproc ( void );
static void shell_loadproc ( void );

#define EXEC_COMMAND( c, l, m, s, code) { \
    if ( same_string( c, argv[0])) { \
        if ( ( argc >= l) && ( argc <= m)) code; else usage( s); \
        continue; \
    } \
}

int START(void)
{
	writeStr( "ShellShock Version 0.000003");
	writeChar( RETURN);
	writeChar( RETURN);
	
	shell_init();
    
	while( 1) {
		writeStr( "# ");
		readLine();
		parseLine();

		if ( argc == 0)
			continue;

		EXEC_COMMAND( "exit",   1,  1, "", shell_exit());
		EXEC_COMMAND( "fire",   1,  1, "", shell_fire());
		EXEC_COMMAND( "clear",  1,  1, "", shell_clearscreen());
		EXEC_COMMAND( "mkfs",   1,  1, "", shell_mkfs());
		EXEC_COMMAND( "open",   3,  3, " <filename> <flag>",
			      shell_open());
		EXEC_COMMAND( "read",   3,  3, " <fd> <size>",
			      shell_read());
		EXEC_COMMAND( "write",  3,  3, " <fd> <string>",
			      shell_write());
		EXEC_COMMAND( "lseek",  3,  3, " <fd> <offset>",
			      shell_lseek());
		EXEC_COMMAND( "mkdir",  2,  2, " <dirname>", shell_mkdir());
		EXEC_COMMAND( "rmdir",  2,  2, " <dirname>", shell_rmdir());
		EXEC_COMMAND( "cd",     2,  2, " <dirname>", shell_cd());
		EXEC_COMMAND( "close",  2,  2, " <fd>", shell_close());
		EXEC_COMMAND( "link",   3,  3, " <src> <dest>", shell_link());
		EXEC_COMMAND( "unlink", 2,  2, " <name>", shell_unlink());
		EXEC_COMMAND( "stat",   2,  2, " <name>", shell_stat());
		EXEC_COMMAND( "ls",     1,  2, "", shell_ls());
		EXEC_COMMAND( "create", 3,  3, " <filename> <size>",
			      shell_create());
		EXEC_COMMAND( "cat",    2,  2, " <filename>", shell_cat());
		EXEC_COMMAND( "list",   1,  1, "", shell_listproc());
		EXEC_COMMAND( "load",   2,  2, "", shell_loadproc());
		writeStr( argv[0]);
		writeStr( " : Command not found.");
		writeChar( RETURN);
	}
	return 0;
}

static void usage( char *s) {
    writeStr( "Usage : ");
    writeStr( argv[0]);
    writeStr( s);
    writeChar( RETURN);
}

static void parseLine( void) {
    char *s = line;

    argc = 0;
    while(1) {
	while( *s == ' ') {
	    *s = 0;
	    s++;
	}
	if ( *s == 0)
	    return;
	else {
	    argv[argc++] = s;
	    while ( ( *s != ' ') && ( *s != 0))
		s++;
	}
    }
}

static void readLine( void) {
    int i = 0, c;

    do {
	readChar( &c);
	if ( ( i < MAX_LINE) && ( c != RETURN)) {
	    line[i++] = c;
	}
    } while ( c != RETURN);
    line[i] = 0;
}

static void shell_exit( void) {
    writeStr( "Goodbye\n"); 
#ifdef FAKE
    exit(0);
#else
    exit();
#endif
}

static void shell_fire( void) {
    fire();
}

static void shell_clearscreen( void) {
    clearShellScreen();
}

static void shell_mkfs( void) {
    if (fs_mkfs() != 0)
	writeStr("mkfs failed\n");
}

static void shell_create( void) {
    int fd, i;
    char letter[1];

    if ((fd = fs_open(argv[1], FS_O_RDWR)) == -1)
	writeStr("Error creating file");
    for(i=0; i < atoi(argv[2]); i++) {
	letter[0] = 'A' + (i % 37);
	if(fs_write(fd, letter, 1) == 0)
	  // error with fs_write
	  break;
	if ((i+1) % 40 == 0) {
	    letter[0] = RETURN;
	    if (fs_write(fd, letter, 1) == 0)
		break;
	}
    }
    fs_close(fd);
}

static void shell_open( void) {
    int i;
    char s[10];

    if ((i = fs_open(argv[1], atoi( argv[2]))) == -1)
	writeStr( "Error while opening file\n");
    else {
	itoa( i, s);
	writeStr( "File handle is : ");
	writeStr( s);
	writeChar( RETURN);
    }
}

static void shell_read( void) {
    char data[SIZEX];
    int i, n, count;

    n = atoi( argv[2]);
    if ( n > SIZEX) {
	writeStr( "Requested size too big\n");
	return;
    }
    if ( ( count = fs_read(atoi(argv[1]), data, n)) == -1)
	writeStr("Read failed\n");
    else {
	writeStr("Data read in : ");
	for ( i = 0; i < count; i++)
	    writeChar( data[i]);
	writeChar( RETURN);
    }
}

static void shell_write( void) {
    if ( fs_write( atoi(argv[1]), argv[2], strlen(argv[2])) == -1)
	writeStr("Error while writing file\n");
    else
	writeStr("Done\n");
}

static void shell_lseek( void) {
    if (fs_lseek(atoi(argv[1]), atoi(argv[2])) == -1)
	writeStr("Problem with seeking\n");
    else
	writeStr("OK\n");
}

static void shell_close( void) {
    if (fs_close(atoi(argv[1])) == -1)
	writeStr("Problem with closing file\n");
    else
	writeStr("OK\n");
}

static void shell_mkdir( void) {
    if (fs_mkdir( argv[1]) == -1)
	writeStr("Problem with making directory\n");
    else
	writeStr("OK\n");
}

static void shell_rmdir( void) {
    if (fs_rmdir(argv[1]) == -1)
	writeStr("Problem with removing directory\n");
    else
	writeStr("OK\n");
}

static void shell_cd( void) {
    if (fs_cd(argv[1]) == -1)
	writeStr("Problem with changing directory\n");
    else
	writeStr("OK\n");
}

static void shell_ls( void) {
    int i, j;
    char fileName[MAX_FILE_NAME + 1];
    fileStat status;
    char s[10];

    i = 0;
    if (fs_ls_one(i, fileName) != -1) {
        // Print column headers
        writeStr("Name");
        for (j = 0; j < MAX_FILE_NAME - 3; j++) {
            writeStr(" ");
        }
        writeStr("Type");
        writeStr(" ");
        writeStr("Inode");
        writeStr(" ");
        writeStr("Size\n");

        // Repeat for all valid directory entries
        do {
            // Read stat info for file
            if (fs_stat(fileName, &status) == -1) {
                writeStr("Problem with ls\n");
                return;
            }

            // Print file name
            writeStr(fileName);
            for (j = 0; j < MAX_FILE_NAME - strlen(fileName) + 1; j++) {
                writeStr(" ");
            }
            
            // Print abbreviated file type
            writeStr(status.type == DIRECTORY ? "D" : "F");
            writeStr("    ");
            
            // Print file inode number
            itoa(status.inodeNo, s);
            writeStr(s);
            writeStr("     ");
            
            // Print file size in bytes
            itoa(status.size, s);
            writeStr(s);
            writeStr("\n");

            i++;
        } while (fs_ls_one(i, fileName) != -1);
    } else {
        writeStr("Problem with ls\n");
    }
}

static void shell_link( void) {
    if (fs_link(argv[1], argv[2]) == -1)
	writeStr("Problem with link\n");
}

static void shell_unlink( void) {
    if (fs_unlink(argv[1]) == -1)
	writeStr("Problem with unlink\n");
}

static void shell_stat( void) {
    fileStat status;
    int ret;
    char s[10];

    ret = fs_stat( argv[1], &status);
    if ( ret == 0) {
	itoa( status.inodeNo, s);
	writeStr( "    Inode No         : "); writeStr( s); writeChar( RETURN);
	if ( status.type == FILE_TYPE)
	    writeStr( "    Type             : FILE\n");
	else
	    writeStr( "    Type             : DIRECTORY\n");
	itoa( status.links, s);
	writeStr( "    Link Count       : "); writeStr( s); writeChar( RETURN);
	itoa( status.size, s);
	writeStr( "    Size             : "); writeStr( s); writeChar( RETURN);
	itoa( status.numBlocks, s);
	writeStr( "    Blocks allocated : "); writeStr( s); writeChar( RETURN);
    } else
	writeStr( "Stat failed\n");
}

static void shell_cat( void) {
    int fd, n, i;
    char buf[256];

    fd = fs_open( argv[1], FS_O_RDONLY);
    if ( fd < 0) {
	writeStr( "Cat failed\n");
	return;
    }

    do {
	n = fs_read( fd, buf, 256);
	for ( i = 0; i < n; i++) 
	    writeChar( buf[i]);
    } while ( n > 0);
    fs_close( fd);
    writeChar( RETURN);
}

static void shell_listproc ( void ) {
#ifdef FAKE
  writeStr ( "Not supported in fake mode.\n" );
#else
  char buf[SECTOR_SIZE];
  struct directory_t *dir;
  int p;

  readdir (buf);
  dir = (struct directory_t*) buf;
  p = 0;
  while (dir->location != 0) {
    writeStr("process ");
    writeInt(p++);
    writeStr(" - location: ");
    writeInt(dir->location);
    writeStr(", size: ");
    writeInt(dir->size);
    writeChar(RETURN);
    dir++;
  }
  
  if (p == 0) {
    writeStr("Process not found.");
  } else {
    writeInt(p);
    writeStr(" process(es).");
  }
  writeChar(RETURN);

#endif

}

static void shell_loadproc ( void ) {
#ifdef FAKE
  writeStr ( "Not supported in fake mode.\n" );
#else
  char buf[SECTOR_SIZE];
  struct directory_t * dir;
  int n, p;

  readdir(buf);
  dir = (struct directory_t *) buf;
  n = atoi(argv[1]);
  for (p = 0; p < n && dir->location != 0; p++, dir++);
  
  if (dir->location != 0) {
    loadproc(dir->location, dir->size);
    writeStr("Done.");
  } else {
    writeStr("File not found.");
  }
  writeChar(RETURN);
  
#endif

}
