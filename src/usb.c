/*	usb.c
 *	instead of implementing a full scale usb driver to handle the flash
 *	drive, use the Virtual 8086 mode of the x86 processor and BIOS
 *	routines to read/write flash drive.
 *      written Tue Nov  2 01:10:45 EST 2004 by nitin@cs
*/

#include "kernel.h"
#include "common.h"
#include "memory.h"
#include "util.h"
#include "usb.h"
#include "thread.h"
#include "scheduler.h"
#include "sleep.h"
#include "interrupt.h"

#define USB_SAVE_REGS \
	asm volatile(" \
	    pusha; \
	    pushl %ds; \
	    pushl %es; \
	    pushl %fs; \
	    pushl %gs;")

#define USB_RESTORE_REGS \
	asm volatile(" \
	    popl %gs; \
	    popl %fs; \
	    popl %es; \
	    popl %ds; \
	    popa;")

#define USB_SETUP_FOR_IRET \
	asm volatile(" \
	    pushfl; \
	    push %%cs; \
	    subl $4, %%esp; \
	    movl %%esp, %0; \
	    addl $4, %%esp;":"=q"(v86_saved_stack))

uint32_t v86_saved_stack;
	  
#define USB_RESTORE_PROT_MODE \
	current_running->inV86 = 0; \
	asm volatile("movl %%eax, %%cr3 "::"a"(current_running->page_directory)); \
	if (!current_running->is_thread) { /* process */ \
	                  /* Setup the kernel stack that will be loaded when the process \
			   * is interrupted and enters the kernel. */ \
	  tss.esp_0	= (uint32_t)current_running->base_kernel_stack; \
	  tss.ss_0	= KERNEL_DS; \
	}

#define USB_SETUP_V86(x) \
	USB_SAVE_REGS; \
	USB_SETUP_FOR_IRET; \
	x(); \
	USB_RESTORE_REGS; \
	USB_RESTORE_PROT_MODE

#define USB_DO_V86(x) \
 \
  prot_to_v86_stack = (prot_to_v86_stack_t *)((ss3 << 4) + STACK_OFFSET - sizeof(prot_to_v86_stack_t)); \
  prot_to_v86_stack->eip = ((uint32_t)(x)) & 0xf;  \
  prot_to_v86_stack->cs  = ((uint32_t)(x)) >> 4;  \
  prot_to_v86_stack->gs  = prot_to_v86_stack->cs; \
  prot_to_v86_stack->fs  = prot_to_v86_stack->cs; \
  prot_to_v86_stack->ds  = prot_to_v86_stack->cs; \
  prot_to_v86_stack->es  = prot_to_v86_stack->cs; \
  prot_to_v86_stack->ss  = (uint32_t)ss3; \
  prot_to_v86_stack->esp = STACK_OFFSET; \
  prot_to_v86_stack->eflags = (uint32_t) 0x00020002;  /* eflags VM=1, IOPL=0 */ \
 \
  enter_critical(); \
  tss.esp_0 = ss0 + STACK_OFFSET;  \
  tss.ss_0  = KERNEL_DS; \
  asm volatile ("movl %%eax, %%cr3 "::"a"  (V86_page_directory)); \
  current_running->inV86 = 1;  \
  RESTORE_STACK((uint32_t) prot_to_v86_stack); /* switch to v86 stack */ \
  leave_critical(); \
  RETURN_FROM_INTERRUPT


#ifdef DEBUG
#define PRINT_STR print_str   /* call print_str */
#else
#define PRINT_STR(a,b,c)      /* do nothing */
#endif

uint32_t *make_v86_page_directory(void);

lock_t	usb_lock;

prot_to_v86_stack_t *prot_to_v86_stack;

uint32_t *V86_page_directory;
uint32_t ss0 = (STACK_MIN - STACK_SIZE);
uint16_t ss3 = (STACK_MIN - 2*STACK_SIZE)/16;
uint8_t  v86_buf[SECTOR_SIZE];
uint16_t v86_es;
uint16_t v86_bx;

uint32_t usb_directory_page = STACK_MIN - 2*STACK_SIZE - PAGE_SIZE;
uint32_t usb_table_page = STACK_MIN - 2*STACK_SIZE - 2*PAGE_SIZE;

void extract_usb_params(void) {
  USB_DO_V86(usb_params);
}

/* set up initial stuff */
void usb_init(void) 
{
// enable VME
  lock_init(&usb_lock);
  /* asm volatile("pushl %eax; \
      movl %cr4,%eax; \
      orl $0x1,%eax; \
      movl %eax,%cr4; \
      popl %eax");
  */

// find out disk params of usb disk

  v86_es = (uint16_t)(((uint32_t) v86_buf) >> 0x4);
  v86_bx = (uint16_t)(((uint32_t) v86_buf) & 0xf);

  // setup page dir for V86
  V86_page_directory = make_v86_page_directory();

  USB_SETUP_V86(extract_usb_params);
}

