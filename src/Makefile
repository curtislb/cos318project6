# Makefile for the OS projects.
# Best viewed with tabs set to 4 spaces.

CC = gcc
LD = ld

# This defines the location of the kernel, and the virtual location of
# the processes.
KERNEL_LOCATION    = 0x1000
PROCESS_LOCATION   = 0x1000000

# Compiler flags
#-fno-builtin:			Don't recognize builtin functions that do not begin with
#						'__builtin_' as prefix.
#
#-fno-defer-pop:		Always pop the arguments to each function call as soon 
#						as that function returns.
#
#-fomit-frame-pointer:	Don't keep the frame pointer in a register for 
#						functions that don't need one.
#
#-make-program-do-what-i-want-it-to-do:
#						Turn on all friendly compiler flags.
#
#-O2:					Turn on all optional optimizations except for loop unrolling
#						and function inlining.
#
#-c:					Compile or assemble the source files, but do not link.
#
#-Wall:					All of the `-W' options combined (all warnings on)
#
#-DPROCESS_START=$(PROCESS_LOCATION): 
#						Define macro PROCESS_START as $(PROCESS_LOCATION). The macro is
#						used by the C preprocessor.

CFLAGS = -fno-builtin-strlen -fno-builtin-bcopy -fno-builtin-bzero

#-fomit-frame-pointer
CCOPTS = -Wall -O1 -c -fno-builtin -fno-stack-protector -fno-defer-pop \
		 -DPROCESS_START=$(PROCESS_LOCATION) \
		 -DKERNEL_START=$(KERNEL_LOCATION) \
		 -m32

# Linker flags
#-nostartfiles:			Do not use the standard system startup files when linking.
#
#-nostdlib:				Don't use the standard system libraries and startup files when
#						linking. Only the files you specify will be passed to the linker.
#          
#-Ttext X:				Use X as the starting address for the text segment of the output 
#						file.

LDOPTS = -melf_i386 -nostartfiles -nostdlib -Ttext

# Add your user program here:
USER_PROGRAMS	=	

KERNEL			=	kernel.o

# Common objects used by both the kernel and user processes
COMMON			=	util.o
# Processes to create
PROCESSES		=	shell.o process1.o process2.o process3.o process4.o
FAKESHELL_OBJS = shellFake.o shellutilFake.o utilFake.o fsFake.o blockFake.o

# Objects needed by the kernel
# make sure the usbV86.o is last (and far away from interrupt.o). this
# is because the usbV86 contains code which will run with CPL=3 and the
# interrupt code should remain in pages which are supervisor access only
# (otherwise a gpf will result)
KERNELOBJ	=	thread.o mbox.o keyboard.o interrupt.o $(COMMON) \
			scheduler.o memory.o entry.o \
			sleep.o time.o fs.o block.o th1.o th2.o usb.o usbV86.o

# Objects needed to build a process
PROCOBJ			=	$(COMMON) syslib.o

# Makefile targets
all: lnxsh

bootable: bootblock createimage kernel floppy.img $(PROCESSES:.o=)

kernel: $(KERNEL) $(KERNELOBJ)
	$(LD) $(LDOPTS) $(KERNEL_LOCATION) -o kernel $^

# Build entry-pp.s by first pre-processing entry.S, then assembling entry-pp.s,
# producing entry.o as output.
entry.o: entry.S
	$(CC) $(CCOPTS) -E $< > entry-pp.s
	$(CC) $(CCOPTS) -c entry-pp.s -o entry.o

usbV86.o: usbV86.S
	$(CC) $(CCOPTS) -E $< > usbV86-pp.s
	$(CC) $(CCOPTS) -c usbV86-pp.s -o usbV86.o

# Build the processes
process1: process1.o $(PROCOBJ)
	$(LD) $(LDOPTS) $(PROCESS_LOCATION) -o process1 $^

process2: process2.o $(PROCOBJ)
	$(LD) $(LDOPTS) $(PROCESS_LOCATION) -o process2 $^

process3: process3.o $(PROCOBJ)
	$(LD) $(LDOPTS) $(PROCESS_LOCATION) -o process3 $^

process4: process4.o $(PROCOBJ)
	$(LD) $(LDOPTS) $(PROCESS_LOCATION) -o process4 $^

# For each user process:
# processX.o $(PROCOBJ)
#	$(LD) $(LDOPTS) $(PROCESS_LOCATION) -o processX $^
#

shell: shell.o shellutil.o $(PROCOBJ)
	$(LD) $(LDOPTS) $(PROCESS_LOCATION) -o shell $^

createimage: createimage.c
	$(CC) -o createimage $<	

bootblock.o: bootblock.s
	$(CC) $(CCOPTS) $<

bootblock: bootblock.o
	$(LD) $(LDOPTS) 0x0 -o bootblock $<

cantboot.o: cantboot.s
	$(CC) $(CCOPTS) $<

cantboot: cantboot.o
	$(LD) $(LDOPTS) 0x0 -o cantboot $<

# Create an image to put on the USB disk
image: createimage bootblock kernel $(PROCESSES:.o=)
	./createimage --vm --kernel ./bootblock ./kernel $(PROCESSES:.o=)

# Put the image on the USB disk (these two stages are independent, as both
# vmware and bochs can run using only the image file stored on the harddisk)
boot: image
	cat ./image > /dev/sda

floppy.img : image
	dd if=/dev/zero of=floppy.img bs=512 count=2880
	dd if=image of=floppy.img conv=notrunc

lnxsh: $(FAKESHELL_OBJS)
	$(CC) -o lnxsh $(FAKESHELL_OBJS)

shellFake.o : shell.c
	$(CC) -Wall $(CFLAGS) -g -c -DFAKE -o shellFake.o shell.c

shellutilFake.o : shellutilFake.c
	$(CC) -Wall $(CFLAGS) -g -c -DFAKE -o shellutilFake.o shellutilFake.c

blockFake.o : blockFake.c
	$(CC) -Wall $(CFLAGS) -g -c -DFAKE -o blockFake.o blockFake.c

utilFake.o : util.c
	$(CC) -Wall $(CFLAGS) -g -c -DFAKE -o utilFake.o util.c

fsFake.o : fs.c
	$(CC) -Wall $(CFLAGS) -g -c -DFAKE -o fsFake.o fs.c

# Figure out dependencies, and store them in the hidden file .depend
depend: .depend
.depend:
	$(CC) $(CCOPTS) -MM *.c > $@

# Clean up!
clean:
	rm -f *.o
	rm -f $(PROCESSES:.o=) kernel image createimage bootblock lnxsh
	rm -f .depend
	rm -f entry-pp.s
	rm -f usbV86-pp.s
	rm -f floppy.img

# No, really, clean up!
distclean: clean
	rm -f *~
	rm -f \#*
	rm -f *.bak
	rm -f bochsout.txt

# How to compile a C file
%.o:%.c
	$(CC) $(CCOPTS) $<

# How to assemble
%.o:%.s
	$(CC) $(CCOPTS) $<

# How to produce assembler input from a C file
%.s:%.c
	$(CC) $(CCOPTS) -S $<

# Include dependencies (the leading dash prevents warnings if the file doesn't exist)
-include .depend
