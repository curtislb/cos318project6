#	entry.S
#	The code in this file serves as an entry point for most interrupts, as well
#	as providing the entry point for exception 14 (page fault). The file is
#	pre-processed before it is assembled, allowing (among other things) the use
#	of macros and C-style comments.

#	Save all registers, except eflags
#define SAVE_GEN_REGS \
	pushl %eax ;\
	pushl %ebx ;\
	pushl %ecx ;\
	pushl %edx ;\
	pushl %edi ;\
	pushl %esi ;\
	pushl %ebp;
	
#	Restore all registers
#define RESTORE_GEN_REGS \
	popl %ebp ;\
	popl %esi ;\
	popl %edi ;\
	popl %edx ;\
	popl %ecx ;\
	popl %ebx ;\
	popl %eax;

#define SAVE_EFLAGS \
	pushfl;

#define RESTORE_EFLAGS \
	popfl;	
	
#	Send an end-of-interrupt (equivalent to outb(0x20,0x20))
#define	SEND_EOI				\
	movb	$0x20, %al;			\
	movw	$0x20, %dx;			\
	outb	%al, %dx;
	
#	Standard pre-code for interrupts. The first macro specifies the action taken
#	by the timer interrupt and floppy interrupt. The second macro, HW_INT_PRE,
#	leaves the critical section before calling the interrupt handler, effectively
#	making the interrupt handler interruptable.
#	
#	The code first enters a critical region, before the context is saved, and 
#	the data segment is switched. The interrupt source is then masked, before
#	cr->nested_count and cr->preempt_count are incremented.
#define	HW_INT_PRE_IRQ0(x) 				\
	call	enter_critical;				\
	SAVE_GEN_REGS;						\
	pushl	%ds;						\
	pushl	$KERNEL_DS;					\
	call	load_data_segments;			\
	pushl	$x;							\
	call	mask_hw_int;				\
	addl	$8, %esp;					\
	movl	current_running, %eax;		\
	incl	CPCB_NESTED_COUNT(%eax);	\
	incl	CPCB_PREEMPT_COUNT(%eax);	\
	SEND_EOI;
	
#	Standard post-code for interrupts. Here we decrease cr->nested_count,
#	unmask the hardware interrupt source and restore the context. Note
#	the use of leave_critical_delayed -- we don't want to reenable interrupts
#	before the iret is executed (the iret handles reenabling interrupts for us,
#	by poping eflags off the stack).
#define	HW_INT_POST_IRQ0(x) 			\
	movl	current_running, %eax;		\
	decl	CPCB_NESTED_COUNT(%eax);	\
	pushl	$x;							\
	call	unmask_hw_int;				\
	addl	$4, %esp;					\
	call	leave_critical_delayed;		\
	popl	%ds;						\
	RESTORE_GEN_REGS;					\
	iret;
	
#	HW_INT_PRE and HW_INT_POST do the same thing as the macros above, as well
#	as leaving and entering the critical region.
#define	HW_INT_PRE(x)					\
	HW_INT_PRE_IRQ0(x);					\
	call	leave_critical;
	
#define	HW_INT_POST(x)					\
	call	enter_critical;				\
	HW_INT_POST_IRQ0(x);
	
.data
	.align 4
.text
.code32
	.align 4

#	Export the following functions
.globl	scheduler_entry
.globl	syscall_entry
.globl	irq0_entry
.globl	irq1_entry
#.globl	irq6_entry
.globl	fake_irq7_entry
.globl	exception13_entry
.globl	exception_14_entry
.globl	enter_critical
.globl	leave_critical
.globl	leave_critical_delayed
.globl  exc_14_eip
.globl 	exc_14_cs
.globl 	exc_14_a
.globl 	exc_14_b
	
#	Some constants. Note that if you change the pcb_t, you may have to recalculate
#	the constants.
.equ	CPCB_KERNEL_STACK,	12
.equ	CPCB_DISABLE_COUNT,	20
.equ	CPCB_PREEMPT_COUNT,	24
.equ	CPCB_NESTED_COUNT,	28

#	This should be the same as KERNEL_DS in kernel.h
.equ	KERNEL_DS,			2 << 3

#	This function gets called to enter the scheduler, saving registers before doing so.
scheduler_entry:
	# Save regs and eflags
	SAVE_GEN_REGS
	SAVE_EFLAGS
	movl	current_running, %eax
	# Simulate a push to the stack, so that the saved stack will include the return address to this function
	subl	$4, %esp
	movl	%esp, CPCB_KERNEL_STACK(%eax)
	addl	$4, %esp
	call	scheduler
	RESTORE_EFLAGS
	RESTORE_GEN_REGS
	ret
	
#	This is a temporary variable to store the value of %eax when the registers are restored.
syscall_return_val:
	.long	0
