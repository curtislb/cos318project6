
.code16

	.equ    USB_DISK_DRIVE_NUM,0x80

.globl params
.globl usb_params

# IRQ_START+6 = 38
	.equ  IRQ_FLOPPY, 38
	.equ  SCREEN_SEG, 0xb800
	.equ  SCREEN_OFF, 0x0

.globl v86_start

v86_start:
usb_params:
	jmp extract_params
params:
	.byte 0x00
	.byte 0x00
	.word 0x00

	.equ  HEADS, 0
	.equ  SECTORS, 1
	.equ  CYLINDERS, 2

extract_params:
# 	movw    $SCREEN_SEG, %ax
# 	movw	%ax, %es
# 	movw	$0x074b, %es:(0x0)
# forever:
# 	jmp	extract_params

	#mov	$params, %ebx
	# lea	usb_params, %eax
	# and	$0xffffff00, %eax
	mov	%ds, %ax
	shl	$4, %eax
	lea	params, %ebx
	sub	%eax, %ebx

	movb    $USB_DISK_DRIVE_NUM, %dl
        movb    $8, %ah
        int     $0x13
	
	incb	%dh
	movb	%dh, HEADS(%bx)
	movb	%cl, %al
	andb	$0x3F, %al
	movb	%al, SECTORS(%bx)
	xorw	%ax, %ax
	movb    %cl, %al
	andb	$0xC0, %al
	shlw	$0x2, %ax
	movb	%ch, %al
	incw	%ax
	movw	%ax, CYLINDERS(%bx)
	hlt


.globl usb_read
.globl io_params
.globl usb_write

usb_write:
	jmp	write_sector  # this arrangement ensures io_params is within CS segment
usb_read:
	jmp	read_sector
io_params:
	.byte 0x00
	.word 0x00
	.word 0x00
	.word 0x00

	.equ DH, 0
	.equ CX, 1
	.equ ES, 3
	.equ BX, 5

read_sector:

	mov	%ds, %ax
	shl	$4, %eax
	lea	io_params, %ebx
	sub	%eax, %ebx

	movb	DH(%bx), %dh
	movw	CX(%bx), %cx
	movw	ES(%bx), %ax
	movw	%ax, %es
	movw	BX(%bx), %ax
	movw	%ax, %bx

	movb    $USB_DISK_DRIVE_NUM, %dl
	movb	$0x2, %ah
	movb	$0x1, %al
	int	$0x13
	
	cmpb	$0,%ah			# check if read was successful
	jne	read_error
	hlt
	
read_error:
	pushw	$read_error_msg	
	call	print_string
	jmp	and_never_stop

read_error_msg:
	.asciz	"Error Reading Sector"

write_sector:

	mov	%ds, %ax
	shl	$4, %eax
	lea	io_params, %ebx
	sub	%eax, %ebx

	movb	DH(%bx), %dh
	movw	CX(%bx), %cx
	movw	ES(%bx), %ax
	movw	%ax, %es
	movw	BX(%bx), %ax
	movw	%ax, %bx

	movb    $USB_DISK_DRIVE_NUM, %dl
	movb	$0x3, %ah
	movb	$0x1, %al
	int	$0x13
	
	cmpb	$0,%ah			# check if read was successful
	jne	write_error
	hlt

write_error:
	pushw	$write_error_msg	
	call	print_string
	jmp	and_never_stop

write_error_msg:
	.asciz	"Error Writing Sector"

print_string:
	push	%ebp
	movw	%sp,%bp
	pushw	%ax
	pushw	%bx
	pushw	%si

	movw	4(%bp),%si
print_string_while:
	lodsb	# load character to write into al, and increment si
	cmpb	$0,%al
	jz	print_string_end_while
	movb	$0x0e,%ah	# function number
	movb	$0x00,%bh	# active page number
	movb	$0x02,%bl	# foreground color
	int	$0x10
	jmp	print_string_while
print_string_end_while:

	popw	%si
	popw	%bx
	popw	%ax
	movw	%bp,%sp
	pop	%ebp
	retw	$2
and_never_stop:
	jmp	and_never_stop
.globl v86_end
v86_end:
	nop
