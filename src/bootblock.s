# 1 "bootblock.S"
# 1 "<built-in>"
# 1 "<command line>"
# 1 "bootblock.S"
# bootblock.s: toy boot loader for COS 318
# Author: David Eisenstat <deisenst@CS.Princeton.EDU>

# Note: on the current fishbowl machines (Dell Dimension 9200s),
# INT 0x13 _crashes_ unless the stack is set up the same way
# as it was when we booted.

# Revision history:
# 2007-09-15 Support for protected mode and memory >= 1MB
# 2007-09-12 Initial version
##

 # The BIOS puts the boot loader here
 .equ BOOT_SEGMENT, 0x07c0
 # The boot loader copies itself here to make room for the OS
 .equ NEW_BOOT_SEGMENT, 0x00e0
 # Number of bytes in a sector
 .equ SECTOR_SIZE, 0x0200
 # The boot loader puts the OS here
 .equ OS_SEGMENT, 0x0100
 # offset in the GDT of the OS code segment
 .equ OS_CODE_DESCRIPTOR, 0x08
 # offset in the GDT of the OS data segment
 .equ OS_DATA_DESCRIPTOR, 0x10
 # Initial stack setup
 .equ STACK_SEGMENT, 0x9000
 .equ STACK_POINTER, 0xfffe

 # The code begins here
 .text
 # Generate 16-bit instructions for real mode execution
 .code16
 # The label ``_start'' is where the code begins executing
 # Make it visible to the linker
 .globl _start
_start:
 # Jump over the memory that holds global variables
 jmp after_global_variables
# Reserve space for createimage to write the size of the OS, in sectors
os_size:
 .long 0x00000000
# Reserve space for the drive parameters
drive_number:
 .byte 0x00
max_sector:
 .byte 0x00
max_head:
 .byte 0x00
after_global_variables:
 # Set up the stack
 # The values below are what the 9200 BIOS uses
 movw $0x0030, %ax
 movw %ax, %ss
 movw $0x0100, %sp
 ## Move the boot loader to the new location
 # The source is %ds:%si
 movw $BOOT_SEGMENT, %ax
 movw %ax, %ds
 movw $0x0000, %si
 # The destination is %es:%di
 movw $NEW_BOOT_SEGMENT, %ax
 movw %ax, %es
 movw $0x0000, %di
 # The number of bytes to be copied is %cx
 # The boot loader occupies one sector, or 512 bytes
 movw $SECTOR_SIZE, %cx
 # Clear the direction bit so that movsb _increments_ %si and %di
 cld
 # Do it
 rep movsb
 # Jump to the next instruction at the new location
 ljmp $NEW_BOOT_SEGMENT, $0x0000 + after_move
after_move:
 # Set up the data segment
 movw $NEW_BOOT_SEGMENT, %ax
 movw %ax, %ds
 ## Load the OS using INT 0x13
 # Save the drive number
 movb %dl, drive_number
 # Get the drive parameters
 movb $0x08, %ah
 int $0x13
 # The maximum sector number is returned in bits 5:0 of %cl
 andb $0x3f, %cl
 movb %cl, max_sector
 # The maximum head number is returned in %dh
 movb %dh, max_head
 ## Prepare to loop
 # %bp is the loop counter
 movw os_size, %bp
 # Load the CHS address of the first sector of the OS
 # Cylinder 0, sector 2
 movw $0x0002, %cx
 # Head 0
 movb $0x00, %dh
 # Destination is %es:%bx
 movw $0x0000, %ax
 movw %ax, %es
 movw $(OS_SEGMENT<<4), %bx
load_loop:
 # Termination test
 cmpw $0x0000, %bp
 jng after_load
 decw %bp
 # Restore the drive number
 movb drive_number, %dl
 # Read a sector
 movw $0x0201, %ax
 # Magic break
 # This otherwise useless instruction makes bochs pause execution
# xchgw %bx, %bx
 int $0x13
 # Compute the CHS coordinates of the next sector
 cmpb max_sector, %cl
 jnl next_head
 incb %cl
 jmp after_next
next_head:
 movb $0x01, %cl
 cmpb max_head, %dh
 jnl next_cylinder
 incb %dh
 jmp after_next
next_cylinder:
 movb $0x00, %dh
 incb %ch
after_next:
 # Increment the destination
 addw $SECTOR_SIZE, %bx
 # Check the carry bit
 jnc load_loop
 # Advance the segment if it's on
 movw %es, %ax
 addw $0x1000, %ax
 movw %ax, %es
 jmp load_loop
after_load:
 # Clear the screen
 movw $0x0003, %ax
 int $0x10
 # Turn on address line 20
 # See http:
 movb $0x02, %al
 outb %al, $0x92
 ## Prepare for protected mode
 # Disable interrupts
 cli
 # Load the global descriptor table
 lgdt gdt
 # Enable protection via the machine status word
 movl %cr0, %eax
 orl $0x00000001, %eax
 movl %eax, %cr0
 ljmp $OS_CODE_DESCRIPTOR, $after_protect + (NEW_BOOT_SEGMENT<<4)

 # In protected mode now
 .code32
after_protect:
 # Set up the data segments for the OS
 movw $OS_DATA_DESCRIPTOR, %ax
 movw %ax, %ds
 movw %ax, %es
 movw %ax, %fs
 movw %ax, %gs
 # Set up the stack
 movw %ax, %ss
 movl $((STACK_SEGMENT<<4) + STACK_POINTER), %esp
 # Transfer control to the OS
 ljmp $OS_CODE_DESCRIPTOR, $(OS_SEGMENT<<4)

 # See http:
gdt:
 # The index of the last byte in the gdt
 .word 0x0017
 # The pointer to the start of the gdt
 .long gdt_contents + (NEW_BOOT_SEGMENT<<4)

 # The GDT must be aligned
 .balign 16
gdt_contents:
 # The first entry is zero
 .long 0x00000000
 .long 0x00000000

 # OS code segment
 # limit bits 7:0, 15:8
 .byte 0xff
 .byte 0xff
 # base bits 7:0, 15:8, 23:16
 .byte 0x00
 .byte 0x00
 .byte 0x00
 # present (0x80)
 # privilege level 0 (0x00/0x60)
 # code or data (0x10)
 # code (0x08)
 # non-conforming (0x00/0x04)
 # readable (0x02)
 .byte 0x9a
 # granularity = 4 KiB pages (0x80)
 # default size = 32 bit (0x40)
 # limit bits 19:16
 .byte 0xcf
 # base bits 31:24
 .byte 0x00

 # OS data segment
 # limit bits 7:0, 15:8
 .byte 0xff
 .byte 0xff
 # base bits 7:0, 15:8, 23:16
 .byte 0x00
 .byte 0x00
 .byte 0x00
 # present (0x80)
 # privilege level 0 (0x00/0x60)
 # code or data (0x10)
 # data (0x00/0x08)
 # non-conforming (0x00/0x04)
 # writable (0x02)
 .byte 0x92
 # granularity = 4 KiB pages (0x80)
 # default size = 32 bit (0x40)
 # limit bits 19:16
 .byte 0xcf
 # base bits 31:24
 .byte 0x00
