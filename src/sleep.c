/*	sleep.c
	Contains functions to allow processes to sleep for some number of milliseconds.
	Best viewed with tabs set to 4 spaces.
*/

#include	"sleep.h"
#include	"util.h"
#include	"interrupt.h"
#include	"scheduler.h"
#include	"time.h"

extern pcb_t	*floppy_queue;
extern int		floppy_ints;


void	msleep(uint32_t msecs) {
	uint64_t	time;
	
	time	= get_timer();
	current_running->wakeup_time	= time + (msecs * cpu_mhz * 1000);
	current_running->status			= SLEEPING;
	yield();
	//	When we return here, we will have waited atleast <msecs> milliseconds.
}
