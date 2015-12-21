/*	interrupt.c
	Best viewed with tabs set to 4 spaces.
*/
#include "common.h"
#include "interrupt.h"
#include "kernel.h"
#include "keyboard.h"
#include "usb.h"
#include "memory.h"
#include "scheduler.h"
#include "util.h"


//	The following three variables are used by the dummy exception handlers.
static uint32_t cr2; /* address that caused a page fault exception */
static uint32_t esp; /* stack pointer */
static uint32_t *s;  /* pointer to stack */

/* Copy the segment descriptor index "seg" into the
 * DS and ES registers
 */ 
void load_data_segments(int seg) {
	asm volatile ("movw %%ax, %%ds \n\t"
		  "movw %%ax, %%es " 
		  ::"a"(seg));
}


/*	This function handles executing a given syscall, and returns the
	result to syscall_entry in entry.S, from where it is returned to
	the calling process. Before we get here, syscall_entry() will have
	stored the context of the process making the syscall, and entered
	a critical section (through enter_critical()).
	
	Note:
	The use of leave_critical() causes the interrupts to be turned on
	again after leave_critical. (cr->disable_count goes from 1 to 0 again.)
	
	This makes sense if we want system calls or other interrupt 
	handlers to be interruptable (for instance allowing a timer interrupt
	to preempt a process while it's inside the kernel in a system call).
		
	It does, however, also mean that we can get interrupts while we are
	inside another interrupt handler (the same thing is done in 
	the other interrupt handlers).
	
	In syslib.c we put systemcall number in eax, arg1 in ebx, arg2 in ecx
	and arg3 in edx. The return value is returned in eax.
	
	Before entering the processor has switched to the kernel stack 
	(PMSA p. 209, Privilege level switch whitout error code)
*/ 
int system_call_helper(int fn, int arg1, int arg2, int arg3) {
	int	ret_val = 0;
	
	ASSERT2(current_running->nested_count == 0, "A process/thread that was running inside the kernel made a syscall.");
	current_running->nested_count++;
	leave_critical();

	//	Call function and return result as usual (ie, "return ret_val");
	if (fn >= SYSCALL_COUNT || fn < 0) {
		//	Illegal system call number, call exit instead
		fn = SYSCALL_EXIT;
	}
	/*	In C's calling convention, caller is responsible for
		cleaning up the stack. Therefore we don't really need to
		distinguish between different argument numbers. Just pass all 3
		arguments and it will work
	*/
	ret_val	= syscall[fn] (arg1, arg2, arg3);
	
	//	We can not leave the critical section we enter here before we return in syscall_entry.
	//	This is due to a potential race condition on a scratch variable used by syscall_entry.
	enter_critical();
	current_running->nested_count--;
	ASSERT2(current_running->nested_count == 0, "Wrong nest count at B");
	return ret_val;
}

/*	Disable a hardware interrupt source by telling the controller to 
	ignore it. 
	
	NOTE: each thread/ process has its own interrupt mask, so this will
	only mask irqI for current_running. The other threads/processes can 
	still get an irq <irq> request.
	
	irq is interrupt number 0-7
*/
void	mask_hw_int(int irq) {
	unsigned char	mask;
	
	//	Read interrupt mask register
	mask	= inb(0x21);
	//	Disable <irq> by or'ing the mask with the corresponding bit
	mask	|= (1 << irq);
	//	Write interrupt mask register
	outb(0x21, mask);
}


//	Unmask the hardware interrupt source indicated by irq
void	unmask_hw_int(int irq) {
	unsigned char	mask;

	mask	= inb(0x21);
	mask	&= ~(1 << irq);
	outb(0x21, mask);
}


//	See Page 1007, Chapter 17, Warnings section in The Undocumented PC
void fake_irq7(void) {
#ifdef DEBUG
	static int fake_irq7_count = 0;
	static int iis_flag;
	
	/* Read Interrupt In-Service Register */
	outb(0x20,0x0b);
	iis_flag = inb(0x20);
	
	/* ASSERT2(IRQ 7 not in-service) */
	ASSERT2((iis_flag & (1 << 7)) == 0, "Real irq7 !");
	print_str(2, 0, "Fake irq7 count : ");
	print_int(2, 20, ++fake_irq7_count);
#endif
}

/*	Page fault exception. The location that generated the fault
	is stored in current_running->fault_addr. 
	page_fault_handler() is called with interrupts on. 
	Stack contents when entering from lower privilege level:
	Error Code, EIP, CS, Eflags, ESP, SS
	Stack contents when entering from same privilege level:
	Error Code, EIP, CS, Eflags
*/
void exception_14(unsigned long cr2, unsigned long err) {
	//	Used for debugging. Can also be extended to include privilege checks.
	current_running->error_code	= err;
	//	The address that caused the page fault is stored in cr->fault_addr
	current_running->fault_addr	= cr2;
	current_running->nested_count++;
	leave_critical();
	
	//	Call upon the page_fault_handler to do the dirty work!
	page_fault_handler();

	enter_critical();
	current_running->nested_count--;
}

