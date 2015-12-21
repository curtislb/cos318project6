#include "common.h"
#include "kernel.h"
#include "util.h"
#include "block.h"
#include "usb.h"

#define START_SECTOR (MAX_IMAGE_SIZE/SECTOR_SIZE)

void block_init( void) {
    ASSERT( BLOCK_SIZE == SECTOR_SIZE );
}

void block_read( int block, char *mem) {
    if (block < 0 || block > 1024 * 2) {
	dprint("BUG READ?");
	print_int(0,0, block);
    }
    read(START_SECTOR+block, mem);
}

void block_write( int block, char *mem) {
    if (block < 0 || block > 1024 * 2) {
	dprint("BUG WRITE?");
    }
    write(START_SECTOR+block, mem);
}

void bzero_block( char *block) {
    int i;

    for ( i = 0; i < BLOCK_SIZE; i++)
	block[i] = 0;
}
