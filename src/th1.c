/*	th1.c

	loader_thread is used to load the shell. 
	clock_thread  is a thread which runs indefinitely
	Best viewed with tabs set to 4 spaces.
*/
#include "kernel.h"
#include "scheduler.h"
#include "th.h"
#include "mbox.h"
#include "util.h"

#define MHZ 2392 /* CPU clock rate */

/*	This thread is started to load the user shell, which is 
	the first process in the directory.
*/ 
void loader_thread (void) {
	unsigned char		buf[SECTOR_SIZE];	//	buffer to hold directory
	struct directory_t	*dir = (struct directory_t *)buf;
	
	//	read process directory sector into buf
	readdir (buf ); 

	//	only load the first process, we assume it's the shell
	if (dir->location != 0 ) {
		loadproc (dir->location, dir->size );
	}
	else {
	  print_str(3,0,"Whooooaah! dir->location is 0!");
	}
	exit();
}

/*	This thread runs indefinitely, which means that the 
	scheduler should never run out of processes. 
*/
void clock_thread(void) {
	unsigned int		time;
	unsigned long long	ticks, start_ticks;
	
	start_ticks	= get_timer() >> 20;
	while(1) {
		ticks = get_timer() >> 20;	//	multiply with 2^10
		time = ((int)(ticks - start_ticks)) / MHZ;
		enter_critical();
		print_status(time);
		leave_critical();
		yield();
	}
}
