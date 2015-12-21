/*	interrupt.h
	Best viewed with tabs set to 4 spaces.
*/
#ifndef INTERRUPT_H
	#define INTERRUPT_H

#include "common.h"

//	Constants and macros
enum {
	NUM_EXCEPTIONS		= 15,
	//	Used by the exception handlers to define where to print messages
	START_COL			= 40,
};

/*	This macro clears a line, before printing a string to the line.
	Params:
	l: line, s: string, f: print function, v: value
*/
#define PRINT_INFO(l,s,f,v) { \
	/* clear line */ \
        int my_l = l; \
	print_str(my_l, START_COL, "                                       "); \
	/* print string */ \
	print_str(my_l, START_COL, s); \
	/* print value */ \
	f(my_l, START_COL+15, v); \
}

/*	This macro expands to a full function definition, allowing us to easily
	define multiple similar exception handlers without a lot of trouble. The
	paramters to the macro are:
	
	name: exception name
	str: Error string
	error_code: TRUE if exception has an error code 
*/
#define INTERRUPT_HANDLER(name, str, error_code) \
void name (void) { \
	/* switch to kernel data segment */ \
	load_data_segments(KERNEL_DS); \
	asm volatile ("movl %%esp, %0" : "=q" (esp)); \
	asm volatile ("movl %%cr2, %0" : "=q" (cr2)); \
	/* s points to top of the kernel stack */ \
	s = (uint32_t*) esp;\
	PRINT_INFO(0, "Error", print_str, (str)); \
	PRINT_INFO(1, "PID", print_int, current_running->pid); \
	/* print stack pointer */ \
	PRINT_INFO(2, "Stack", print_hex, ((int)s)); \
	PRINT_INFO(3, "Preemptions", print_int, current_running->preempt_count); \
	PRINT_INFO(4, "Yields", print_int, current_running->yield_count); \
	/* Stack: (Error code), EIP, CS... */ \
	PRINT_INFO(5, "EIP", print_hex, error_code ? s[1] : s[0]); \
	PRINT_INFO(6, "CS", print_hex, error_code ? s[2] : s[1]); \
	PRINT_INFO(7, "nested count", print_int, current_running->nested_count); \
	PRINT_INFO(8, "Error code", print_hex, error_code ? s[0] : 0); \
	PRINT_INFO(9, "Hardware mask", print_hex, \
		current_running->int_controller_mask); \
	PRINT_INFO(10, "CR2, if pfault", print_hex, cr2); \
	print_status(-1); \
	asm volatile ("hlt"); \
}

//	Prototypes

	//	Copy the segment descriptor index "seg" into the DS and ES registers
	void	load_data_segments(int seg);

	//	Helper function for system calls
	int		system_call_helper(int fn, int arg1, int arg2, int arg3);


	void	irq6(void);			//	Floppy interrupt
	void	fake_irq7(void);


	//	Mask/unmask a hardware interrupt source
	void	mask_hw_int(int irq);
	void	unmask_hw_int(int irq);

	/* Used for any other interrupt which should not happen */ 
	void	bogus_interrupt(void);

	/* Exception handlers, see PMSA Chapter 12 */ 
	void	exception_0(void);		/* Divide by zero 		*/ 
	void	exception_1(void);		/* Debug 			*/ 
	void	exception_2(void);		/* NMI				*/
	void	exception_3(void);		/* Breakpoint			*/ 
	void	exception_4(void);		/* INTO instruction 		*/
	void	exception_5(void);		/* BOUNDS instruction   	*/ 
	void	exception_6(void);		/* Invalid opcode		*/ 
	void	exception_7(void);		/* Device not available 	*/
	void	exception_8(void);		/* Double-fault encountered 	*/
	void	exception_9(void);		/* Coprocessor segment overrun  */ 
	void	exception_10(void);	/* Invalid TSS Fault		*/
	void	exception_11(void);	/* Segment not present		*/
	void	exception_12(void);	/* Stack exception		*/ 
	void	exception_13(void);	/* General Protection		*/
	void exception_14(unsigned long cr2, unsigned long err);	//	Page fault

	//	The following prototypes are for functions located in entry.S

	void	syscall_entry(void);

	/*	Entry points for the above interrupt handlers (irq0 doesn't have an
		explicit interrupt handler, nor does irq1 -- they call respectively
		yield and keyboard_interrupt() directly.
	*/
	void	irq0_entry(void);
	void	irq1_entry(void);
	void	irq6_entry(void);
	void	fake_irq7_entry(void);
	void	exception_14_entry(void);

	//	Enter/leave a critical region
	void	enter_critical(void);
	void	leave_critical(void);
	void	leave_critical_delayed(void);

/* segment:offset pair */
typedef uint32_t FARPTR;

/* Make a FARPTR from a segment and an offset */
#define MK_FP(seg, off)    ((FARPTR) (((uint32_t) (seg) << 16) | (uint16_t) (off)))

/* Extract the segment part of a FARPTR */
#define FP_SEG(fp)        (((FARPTR) fp) >> 16)

/* Extract the offset part of a FARPTR */
#define FP_OFF(fp)        (((FARPTR) fp) & 0xffff)

/* Convert a segment:offset pair to a linear address */
#define FP_TO_LINEAR(seg, off) ((void*) ((((uint16_t) (seg)) << 4) + ((uint16_t) (off))))

typedef struct {
  uint32_t	ebp,
  		esi,
		edi,
		edx,
		ecx,
		ebx,
		eax, // all saved by SAVE_GEN_REGS. Maintian same order
		error_code,
  		eip,
		cs,	// only lower 16 bits meaningful
		eflags,
		esp,	// only lower 16 bits meaningful if coming from v86 mode
		ss,
		es,
		ds,
		fs,
		gs;
} __attribute__((packed)) gpf_context_t;

void	exception13_entry(void);
void	gpf_handler(gpf_context_t *ctx);
	
#endif