void usb_read_helper(void) {
  USB_DO_V86(usb_read);
}

void read(int block_num, unsigned char *buf) {
  
  uint8_t head, sector;
  uint16_t cylinder;

  sector = 1 + (block_num % params.sectors);
  head = (block_num % (params.sectors * params.heads))/params.sectors;
  cylinder = block_num/(params.sectors * params.heads);
      
  lock_acquire(&usb_lock);

  io_params.dh = head;
  io_params.cx = (((cylinder & 0x0300) >> 2) | sector) | ((cylinder & 0xff) << 8);
  io_params.dest_seg = v86_es;
  io_params.dest_off = v86_bx;

  USB_SETUP_V86(usb_read_helper);
	
  bcopy(v86_buf, buf, SECTOR_SIZE);

  lock_release(&usb_lock);
}

void usb_write_helper(void) {
  USB_DO_V86(usb_write);
}

void write(int block_num, unsigned char* buf) {

  uint8_t head, sector;
  uint16_t cylinder;

  sector = 1 + (block_num % params.sectors);
  head = (block_num % (params.sectors * params.heads))/params.sectors;
  cylinder = block_num/(params.sectors * params.heads);
      
  lock_acquire(&usb_lock);

  bcopy(buf, v86_buf, SECTOR_SIZE);

  io_params.dh = head;
  io_params.cx = (((cylinder & 0x0300) >> 2) | sector) | ((cylinder & 0xff) << 8);
  io_params.dest_seg = v86_es;
  io_params.dest_off = v86_bx;

  USB_SETUP_V86(usb_write_helper);

  lock_release(&usb_lock);
}

/*      Use virtual address to get index in a page table.
	The bits are masked, so we essentially get get a modulo 1024 index.
	The selection of which page table to index into is done with
	get_directory_index().
*/
static inline int usb_get_table_index(uint32_t vaddr) {
	return (vaddr & PAGE_TABLE_MASK) >> PAGE_TABLE_BITS;
}

/* Use virtual address to get index in page directory. */
static inline int usb_get_directory_index(uint32_t vaddr) {
    return (vaddr & PAGE_DIRECTORY_MASK) >> PAGE_DIRECTORY_BITS;
}

/*      Maps a page as present in the page table.
        'vaddr' is the virtual address which is mapped to the physical
	address 'paddr'.
	If user is nonzero, the page is mapped as accessible from a user
	application.
*/
static inline void usb_table_map_present(uint32_t* table, uint32_t vaddr, uint32_t paddr, int user) {
    int access  = PE_RW | PE_P,
	index   = usb_get_table_index(vaddr);

    if (user)
	access |= PE_US;

    table[index] = (paddr & PE_BASE_ADDR_MASK) | access;
}

static inline void usb_directory_insert_table(uint32_t* directory, uint32_t vaddr, uint32_t* table, int user) {
    int access  = PE_RW | PE_P,
	index   = usb_get_directory_index(vaddr);
    uint32_t        taddr;

    if (user)
	access |= PE_US;

    taddr = (uint32_t) table;

    directory[index] = (taddr & PE_BASE_ADDR_MASK) | access;
}

static void clear_page (void *page)
{
    unsigned char *p, *p_page = page;
    for (p = p_page; p < p_page + PAGE_SIZE; p++)
	*p = '\0';
}

uint32_t *make_v86_page_directory(void)
{
  uint32_t *directory, *table, addr;

  directory = (uint32_t *)usb_directory_page;
  clear_page(directory);
  table = (uint32_t *)usb_table_page;
  clear_page(table);

  for(addr = 0; addr < 0x100000; addr += PAGE_SIZE)
      usb_table_map_present(table, addr, addr, 1); // user access

  // wrap around at 1MB
//  for(addr = 0; addr < 0x10000; addr += PAGE_SIZE)
//      usb_table_map_present(table,0x100000 + addr, addr, 1);
  for (addr = MEM_START; addr < MAX_PHYSICAL_MEMORY; addr += PAGE_SIZE);
      usb_table_map_present(table, addr, addr, 1); // user access
  
  // see comment below
  // uint32_t kernel_start = (KERNEL_START/PAGE_SIZE)*PAGE_SIZE;
  // uint32_t v86_page     = (((uint32_t) v86_start)/PAGE_SIZE)*PAGE_SIZE;
  
  // Not setting kernel code pages to supervisor mode is dangerous as
  // the x86 program can trash it. But we arent running generic real
  // mode code. only the usb code which was also written by us and is
  // careful to not trash anything.
  // make kernel code supervisor access
  // for(addr = kernel_start; addr < v86_page; addr += PAGE_SIZE)
  //  usb_table_map_present(table, addr, addr, 0);

  usb_directory_insert_table(directory, 0, table, 1);
  return directory;
}

