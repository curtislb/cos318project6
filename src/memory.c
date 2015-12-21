/*	memory.c

	Note: 
	There is no separate swap area. When a data page is swapped out, 
	it is stored in the location it was loaded from in the process' 
	image. This means it's impossible to start two processes from the 
	same image without screwing up the running. It also means the 
	disk image is read once. And that we cannot use the program disk.

	Best viewed with tabs set to 4 spaces.
*/
#include "common.h"
#include "kernel.h"
#include "scheduler.h"
#include "memory.h"
#include "thread.h"
#include "util.h"
#include "interrupt.h"
#include "usb.h"

//	Static prototypes
	/*	page_alloc allocates a page.  If necessary, it swaps a page out.
		On success, it returns the index of the page in the page map.  On
		failure, it aborts.  BUG: pages are not made free when a process
		exits.
	*/
	static int		page_alloc(int pinned);

	//	page_addr returns the physical address of the i-th page
        static uint32_t		*page_addr(int i);

	//	page_replacement_policy returns the index in the page map of a page to be swapped out
	static int		page_replacement_policy(void);

	//	swap the i-th page in
	static void		page_swap_in(int pageno);

	//	swap the i-th page out
	static void		page_swap_out(int pageno);

	//	return the disk_sector of the given page
	static int		page_disk_sector(page_map_entry_t *page);

//	Static global variables
	//	the page map
	static page_map_entry_t		page_map[PAGEABLE_PAGES];

	//	lock to control the access to the page map
	static lock_t				page_map_lock;

	//	address of the kernel page directory (shared by all kernel threads)
	static uint32_t				*kernel_pdir;

	//	addresses of the kernel page tables
	static uint32_t				*kernel_pts[N_KERNEL_PTS];

//	Use virtual address to get index in page directory. 
inline uint32_t	get_directory_index(uint32_t vaddr) {
	return (vaddr & PAGE_DIRECTORY_MASK) >> PAGE_DIRECTORY_BITS;
}

/*	Use virtual address to get index in a page table. 
	The bits are masked, so we essentially get a modulo 1024 index. 
	The selection of which page table to index into is done with 
	get_directory_index(). 
*/ 
inline uint32_t	get_table_index(uint32_t vaddr) {
	return (vaddr & PAGE_TABLE_MASK) >> PAGE_TABLE_BITS;
}


//	Use the virtual address to invalidate a page in the TLB.
inline void	invalidate_page(uint32_t *vaddr) {
	/*	Invalidating a page *should* be possible to do, by using invplg. For some
		reason, this doesn't seem to work correctly, likely due to a bug somewhere
		in the code. The workaround is to flush the TLB completely, by reloading
		the page directory.
		
		Update 12. March 2003: Seems that it works now. A missing asterisk was
		the culprit.
	*/
	//select_page_directory();
	asm ( "invlpg %0" : : "m" (*((char*)vaddr)));
}

//	Set 12 least significant bytes in a page table entry to 'mode'
inline void page_set_mode(uint32_t* pdir, void *vaddr, uint32_t mode) {
	uint32_t	dir_index = get_directory_index((uint32_t) vaddr),
				index     = get_table_index((uint32_t) vaddr),
				dir_entry,
				*table,
				entry;

	dir_entry	= pdir[dir_index];
	ASSERT(dir_entry & PE_P);	//	dir entry present

	table			= (uint32_t*) (dir_entry & PE_BASE_ADDR_MASK);
	//	clear table[index] bits 11..0
	entry			= table[index] & PE_BASE_ADDR_MASK;

	//	set table[index] bits 11..0
	entry			|= mode & ~PE_BASE_ADDR_MASK;
	table[index]	= entry;

	//	Flush TLB
	invalidate_page(vaddr);
}


/*	Maps a page as present in the page table.
	
	'vaddr' is the virtual address which is mapped to the physical 
	address 'paddr'. 'mode' sets bit [12..0] in the page table entry.
	
	If user is nonzero, the page is mapped as accessible from a user
	application. 
*/
inline void table_map_page(uint32_t* table, uint32_t vaddr, uint32_t paddr, uint32_t mode) {
	int index		= get_table_index(vaddr); 
	table[index]	= (paddr & PE_BASE_ADDR_MASK) | (mode & ~PE_BASE_ADDR_MASK);

	//	Flush TLB
	invalidate_page((uint32_t*)vaddr);
}


/*	Insert a page table entry into the page directory. 
	
	'vaddr' is the virtual address which is mapped to the physical 
	address 'paddr'. 'mode' sets bit [12..0] in the page table entry.
*/
inline void	dir_ins_table(uint32_t* directory, uint32_t vaddr, void* table, uint32_t mode) {
	uint32_t	access	= mode & MODE_MASK,
				taddr	= (uint32_t)  table;
	int 		index	= get_directory_index(vaddr);

	directory[index]	= (taddr & PE_BASE_ADDR_MASK) | access;
}


