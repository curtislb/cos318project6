#include "common.h"
#include "syslib.h"
#include "util.h"
#include "shellutil.h"

#define COMMAND_MBOX 1

#define MAXX (MINX+SIZEX)
#define MAXY (MINY+SIZEY)

#define PRINT_CHAR(y,x,c) \
if (((x)>=MINX) && ((x)<MAXX) && ((y)>=MINY) && ((y)<MAXY)) print_char(y,x,c)

int cursorX = MINX, cursorY = MINY;

static int q;

void 
shell_init( void) {
    q = mbox_open( COMMAND_MBOX);
}

void
writeChar( int c) {
    if ( c == RETURN_R || c == RETURN_N ) {
	cursorX = MINX;
	cursorY++;
    } else {
	PRINT_CHAR( cursorY, cursorX, c);
	if ( cursorX < MAXX-1) cursorX++;
    }

    if ( cursorY >= MAXY) {
	scroll( MINX, MINY, MAXX, MAXY);
	cursorY--;
    }
}

void
clearShellScreen( void) {
    clear_screen( MINX, MINY, MAXX, MAXY);
    cursorX = MINX;
    cursorY = MINY;
}

void
fire( void) {
    if ( q >= 0) {
	msg_t m;
	m.size = 0;
	mbox_send( q, &m);
    } else {
	writeStr( "Mailboxes not available");
	writeChar( RETURN);
    }
}

void
readChar( int *c) {
    getchar( c);
    writeChar( *c);
}

void
writeStr( char *s) {
    while( *s != 0) {
	writeChar( *s);
	s++;
    }
}

void 
writeInt( int i ) {
  char str[10];
  itoa ( i, str );
  writeStr ( str );
}
