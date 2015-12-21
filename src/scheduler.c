/*	scheduler.c
	Best viewed with tabs set to 4 spaces.
*/
#include "interrupt.h"
#include "kernel.h"
#include "scheduler.h"
#include "usb.h"
#include "thread.h"
#include "util.h"
#include "time.h"

static int	eflags = INIT_EFLAGS;	// contents of EFlags when a job is started for the first time

//	Static prototypes
	//	Insert 'job' in the ready queue. 'job' is inserted after 'q'
	static void insert_job(pcb_t *job, pcb_t *q);
	//	Remove 'job' from the ready queue
	static void remove_job(pcb_t *job);


//	Call scheduler to run the 'next' process
void yield(void) {
	enter_critical();
	current_running->yield_count++;
	scheduler_entry();
	leave_critical();
}

/*	The scheduler must be called within a critical section since it changes global
	state, and since dispatch() needs to be called within a critical section. 
	Disable_count for current_running is checked to see that it's != 0. The scheduler
	also handles saving the current interrupt controller mask (which is later
	restored in setup_current_running()).
*/
void scheduler(void) {
	unsigned long long	t;
	
	/*	Save hardware interrupt mask in the pcb struct. The mask will be
		restored in setup_current_running()
	*/
	current_running->int_controller_mask	= inb(0x21);
	ASSERT(current_running->disable_count != 0);
	
	do {
		switch (current_running->status) {
			case SLEEPING:
				t	= get_timer();
				if (current_running->wakeup_time < t)
					current_running->status	= RUNNING;
				//	no break here
			case RUNNING:
				/* pick the next job to run */
				current_running = current_running->next;
				break;
			case BLOCKED:
				/* if no more jobs, halt */
				if (current_running->next == current_running) {
					HALT("No more jobs.");
				}

				current_running = current_running->next;
				/* Remove the job from the ready queue */
				remove_job(current_running->previous);
				break;
			case EXITED: 
				/* if no more jobs, loop forever */
				if (current_running->next == current_running) {
					HALT("No more jobs.");
				}

				current_running = current_running->next;
				/* Remove pcb from the ready queue and insert it into the free_pcb 
				 * queue */
				free_pcb(current_running->previous);
				break;
			default:
				HALT("Invalid job status.");
				break;
		}
	} while (current_running->status != RUNNING);

	/* .. and run it */
	dispatch();
}


//	Helper function for dispatch()
void	setup_current_running(void) {
	//	Restore harware interrupt mask
	outb(0x21, current_running->int_controller_mask);
	
	//	Load pointer to the page directory of current_running into CR3
	select_page_directory();
	reset_timer();
	
	if (current_running->inV86) {
	  tss.esp_0 = ss0 + STACK_OFFSET;
	  tss.ss_0  = KERNEL_DS;
	  asm volatile ("movl %%eax, %%cr3 "::"a"  (V86_page_directory));
	} else if (!current_running->is_thread) { /* process */
		/* Setup the kernel stack that will be loaded when the process
		 * is interrupted and enters the kernel. */
		tss.esp_0	= (uint32_t)current_running->base_kernel_stack;
		tss.ss_0	= KERNEL_DS;
	}
}

/*	Note: Dispatch should only be called within a critical section/with 
	interrupts off. It checks that disable_count != 0. 
*/
void dispatch(void) {
	setup_current_running();
	
	if (current_running->first_time) {
		//	Special case for the first time we run a process/thread
		current_running->first_time = FALSE;

		//	Create an empty kernel stack
		RESTORE_STACK( current_running->kernel_stack);
		if ( !current_running->is_thread) {
			PUSH(current_running->ds);				//	SS
			PUSH(current_running->user_stack);		//	ESP
		}
		PUSH(eflags);								//	EFlags
		PUSH(current_running->cs);					//	CS
		PUSH(current_running->start_pc);			//	EIP
		leave_critical();
		
		//	Load data segment descriptor into DS and ES
		asm volatile ("movw %%ax, %%ds"::"a"(current_running->ds));
		asm volatile ("movw %%ax, %%es"::"a"(current_running->ds));
	
		//PREVIOUS TWO LINES ARE REPLACED INSTEAD OF THE LINE BELOW. CAUSES PROBLEMS IN GCC 4.1.1
		//BECAUSE IT ALTERS THE STACK DURING THE FUNCTION CALL. -- SONER SEVINC
		//load_data_segments( current_running->ds);

		/*	When returning from interrupt, the processor:
			1. Pops start_pc (into EIP), CS and Eflags
			2. If current_running is a process it also restores
			   the user stack (by using the SS and ESP that were pushed above)
			3. Enables interrupts
		 */
		RETURN_FROM_INTERRUPT;
	}
	/*	Note: Disable_count of the selected process shouldn't 
		be 0 when we resume. This is because we should be
		inside a critical region 
	*/
	ASSERT(current_running->disable_count != 0);
	
	/*	The kernel stack was saved in scheduler_entry(). When it is restored
		here, we will return to scheduler_entry, as that is the last return
		address pushed on the stack. scheduler_entry() will then take care of
		restoring the context of the process, by poping registers off the stack, etc.
	*/
	RESTORE_STACK(current_running->kernel_stack);
	RETURN_FROM_PROCEDURE;
	/*	The processor pops the return address from the stack and returns to
		the function that called scheduler()
	*/
}


/*	Remove the current_running process from the linked list so it
	will not be scheduled in the future
*/
void exit(void) {
	enter_critical();
	current_running->status = EXITED;
	//	Removes job from ready queue, and dispatchs next job to run
	scheduler_entry();
	//	No leave_critical() needed, as we never return here. (The process is exiting!)
}

/*	q is a pointer to the waiting list where
	current_running should be inserted.
*/ 
void	block( pcb_t **q, int *spinlock) {
	pcb_t *tmp;
	
	enter_critical();
	
	if (spinlock)
		spinlock_release( spinlock);

	//	mark the job as blocked
	current_running->status = BLOCKED;

	//	Insert into waiting list
	tmp		= *q;
	(*q)	= current_running;
	current_running->next_blocked = tmp;

	//	remove job from ready queue, pick next job to run and dispatch it
	scheduler_entry();
	leave_critical();
}

/*	Unblocks the first process in the waiting queue (q),
	(*q) points to the last process.
*/
void	unblock(pcb_t **q) {
	pcb_t *new, *tmp;

	enter_critical();
	ASSERT( (*q) != NULL );
	
	if ( (*q)->next_blocked == NULL) {
		new = (*q);
		(*q) = NULL;
	} 
	/* (*q) not only pcb in queue */
	else {
		for (tmp = *q; 
			 tmp->next_blocked->next_blocked != NULL; 
			 tmp = tmp->next_blocked)
			/* do nothing */;
		
		new = tmp->next_blocked;
		/* new = last pcb in waiting queue */
		tmp->next_blocked = NULL;
	}
	
	new->status = RUNNING;
	/* Add the process to active process queue */
	insert_job(new, current_running);
	leave_critical();
}


int	getpid(void) {
	return current_running->pid;
}

int	getpriority(void) {
	return current_running->priority;
}

void	setpriority(int p) {
	current_running->priority = p;
}

int	cpuspeed(void) {
	return cpu_mhz;
}

//	Insert 'job' in queue the ready queue 'job' is inserted after q
static void insert_job(pcb_t *job, pcb_t *q) {
	job->next = q->next;
	job->previous = q;
	q->next->previous = job;
	q->next = job;
}

/* Remove 'job' from the ready queue */
static void remove_job(pcb_t *job) {
	job->previous->next = job->next;
	job->next->previous = job->previous;
}