/*	init_memory()
	
	called once by _start() in kernel.c
	
	This function actually only sets up a page directory and 
	page table for the kernel!
	
	This consists of setting up N_KERNEL_PTS (one in this case) which
	identity maps memory between 0x0 and MAX_PHYSICAL_MEMORY.
	
	The interrupts are off and paging is not enabled when this function
	is called.
*/
void	init_memory(void) {

	int			p,			//	page index in page map
				i, j;
	uint32_t	pbaddr;		//	page base address (vm)

	//	initialize the lock to access the page map
	lock_init(&page_map_lock);
	
	//	allocate the kernel page directory
	p				= page_alloc(TRUE);
	kernel_pdir		= page_addr(p);

	//	for each kernel page table
	pbaddr			= 0;
	for (i=0; i<N_KERNEL_PTS; i++) {
		//	allocate the page table
		p				= page_alloc(TRUE);
		kernel_pts[i]	= page_addr(p);
		
		//	Insert table into the page directory
		dir_ins_table(kernel_pdir, pbaddr, kernel_pts[i], PE_P | PE_RW);
		
		//	fill in the page table
		j = 0;
		while ((pbaddr < MAX_PHYSICAL_MEMORY) && (j < PAGE_N_ENTRIES)) {
			table_map_page(kernel_pts[i], pbaddr, pbaddr, PE_P | PE_RW);
			pbaddr += PAGE_SIZE;
			j++;
		}
	}
	
	//	Give the user permission to write on the screen
	page_set_mode(kernel_pdir, SCREEN_ADDR, PE_P | PE_RW | PE_US);
}


/*	Sets up a page directory and page table for a new process or thread. 
	
	Interrupts should be on when calling this function. If they are off
	and a page has to be swapped out, we will never catch the disk interrupt.
*/
void setup_page_table(pcb_t *p) {
	lock_acquire(&page_map_lock);
	
	if (p->is_thread) {
		/*	if p is a thread, it uses the kernel page directory and
			page tables, and it doesn't need anything else 
		*/
		p->page_directory = kernel_pdir;
	}
	else {
		/*	if p is a process, we have to allocate four pages: a page
			directory, a page table, a stack page table, and a stack
			page.
		*/
		uint32_t	*pde,			//	pointer to page directory entry
					*pte;			//	pointer to page table entry
		int			n_img_pages,	//	number of pages for process image
					i,
					pdir, ptbl, stkt, stkp; 
		
		//	allocate the four pages and pin them immediately
		pdir	= page_alloc(TRUE);	//	page directory
		ptbl	= page_alloc(TRUE);	//	page table
		stkt	= page_alloc(TRUE);	//	stack page table
		stkp	= page_alloc(TRUE);	//	stack page

		//	save process page directory address
		pde		= page_addr(pdir);
		p->page_directory = pde;

		//	map kernel page tables into process page directory
		for (i=0; i<N_KERNEL_PTS; i++) {
			dir_ins_table(pde, PTABLE_SPAN * i, kernel_pts[i], PE_P | PE_RW | PE_US);
		}
		
		//	map process page table into process page directory
		dir_ins_table(pde, PROCESS_START, page_addr(ptbl), PE_P | PE_RW | PE_US);

		//	map stack page table into process page directory
		dir_ins_table(pde, PROCESS_STACK, page_addr(stkt), PE_P | PE_RW | PE_US);

		//	force demand paging for code and data of process
		n_img_pages	= p->swap_size / SECTORS_PER_PAGE;
		if ((p->swap_size % SECTORS_PER_PAGE) != 0)
			n_img_pages++;
		
		pte		= page_addr(ptbl);
		for (i=0; i<n_img_pages; i++) {
			//	set all pages to not present (note that PE_P is not here)
			table_map_page(pte, PROCESS_START + i * PAGE_SIZE, PROCESS_START + i * PAGE_SIZE, PE_RW | PE_US);
		}
		
		pte = page_addr(stkt);
		//	map stack page into stack page table
		table_map_page(pte, PROCESS_STACK, (uint32_t) page_addr(stkp), PE_P | PE_RW | PE_US);
	}
	
	lock_release(&page_map_lock);
}

