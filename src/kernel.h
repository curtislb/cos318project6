/*	kernel.h
	
	Various definitions used by the kernel and related code.
	Best viewed with tabs set to 4 spaces.
*/
#ifndef KERNEL_H
	#define KERNEL_H

//	Includes
	#include	"common.h"

//	Constants
enum {
	KERNEL_CODE						= 1,
	KERNEL_DATA,
	PROCESS_CODE,
	PROCESS_DATA,
	TSS_INDEX,
	
	//	Pointer to top of empty process stack
	PROCESS_STACK					= 0xEFFFFFF0,
	
	/*	These are used for the code, data and task state segment 
		registers. The numbers are indexes into the global descriptor
		table. Shiftlefted since the contents of the register is 
		more than just the index. (PMSA p. 80).
	*/ 
	KERNEL_CS						= KERNEL_CODE << 3,
	KERNEL_DS						= KERNEL_DATA << 3,
	PROCESS_CS						= PROCESS_CODE << 3,
	PROCESS_DS						= PROCESS_DATA << 3,
	KERNEL_TSS						= TSS_INDEX << 3,
	
	//	Segment descriptor types (used in create_segment)
	CODE_SEGMENT					= 0x0A,
	DATA_SEGMENT					= 0x02,
	TSS_SEGMENT						= 0x09,
	
	//	Used to set the system bit in create_segment()
	MEMORY							= 1,
	SYSTEM							= 0,
	
	//	Used to define size of a TSS segment
	TSS_SIZE						= 104,
	
	/*	Used to set Timer 0 frequency 
		For more details on how to set PREEMPT_TICKS refer to:
		The Undocumented PC p.978-979 (Port 40h).
		Alternate values are for instance 1193 (1000 Hz) and
		119 (10 000 Hz). The last value will be too have the timer
		interrupt fire too often, so try something higher!
	*/
	PREEMPT_TICKS					= 11932,	//	Timer interrupt at 100 Hz
	
	/*	Number of threads initially started by the kernel. Change
		this when adding to or removing elements from the start_addr array.
	*/
	NUM_THREADS						= 4,
	
	//	Number of pcbs the OS supports
	PCB_TABLE_SIZE					= 128,

	//	kernel stack allocator constants
	STACK_MIN						= 0x30000,
	STACK_MAX						= 0x80000,
	STACK_OFFSET					= 0x0FFC,
	STACK_SIZE						= 0x1000,

	/* 
	 * IDT - Interrupt Descriptor Table 
	 * GDT - Global Descriptor Table 
	 * TSS - Task State Segment 
	 */ 
	GDT_SIZE        				= 8,
	IDT_SIZE        				= 49,
	IRQ_START       				= 32,	//	remapped irq0 IDT entry */
	INTERRUPT_GATE					= 0x0E,	//	interrupt gate descriptor */
	IDT_SYSCALL_POS					= 48,	//	system call IDT entry */
};


//	Typedefs

/*	The process control block is used for storing various information about
	a thread or process
*/
typedef struct pcb_t {
	uint32_t		pid,					//	Process id of this process
					is_thread,				//	Thread or process
					user_stack,				//	Pointer to base of the user stack
					kernel_stack,			/*	Used to set up kernel stack, and
												temporary save esp in scheduler()/dispatch() */
					base_kernel_stack,		/*	Pointer to base of the kernel stack 
												(used to restore an empty ker. st.) */
					disable_count,			/*	When d_c == 0, we can turn on interrupts. See enter_critical
												and leave_critical in entry.S */
					preempt_count,			//	Number of times process has been preempted
					nested_count,			//	Number of nested interrupts
					start_pc,				//	Start address of a process or thread
					ds,						//	Data segment selector
					cs,						//	Code segment selector
					fault_addr,				//	Location that generated a page fault
					error_code,				//	Error code associated with a page fault
					swap_loc,				//	Swap space base address
					swap_size,				//	Size of this process
					first_time,				//	True before this process has had a chance to run
					priority,				//	This process' priority
					status,					//	RUNNING, BLOCKED, SLEEPING or EXITED
					page_fault_count,		//	Number of page faults
					yield_count,			//	Number of yields made by this process
					int_controller_mask;	/*	Interrupt controller mask (bit x = 0,
												enable irq x). */
	uint64_t		wakeup_time;			/*	Time at which this process should transition 
												from SLEEPING to RUNNING. */
	struct pcb_t	*next_blocked;			//	Used when job is in some waiting queue
	uint32_t*		page_directory;			//	Virtual memory page directory
	struct pcb_t	*next,					//	Used when job is in the ready queue
					*previous;
	uint32_t	inV86; // set when in virtual 86 mode.
	uint32_t	v86_if; // true when interrupts are enabled.
} pcb_t;

/*	Structure describing the contents of an interrupt gate entry.
	(Protected Mode Software Architecture p. 206) */
struct gate_t {
	uint16_t offset_low;
	uint16_t selector;
	uint8_t count;
	uint8_t access;
	uint16_t offset_high;
} __attribute__((packed));

/*	Structure describing the contents of a segment descriptor entry
	(Protected Mode Software Architecture p.95) */
struct segment_t {
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_mid;
	uint8_t access;
	uint8_t limit_high;
	uint8_t base_high;
} __attribute__((packed));


/*	Structure describing the contents of the idt and gdt registers. 	
	Used for loading the idt and gdt registers. 
	(Protected Mode Software architecture p.42, ) */
struct point_t {
	uint16_t limit;
	uint32_t base;
} __attribute__((packed));

/*	Structure describing the contents of a Task State Segment
	(Protected Mode Software architecture p.141) */
typedef struct tss_t {
	uint32_t backlink;
	uint32_t esp_0;
	uint16_t ss_0;
	uint16_t pad0;
	uint32_t esp_1;
	uint16_t ss_1;
	uint16_t pad1;
	uint32_t esp_2;
	uint16_t ss_2;
	uint16_t pad2;
	uint32_t reserved;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	uint32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	uint16_t es;
	uint16_t pad3;
	uint16_t cs;
	uint16_t pad4;
	uint16_t ss;
	uint16_t pad5;
	uint16_t ds;
	uint16_t pad6;
	uint16_t fs;
	uint16_t pad7;
	uint16_t gs;
	uint16_t pad8;
	uint16_t ldt_selector;
	uint16_t pad9;
	uint16_t debug_trap;
	uint16_t iomap_base;
	// uint32_t interrupt_redirect[8];
	uint8_t iomap[8193];
} __attribute((packed)) tss_t;

//	Defining a function pointer is easier when we have a type.
typedef	int		(*syscall_t)();		//	Syscalls return an int. Don't specify arguments!
typedef	void	(*handler_t)(void);	//	Exception handlers don't return anythings

//	Variables
/*	Global table with pointers to the kernel functions implementing
	the system calls. System call number 0 has index 0 here.
*/
extern syscall_t syscall[SYSCALL_COUNT];

//	An array of pcb structures we can allocate pcbs from
extern pcb_t pcb[];

//	The currently running process, and also a pointer to the ready queue
extern pcb_t *current_running;

//	The global shared tss for all processes
extern tss_t tss;

//	Prototypes
	//	Load pointer to the page directory of current_running into the CR3 register. 
	void	select_page_directory(void);
	//	Print some debug info
	void	print_status(int time);
	//	Reset timer 0 to the frequency specified by PREEMPT_TICKS
	void	reset_timer(void);
	
#endif