/*	Exception handlers, currently they are all dummies (except exception 14, above).
	The following macro invocations are expanded into function definitions by the
	C preprocessor.
	Refer to PMSA p. 192 for exception categories
*/

/* Default interrupt handler */ 
INTERRUPT_HANDLER(bogus_interrupt, "In bogus_interrupt", FALSE);

/* Various exception handlers */ 
INTERRUPT_HANDLER(exception_0, "Excp. 0 - Divide by zero", FALSE);
INTERRUPT_HANDLER(exception_1, "Excp. 1 - Debug", FALSE);
INTERRUPT_HANDLER(exception_2, "Excp. 2 - NMI", FALSE);
INTERRUPT_HANDLER(exception_3, "Excp. 3 - Breakpoint", FALSE);
INTERRUPT_HANDLER(exception_4, "Excp. 4 - INTO instruction", FALSE);
INTERRUPT_HANDLER(exception_5, "Excp. 5 - BOUNDS instruction", FALSE);
INTERRUPT_HANDLER(exception_6, "Excp. 6 - Invalid opcode", FALSE);
INTERRUPT_HANDLER(exception_7, "Excp. 7 - Device not available", FALSE);
INTERRUPT_HANDLER(exception_8, "Excp. 8 - Double fault encountered", TRUE);
INTERRUPT_HANDLER(exception_9, "Excp. 9 - Coprocessor segment overrun", FALSE);
INTERRUPT_HANDLER(exception_10, "Excp. 10 - Invalid TSS Fault", TRUE);
INTERRUPT_HANDLER(exception_11, "Excp. 11 - Segment not Present", TRUE);
INTERRUPT_HANDLER(exception_12, "Excp. 12 - Stack exception", TRUE);
INTERRUPT_HANDLER(exception_13, "Excp. 13 - General Protection", TRUE);

void dump_state(uint32_t *s) {
	asm volatile ("movl %%cr2, %0" : "=q" (cr2)); 
	/* s points to top of the kernel stack */ 
	s = &(s[7]); // skip saved gen regs for now
	PRINT_INFO(0, "Error", print_str, "GPF - Exception 13"); 
	PRINT_INFO(1, "Stack", print_hex, ((int)s)); 
	PRINT_INFO(2, "Error code", print_hex, s[0]); 
	PRINT_INFO(3, "Saved EIP", print_hex, s[1]); 
	PRINT_INFO(4, "Saved CS", print_hex, s[2] & 0xffff); 
	PRINT_INFO(5, "Saved EFLAGS", print_hex, s[3]);
	PRINT_INFO(6, "Saved ESP", print_hex, s[4]); 
	PRINT_INFO(7, "Saved SS", print_hex, s[5] & 0xffff); 
	PRINT_INFO(8, "Preemptions", print_int, current_running->preempt_count);
	PRINT_INFO(9, "Yields", print_int, current_running->yield_count);
	PRINT_INFO(10, "nested count", print_int, current_running->nested_count);
	PRINT_INFO(11, "CR2, if pfault", print_hex, cr2);
	PRINT_INFO(12, "PID", print_int, current_running->pid);
//	print_status(-1);
	asm volatile ("hlt"); 
}

/* many thanks to tim robinson (http://osdev.berlios.de/v86.html) for the inspiration for the code below
 * its largely copied and adapted (to work with our variables etc) from the page above.
 * nitin@cs.princeton.edu, Mon Nov  8 03:14:20 EST 2004
 */

#define VALID_FLAGS         0xDFF
//#define DEBUG_GPF_TRACE
#ifdef DEBUG_GPF_TRACE
static int GPF_TRACE_LINE = 0;
#define GPF_TRACE(s)	print_str(GPF_TRACE_LINE++ % 20, 0, s)
#else
#define GPF_TRACE(s)
#endif
#define EFLAG_IF	0x00000200
#define EFLAG_VM	0x00020000