extern uint32_t exc_14_eip, exc_14_cs, exc_14_a, exc_14_b;
//	Page fault but page table present and page present
void page_protection_error(uint32_t pde, uint32_t pte) {
	uint32_t	cr2	= current_running->fault_addr;
	int			i	= 1; 

//	clear_screen(0, 10, 80, 25);
	PRINT_INFO(i++, "Page protection error ", print_str, "");
	PRINT_INFO(i++, "PID", print_int, current_running->pid); 
	PRINT_INFO(i++, "Preemptions", print_int, current_running->preempt_count); 
	PRINT_INFO(i++, "Yields", print_int, current_running->yield_count); 
	PRINT_INFO(i++, "Nested count", print_int, current_running->nested_count); 
	PRINT_INFO(i++, "Hardware mask", print_hex, current_running->int_controller_mask);
	PRINT_INFO(i++, "Fault address", print_hex, cr2); 
	PRINT_INFO(i++, "Error code", print_hex, current_running->error_code);
	PRINT_INFO(i++, "PDirectory entry", print_hex, pde);
	PRINT_INFO(i++, "PTable entry", print_hex, pte);
	PRINT_INFO(i++, "Faulting eip", print_hex, exc_14_eip);
	PRINT_INFO(i++, "Faulting cs", print_hex, exc_14_cs);
	PRINT_INFO(i++, "Faulting a", print_hex, exc_14_a);
	PRINT_INFO(i++, "Faulting b", print_hex, exc_14_b);
	PRINT_INFO(i++, "cs", print_hex, current_running->cs);
	HALT("halting due to protection error");
}

/*	called by exception_14 in interrupt.c (the faulting address is in current_running->fault_addr) 

	Interrupts are on when calling this function.
*/
void page_fault_handler(void) {
	int					pdi,		//	page directory index
						pti;		//	page table index
	uint32_t			pde,		//	page directory entry
						pte,		//	page table entry
						*pta;		//	page table address
	int					pidx;		//	page index in page map
	page_map_entry_t	*page;		//	ptr to page map entry of a page
	
	current_running->page_fault_count++;
	lock_acquire(&page_map_lock);
	
	pdi		= get_directory_index(current_running->fault_addr);
	pde		= current_running->page_directory[pdi];

	//	if page table not present
	if ((pde & PE_P) == 0) {
		//	the page fault is due to the abscence of a page table
		HALT("Page tables should all be pinned");
		page_protection_error(pde, 0);
	}
	else {
		//	The page table is present, so the page fault is due to the abscence of a target page.
		print_str(24, 10, "         ");
		print_hex(24, 10, current_running->fault_addr);
		
		//	get page table base address from the page directory entry
		pta				= (uint32_t *) (pde & PE_BASE_ADDR_MASK);

		//	get page table index from the faulting virtual address
		pti				= get_table_index(current_running->fault_addr);

		//	get the page table entry of the faulting virtual address
		pte				= pta[pti];

		print_str(24, 20, "         ");
		print_hex(24, 20, pte);

		//	make sure the target page is indeed not present
		if (pte & PE_P)
			page_protection_error(pde, pte);
		
		pidx			= page_alloc(FALSE);
		
		//	update the mapping for the new page
		page			= &page_map[pidx];
		page->owner		= current_running;
		page->vaddr		= current_running->fault_addr & PE_BASE_ADDR_MASK;
		page->entry		= &pta[pti];
		page->pinned	= FALSE;
		
		page_swap_in(pidx);
	}
	lock_release(&page_map_lock);
}

//	Other local functions


/*	Allocate a page. Returns page number / index in the 
	page_map directory.
	
	Marks page as pinned if pinned == TRUE. 
	
	Swaps out a page if no space is available. 
*/
static int page_alloc(int pinned) {
	static int		dole_ptr	= 0;
	int				i, page;
	uint32_t		*p;

	if (dole_ptr < PAGEABLE_PAGES) {
		//	The first PAGEABLE_PAGES are trivial, hand out one by one 
		page = dole_ptr;
		dole_ptr++;
	}
	else {
		//	no free pages left: swap a page out
		page = page_replacement_policy();
		page_swap_out(page);
	}
	ASSERT((page >= 0) && (page < PAGEABLE_PAGES));

	//	Clean out entry before returning index to it
	page_map[page].owner	= NULL;
	page_map[page].vaddr	= 0;
	page_map[page].entry	= NULL;
	page_map[page].pinned	= pinned; 
	
	//	Zero out page before returning 
	p						= page_addr(page);
	for (i=0; i<PAGE_N_ENTRIES; i++) {
		p[i] = 0;
	}
	return page;
}


//	Returns physical address of page number i
static uint32_t *page_addr(int i) {
	if (i < 0 || i >= PAGEABLE_PAGES) { 
		print_str(3, 0, "page: ");
		print_int(3, 6, i);
		print_str(4, 0, "valid: 0--");
		print_int(4, 10, PAGEABLE_PAGES - 1);
		HALT("page number out of range of pageable pages");
	}
	return (uint32_t *) (MEM_START + (PAGE_SIZE * i));
}