#	Syscalls are entered via this function. Registers are saved, before the arguments to the
#	syscall are pushed onto the stack. Then the data segment is switched, before the syscall
#	is handled. When the syscall returns, the arguments are popped off the stack, the return
#	value is saved to syscall_return_val (above), before the context is restored. Finally,
#	%eax is given the value we just stored in syscall_return_val, before the iret is executed.
syscall_entry:
	# Save registers
	SAVE_GEN_REGS
	# Save user data segment (code and stack segments already switched by processor)
	pushl	%ds
	
	# Push syscall arguments
	pushl	%edx	# Arg 3
	pushl	%ecx	# Arg 2
	pushl	%ebx	# Arg 1
	pushl	%eax	# Syscall number
	
	# Enter critical section
	call	enter_critical
	# Switch to kernel ds
	pushl	$KERNEL_DS
	call	load_data_segments
	addl	$4, %esp
	
	# System call helper will temporarily exit the critical region, but re-enters it before it returns.
	call	system_call_helper
	# Pop arguments
	addl	$16, %esp
	
	# Save return value
	movl	%eax, (syscall_return_val)
	
	# Restore user data segments
	popl	%ds
	RESTORE_GEN_REGS
	# Restore return value (do this before leaving critical, otherwise
	# a race condition may arise)
	movl	(syscall_return_val), %eax
	call	leave_critical
	iret
	
#	Timer interrupt. We call yield inside a critical section here, to avoid yield()
#	reenabling interrupts, and thus creating a nasty race condition. Note that this
#	function used HW_INT_PRE_IRQ0, which does not leave the critical region before
#	servicing the interrupt.
irq0_entry:
	HW_INT_PRE_IRQ0(0)
	call	yield
	HW_INT_POST_IRQ0(0)

# Keyboard interrupt. Save context and call the keyboard interrupt handler.
irq1_entry:
	HW_INT_PRE(1)
	call	keyboard_interrupt
	HW_INT_POST(1)

# Floppy interrupt.
# irq6_entry:
	# HW_INT_PRE(6)
	# call	irq6
	# HW_INT_POST(6)

# Fake irq7 (duh!)	
fake_irq7_entry:
	HW_INT_PRE_IRQ0(7)
	call	fake_irq7
	HW_INT_POST_IRQ0(7)
	
	
#	Page fault entry point. The code first enters a critical region, before it
#	saves off %eax in exc_14_scratch. Then the error code associated with the
#	page fault is popped off the stack, and stored in exc_14_err, before %eax is
#	restored, and the context is saved. 
#
#	The reason that we just don't calculate the offset on the stack where the error
#	code is, is primarily that if someone makes changes to SAVE_GEN_REGS, this code
#	will not fail due to a bad stack calculation.
#
#	Once the context has been saved, the error code and contents of the %cr2 registers
#	are pushed onto the stack, before the page fault handler is called.
exc_14_scratch:
	.long	0
exc_14_err:
	.long	0
exc_14_eip:	
	.long	0
exc_14_cs:	
	.long	0
exc_14_a:	
	.long	0
exc_14_b:	
	.long	0
exception_14_entry:
	call	enter_critical
	# Save %eax before modifying it
	movl	%eax, (exc_14_scratch)
	popl	%eax
	# Get error code
	movl	%eax, (exc_14_err)

	movl	(%esp), %eax
	movl	%eax, (exc_14_eip)
	movl	4(%esp), %eax
	movl	%eax, (exc_14_cs)
	movl	8(%esp), %eax
	movl	%eax, (exc_14_a)
	movl	-4(%esp), %eax
	movl	%eax, (exc_14_b)
	
	movl	(exc_14_scratch), %eax
	SAVE_GEN_REGS
	pushl	%ds
	pushl	$KERNEL_DS
	call	load_data_segments
	addl	$4, %esp
	# Push error code, and then contents of cr2
	movl	(exc_14_err), %eax
	pushl	%eax
	movl	%cr2, %eax
	pushl	%eax
	# exception_14 handles the page fault and leaving the critical region
	call	exception_14
	
	# Pop arguments
	addl	$8, %esp
	
	# Restore data segment and registers
	popl	%ds
	RESTORE_GEN_REGS
	call	leave_critical
	iret
	
# GPF -- Exception 13 handler
exception13_entry:
	SAVE_GEN_REGS
	pushl	$KERNEL_DS
	call	load_data_segments
	addl	$4, %esp
	movl	%esp, %eax
	pushl	%eax
	call	gpf_handler
	pop	%eax
	RESTORE_GEN_REGS
	addl	$4, %esp	# pop error code
	iret
	
#	enter_critical, leave_critical and leave_critical_delayed:
#	These functions enter and leave a critical region by disabling and
#	enabling interrupts. Interrupts are only enabled if current_running->disable_count == 0.
#	Note that they do not modify any registers! Also note that leave_critical_delayed requires
#	that cr->disable_count == 0 (if not, the computer is halted).
enter_critical:
	cli
	pushl	%eax
	xorl	%eax, %eax
	movw	%ds,%ax
	cmpl	$0, %eax
	jne	increment_count
	movl	$KERNEL_DS, %eax  # interrupted from v86. ds etc are null
	movw	%ax, %ds
	movw	%ax, %es
increment_count:
	movl	current_running, %eax
	incl	CPCB_DISABLE_COUNT(%eax)
	popl	%eax
	ret
	
leave_critical:
	pushl	%eax
	movl	current_running, %eax
	decl	CPCB_DISABLE_COUNT(%eax)
	cmpl	$0, CPCB_DISABLE_COUNT(%eax)
	popl	%eax
	jne		leave_critical_return
	sti
leave_critical_return:
	ret
	
leave_critical_delayed:
	pushl	%eax
	movl	current_running, %eax
	decl	CPCB_DISABLE_COUNT(%eax)
	cmpl	$0, CPCB_DISABLE_COUNT(%eax)
	jne		leave_crit_delayed_err
	popl	%eax
	ret
leave_crit_delayed_err:
	call	exception_1
	# Not reached:
	hlt