void gpf_handler(gpf_context_t *ctx) { 

  uint8_t *ip;
  uint16_t *stack, *ivt;
  uint32_t *stack32;
  bool_t is_operand32, is_address32;

  if (!(ctx->eflags & 0x20000)) // gpf not due to v86
    dump_state((uint32_t *)ctx);

  ip = FP_TO_LINEAR(ctx->cs, ctx->eip);
  ivt = (uint16_t*) 0;
  stack = (uint16_t*) FP_TO_LINEAR(ctx->ss, ctx->esp);
  stack32 = (uint32_t*) stack;
  is_operand32 = is_address32 = FALSE;

  while (TRUE) {
    switch (ip[0]) {
      case 0x66:            /* O32 */
        GPF_TRACE("o32");
        is_operand32 = TRUE;
	ip++;
        ctx->eip = (uint16_t) (ctx->eip + 1);
        break;

      case 0x67:            /* A32 */
        GPF_TRACE("a32 ");
	is_address32 = TRUE;
        ip++;
        ctx->eip = (uint16_t) (ctx->eip + 1);
	break;

      case 0x9c:            /* PUSHF */
        GPF_TRACE("pushf");

        if (is_operand32) { 
	  ctx->esp = ((ctx->esp & 0xffff) - 4) & 0xffff;
	  stack32--;
	  stack32[0] = ctx->eflags & VALID_FLAGS;

          if (current_running->v86_if) 
	    stack32[0] |= EFLAG_IF;
          else
            stack32[0] &= ~EFLAG_IF;
	} 
	else { 
	  ctx->esp = ((ctx->esp & 0xffff) - 2) & 0xffff;
	  stack--;
	  stack[0] = (uint16_t) ctx->eflags;

          if (current_running->v86_if)
             stack[0] |= EFLAG_IF;
          else
             stack[0] &= ~EFLAG_IF;
	} 

	ctx->eip = (uint16_t) (ctx->eip + 1);
	return;

      case 0x9d:            /* POPF */
        GPF_TRACE("popf");

        if (is_operand32) { 
	  ctx->eflags = EFLAG_IF | EFLAG_VM | (stack32[0] & VALID_FLAGS);
	  current_running->v86_if = (stack32[0] & EFLAG_IF) != 0; 
	  ctx->esp = ((ctx->esp & 0xffff) + 4) & 0xffff; 
	} 
	else { 
	  ctx->eflags = EFLAG_IF | EFLAG_VM | stack[0]; 
	  current_running->v86_if = (stack[0] & EFLAG_IF) != 0; 
	  ctx->esp = ((ctx->esp & 0xffff) + 2) & 0xffff;
	} 

	ctx->eip = (uint16_t) (ctx->eip + 1); 
	return;

      case 0xcd:            /* INT n */
        GPF_TRACE("interrupt"); 
	switch (ip[1]) {

	  default: 
	    stack -= 3; 
	    ctx->esp = ((ctx->esp & 0xffff) - 6) & 0xffff; 
	    stack[0] = (uint16_t) (ctx->eip + 2); 
	    stack[1] = ctx->cs; 
	    stack[2] = (uint16_t) ctx->eflags; 
	    
	    if (current_running->v86_if) 
	      stack[2] |= EFLAG_IF; 
	    else 
	      stack[2] &= ~EFLAG_IF;

	    ctx->cs = ivt[ip[1] * 2 + 1]; 
	    ctx->eip = ivt[ip[1] * 2]; 
	    return; 
	} 
	break; 
      
      case 0xcf:            /* IRET */ 
	GPF_TRACE("iret"); 
	ctx->eip = stack[0]; 
	ctx->cs = stack[1]; 
	ctx->eflags = EFLAG_IF | EFLAG_VM | stack[2]; 
	current_running->v86_if = (stack[2] & EFLAG_IF) != 0;

	ctx->esp = ((ctx->esp & 0xffff) + 6) & 0xffff;
	                
	return;

			        
      case 0xfa:            /* CLI */
        GPF_TRACE("cli");
        current_running->v86_if = 0;
        ctx->eip = (uint16_t) (ctx->eip + 1);
        return;

      case 0xfb:            /* STI */ 
	GPF_TRACE("sti"); 
	current_running->v86_if = 1;
	ctx->eip = (uint16_t) (ctx->eip + 1);
	return;

      case 0xec: /* inb %dx, %al */
	{ uint32_t eax;
	  asm volatile("mov %0, %%eax; \
	    mov %1, %%edx; \
	    in	%%dx,%%al"::"r" (ctx->eax), "r"(ctx->edx));
	  asm volatile("mov %%eax, %0":"=r"(eax));
	  eax &= 0x000000ff;
	  ctx->eax = (ctx->eax & 0xffffff00) | eax;
	}
	ctx->eip = (uint16_t) (ctx->eip + 1);
	return;
	
      case 0xee: /* outb %al,%dx */
	asm volatile("mov %0, %%eax; \
	    mov %1, %%edx; \
	    outb %%al, %%dx"::"r"(ctx->eax), "r"(ctx->edx));
	ctx->eip = (uint16_t) (ctx->eip + 1);
	return;

      case 0xef: /*outw %eax, %dx */
	asm volatile("mov %0, %%eax; \
	    mov %1, %%edx; \
	    out  %%eax, %%dx"::"r"(ctx->eax), "r"(ctx->edx));
	ctx->eip = (uint16_t) (ctx->eip + 1);
	return;

      case 0xf4: /* hlt */
	asm volatile("mov %0, %%esp"::"r"(v86_saved_stack));
	asm volatile("iret"); // end v86 mode.


     default: 
	print_str(0,0,"unhandled opcode");
	print_hex(1,0,ip[0]);
	dump_state((uint32_t *) ctx);
	return; 
    }
  } 
  return;
}

