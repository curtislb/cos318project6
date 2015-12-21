/*	thread.h

	Implementation of spinlocks, locks and condition variables
	Best viewed with tabs set to 4 spaces.
*/
#ifndef THREAD_H
	#define THREAD_H
	
//	Includes
	#include	"kernel.h"
	
//	Constants
enum {
	LOCKED		= 1,
	UNLOCKED	= 0
};
	
//	Typedefs
typedef struct {
	pcb_t	*waiting;	//	waiting queue
	int		status,		//	locked/ unlocked
			spinlock;
} lock_t;

typedef struct {
	int		spinlock;
	pcb_t	*waiting;	//	waiting queue
} condition_t;


//	Prototypes
	//	Spinlock functions
	void spinlock_init(int *s);
	void spinlock_acquire(int *s);
	void spinlock_release(int *s);

	//	Lock functions
	void lock_init(lock_t *);
	void lock_acquire(lock_t *);
	void lock_release(lock_t *);

	//	Initialize c
	void condition_init(condition_t *c);

	//	Unlock m and block on condition c, when unblocked acquire lock m
	void condition_wait(lock_t *m, condition_t *c);

	//	Unblock first thread enqued on c
	void condition_signal(condition_t *c);

	//	Unblock all threads enqued on c
	void condition_broadcast(condition_t *c);

#endif
