/* process3.c
 * 
 * Example process sending and receiving messages. 
 */

#include "common.h"
#include "syslib.h"
#include "util.h"

/* mailboxes */
#define QUEUE 2     /* shared buffer (p3 sends message m to reciver p4) */
#define FLOW_IN 3   /* mailbox p3 recives token from */
#define FLOW_OUT 4  /* mailbox p3 sends token to */

#define MAX_MSG_SIZE 256                     
#define ACTUAL_MSG_SIZE(n) ((n)+sizeof(int))

char space[ACTUAL_MSG_SIZE(MAX_MSG_SIZE)]; /* allocate memory for one message 
					    *  with a body of 256 bytes */
msg_t *m = (msg_t *)space;
msg_t token; /* a message with a body of one byte (only header) */

#define LINE 4

void _start(void) 
{
    int q, fi, fo;     /* mbox numbers */
    int size, c, i, j;
    int wait = TRUE;   /* should the process wait for messages or send msgs */
    int count, space;  /* number of messages, available space in QUEUE */

    /* q: shared buffer, process 3 sends message m to process 4 */
    if ((q = mbox_open(QUEUE)) < 0) 
	exit();
    /* process 3 recives token from fi (process 4 sends token to fi) */
    if ((fi = mbox_open(FLOW_IN)) < 0) 
	exit();
    /* process 3 send token to fo (process 4 recives token from fo) */
    if ((fo = mbox_open(FLOW_OUT)) < 0) 
	exit();
  
    for (i = 0; i < 1000000; i++) {
	/* Try to send and receive messages of sizes 128-255 bytes. 
	 */       

	/* print number of iterations */
	print_str(LINE,   0, "Process 3");
	print_int(LINE+1, 0, i);
	
	/* create message to send */
	size = 128 + i % 128;
	m->size = size;
	/* Fill in a message to send, use a "random" character */ 
	c = 'a' + i % 26;	
	for (j = 0; j < size; j++)
	    m->body[j] = c;

	/* Just try various things to see wheter the implementation
	 * hangs, or whether it works correctly. 
	 */ 
	if (i < 5000) {
	    if (wait) {
		mbox_recv(fi, &token); /* wait until process4 sends token 
					*  (when no more messages in q) */
		if (token.size != 0) {
		    print_str(LINE, 0, "Error:   ");
		    print_str(LINE+1, 0, "Invalid control msg");
		    exit();
		} 

		wait = FALSE;
	    } 
	    else {
		mbox_stat(q, &count, &space);
		/* if enough space in shared buffer q for message m */
		if (space < ACTUAL_MSG_SIZE(m->size)) {
		    token.size = 0;
		    /* send token to process4 */
		    mbox_send(fo, &token);
		    wait = TRUE;
		}
	    }
	} 
	else if (i == 5000) {
	    mbox_send(fo, &token);
	}
	/* send message m to shared buffer (block if not enough space) */
	mbox_send(q, m);
    }
    
    print_str(LINE,   0, "Process 3");
    print_str(LINE+1, 0, "Done   ");
    mbox_close(q);
    mbox_close(fi);
    mbox_close(fo);
    exit();
}











