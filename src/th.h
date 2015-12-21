/*	th.h
	Prototypes for the threads.
	
	Best viewed with tabs set to 4 spaces.
*/
#ifndef TH_H
	#define TH_H

//	Loads shell
void loader_thread(void);

//	Runs indefinitely
void clock_thread(void);

//	Threads to test the condition variables and locks
void thread2(void);
void thread3(void);

#endif