//	Decide which page to replace, return the page number 
static int page_replacement_policy(void) {
	static int		page = -1;
	bool_t			found;
	int				i;
	
	//	check if there is any page not pinned
	found	= FALSE;
	i = 0;
	while ((!found) && (i < PAGEABLE_PAGES)) {
		i++;
		found = (page_map[i].pinned == FALSE);
	}
	ASSERT2(found, "All pages pinned");
	
	//	Cycle through looking for an unpinned page.  Avoid last swapped page if possible. 
	while (1) {
		page++;
		if (page >= PAGEABLE_PAGES)
			page = 0;
		if (page_map[page].pinned == FALSE)
			return page;
	}
}


//	Swap page in from image
static void page_swap_in(int pageno) {
	page_map_entry_t	*page	= &page_map[pageno];
	uint32_t			addr	= (uint32_t) page_addr(pageno);
	int					sector	= page_disk_sector(page),
						i, nsectors;
	
	print_str(23, 50, "pid ");
	print_int(23, 54, current_running->pid);
	print_str(23, 57, "rding page ");
	print_int(23, 68, pageno);

	if ((sector + SECTORS_PER_PAGE) >
		(page->owner->swap_loc + page->owner->swap_size)) {
		/*	if the final sector is past the end of the image
			read only the sectors that belong to this image 
		*/
		nsectors = page->owner->swap_size % SECTORS_PER_PAGE;
	}
	else {
		//	else, read an entire page
		nsectors = SECTORS_PER_PAGE;
	}
	
	print_str(23, 72, "........");
	for (i = 0; i < nsectors; i++) {
		print_str(23, 10, "         ");
		print_int(23, 10, sector+i);
		print_str(23, 72 + i, "o");
		read(sector + i, ((unsigned char *)addr) + (i * SECTOR_SIZE));
	}
	for ( /* current i */ ; i<SECTORS_PER_PAGE; i++) {
		print_str(23, 72 + i, "*");
	}
	*page->entry = PE_P | PE_RW | PE_US | PE_A | addr;
	
	/*	No need to flush the TLB since the page table entry cannot be in
		the TLB (we only swap in pages after page faults).
	*/
}

/*	page_swap_out()
	
	Actually just writes the bloody page back to the 
	process image! There is no separate swap space on the
	disk.
	
	Only writes out data pages though. The text pages should
	not have been modified, so those should just be 
	discarded.
*/
static void page_swap_out(int pageno) {
	page_map_entry_t	*page = &page_map[pageno];
	
	print_str(24, 50, "pid ");
	print_int(24, 54, current_running->pid);
	print_str(24, 57, "wting page ");
	print_int(24, 68, pageno);
	
	ASSERT((page->vaddr & PAGE_DIRECTORY_MASK) >= PROCESS_START);
	
	print_str(24, 40, "         ");
	print_hex(24, 40, *page->entry);

	//	mark page as not present
	*page->entry &= ~PE_P;
	
	//	Flush TLB
	invalidate_page((uint32_t*)page->vaddr);

	print_str(24, 50, "         ");
	print_hex(24, 50, *page->entry);

	print_str(24, 71, "0");

	//	if page is dirty
	if ((*page->entry & PE_D) != 0) {
		int			i, sector, nsectors;
		uint32_t	addr;

		sector	= page_disk_sector(page);
		addr	= (uint32_t) page_addr(pageno);

		//	if the final sector is past the end of the image...
		if ((sector + SECTORS_PER_PAGE) >
			(page->owner->swap_loc + page->owner->swap_size)) {
			//	write only the sectors that belong to this image
			nsectors	= page->owner->swap_size % SECTORS_PER_PAGE;
		}
		else {
			//	else, write an entire page
			nsectors	= SECTORS_PER_PAGE;
		}

		//	Status bar. How many pages out of the sector? 
		print_str(24, 72, "........");
		for (i = 0; i < nsectors; i++) {
			print_str(24, 72 + i, "o");
			write(sector + i, ((unsigned char *)addr) + (i * SECTOR_SIZE));
		}
		for ( /* current i */ ; i < SECTORS_PER_PAGE; i++) {
			print_str(24, 72 + i, "*");
		}
	}
	print_str(24, 71, "x");
}

//	Get the sector number on disk of a process image 
static int page_disk_sector(page_map_entry_t *page) {
	return page->owner->swap_loc + ((page->vaddr - PROCESS_START) / PAGE_SIZE) * SECTORS_PER_PAGE;
}

