/*	memory.h
	Best viewed with tabs set to 4 spaces.
*/
#ifndef MEMORY_H
	#define MEMORY_H

//	Includes
	#include	"kernel.h"
	
//	Constants
enum {
	//	physical page facts
	PAGE_SIZE					= 4096,
	PAGE_N_ENTRIES				= (PAGE_SIZE / sizeof(uint32_t)),
	SECTORS_PER_PAGE			= (PAGE_SIZE / SECTOR_SIZE),

	PTABLE_SPAN					= (PAGE_SIZE * PAGE_N_ENTRIES),
	
	//	page directory/table entry bits (PMSA p.235 and p.240)
	PE_P						= 1 << 0,		//	present
	PE_RW						= 1 << 1,		//	read/write
	PE_US						= 1 << 2,		//	user/supervisor
	PE_PWT						= 1 << 3,		//	page write-through
	PE_PCD						= 1 << 4,		//	page cache disable
	PE_A						= 1 << 5,		//	accessed
	PE_D						= 1 << 6,		//	dirty
	PE_BASE_ADDR_BITS			= 12,			//	position of base address
	PE_BASE_ADDR_MASK			= 0xfffff000,	//	extracts the base address

	//	Constants to simulate a very small physical memory.
	MEM_START					= 0x100000,		//	== 1MB
	PAGEABLE_PAGES				= 128,
//	PAGEABLE_PAGES				= 29,	
	MAX_PHYSICAL_MEMORY			= (MEM_START + PAGEABLE_PAGES * PAGE_SIZE),

	//	number of kernel page tables
	N_KERNEL_PTS				= 1,
	
	PAGE_DIRECTORY_BITS			= 22,			//	position of page dir index
	PAGE_TABLE_BITS				= 12,			//	position of page table index
	PAGE_DIRECTORY_MASK			= 0xffc00000,	//	page directory mask
	PAGE_TABLE_MASK				= 0x003ff000,	//	page table mask
	PAGE_MASK					= 0x00000fff,	//	page offset mask
	MODE_MASK               	= 0x000003ff,	/*	used to extract the 10 lsb of
													a page directory entry */
	
	PAGE_TABLE_SIZE         	= (1024*4096 -1),	//	size of a page table in bytes
};

#ifndef MAKE_PRE_FILE
//	structure of an entry in the page map
typedef struct {
    pcb_t		*owner;			//	process that owns this page
    uint32_t	vaddr;			//	page-aligned virtual address of this page
    uint32_t	*entry;			//	entry that points to this page
    bool_t		pinned;			//	is this page pinned?
} page_map_entry_t;
#endif

//	Prototypes
	//	Initialize the memory system, called from kernel.c: _start()
	void	init_memory(void);

	/*	Set up a page directory and page table for the process. Fill in
		any necessary information in the pcb. 
	*/
	void	setup_page_table(pcb_t *p);

	/*	Page fault handler, called from interrupt.c: exception_14(). 
		Should handle demand paging 
	*/
	void	page_fault_handler(void);
	
	//	Use virtual address to get index in page directory.
	inline uint32_t	get_directory_index(uint32_t vaddr);

	//	Use virtual address to get index in a page table. 
	inline uint32_t	get_table_index(uint32_t vaddr);
	
	//	Invalidate a page
	inline void	invalidate_page(uint32_t *vaddr);
	
#ifndef MAKE_PRE_FILE
	//	Set 12 least significant bytes in a page table entry to 'mode'
	inline void	page_set_mode(uint32_t* pdir, void *vaddr, uint32_t mode);
	
	//	Maps a page as present in the page table. 
	inline void	table_map_page(uint32_t* table, uint32_t vaddr, uint32_t paddr, uint32_t mode);
	
	//	Insert a page table entry into the page directory.
	inline void	dir_ins_table(uint32_t* directory, uint32_t vaddr, void* table, uint32_t mode);
#endif
#endif
