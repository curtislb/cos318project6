/*	usb.h
*/
#ifndef USB_H
#define USB_H

//	Includes
#include "kernel.h"
#include "thread.h"
	
typedef struct {
  uint8_t heads;
  uint8_t sectors;
  uint16_t cylinders;
} __attribute__((packed)) usb_disk_params_t;

typedef struct {
  uint8_t dh;
  uint16_t cx;
  uint16_t dest_seg;
  uint16_t dest_off;
} __attribute__((packed)) sector_spec_t;

typedef struct {
  uint32_t eip;
  uint32_t cs;
  uint32_t eflags;
  uint32_t esp;
  uint32_t ss;
  uint32_t es;
  uint32_t ds;
  uint32_t fs;
  uint32_t gs;
} __attribute__((packed)) prot_to_v86_stack_t;
extern prot_to_v86_stack_t *prot_to_v86_stack;

//	Prototypes
/* Functions available to the rest of the kernel */

/* Must be called by kernel before usb can be used */
void usb_init(void);

/* Read one sector. 
 * 'block' is the sector number to read. 
 * 'address is a pointer to the buffer we should read the sector
 * into. 
 */ 
void read(int block, unsigned char *buf);
void write(int block, unsigned char *buf);

// lock for serializing access to usb.
extern lock_t	usb_lock;
extern uint32_t ss0;
extern uint32_t *V86_page_directory;
extern uint32_t v86_saved_stack;

// global decls from usbV86.h
extern usb_disk_params_t params;
extern sector_spec_t io_params;
void usb_read(void);
void usb_write(void);
void usb_params(void);
void v86_start(void);

#endif
