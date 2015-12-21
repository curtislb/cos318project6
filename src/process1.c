/* process1.c
 * 
 * Just "flies a plane" over the screen. 
 * 
 * Listens for messages in the mailbox with key 1, and fires
 * a bullet each time a message can be read. 
 * 
 */
#include "common.h"
#include "syslib.h"
#include "util.h"

#define COMMAND_MBOX 1 

#define ROWS 4      
#define COLUMNS 18  
static char picture[ROWS][COLUMNS+1] = {
    "     ___       _  ",
    " | __\\_\\_o____/_| ",
    " <[___\\_\\_-----<  ",
    " |  o'            "
};

static void draw_plane(int locX, int loc_y, int plane);
static void draw_bullet(int locX, int loc_y, int plane);

void _start(void) 
{
  int loc_x = 80, loc_y = 7, q, fired = FALSE, bullet_x = -1, bullet_y = -1;
  msg_t m;          /* "fire bullet" message */
  int count, space; /* number of messages, free space in buffer */
  int c;

  /* open command mailbox, to communicate with the shell */
  q = mbox_open(COMMAND_MBOX);
  while(1) {
      /* erase plane */
      draw_plane(loc_x, loc_y, FALSE);
      loc_x -= 1;
      if (loc_x < 10) {
	  loc_x = 80;
      }
      /* draw plane */
      draw_plane(loc_x, loc_y, TRUE);
      
      if ((q >= 0) && (!fired)) { /* mbox opened and bullet not fired */
	  /* recive number of messages and free space in command mbox */
	  mbox_stat(q, &count, &space);
	  if (count > 0) {
	      /* if messages in mbox, recive first command */
	      mbox_recv(q, &m);
	      if (m.size != 0) {
		  print_str(7, 30, "Error: Invalid msg size...exiting");
		  exit();
	      }
	
	      /* fire bullet */
	      fired = TRUE;
	      if (loc_x < 30)
		  bullet_x = 80;
	      else
		  bullet_x = loc_x-2;
	      bullet_y = loc_y+2;
	  }
      }
      
      if (fired) {  
	  /* erase bullet */
	  draw_bullet(bullet_x, bullet_y, FALSE);
	  
	  /* if bullet hit a character at screen[X-1, Y]*/
	  if ((bullet_x-1 >= 0) && 
	       ((c = peek_screen(bullet_x-1, bullet_y)) != 0) && 
	       (c != ' ')) { 
	      /* erase bullet and character at screen[X-1, Y] */
	      draw_bullet(bullet_x-1, bullet_y, FALSE); 
	      fired = FALSE; 
	  } 
	  /* if bullet hit a character at screen[X-2, Y]*/
	  else if ((bullet_x-2 >= 0) && 
		    ((c = peek_screen(bullet_x-2, bullet_y)) != 0) && 
		    (c != ' ')) { 
	      /* erase bullet and character at screen[X-2, Y] */
	      draw_bullet(bullet_x-2, bullet_y, FALSE);
	      fired = FALSE; 
	  }
	  /* bullet did not hit a character */
	  else {
	      bullet_x -= 2;
	      if (bullet_x < 0) 
		  fired = FALSE;
	      else
		  draw_bullet(bullet_x, bullet_y, TRUE);
	  }
      }
      ms_delay(250);
  } /* end forever loop */

  if (q >= 0) {      /* should not be reached */
      mbox_close(q);
  }
}

static void draw_plane(int loc_x, int loc_y, int plane) 
{
    int i, j;
    for (i = 0; i < COLUMNS; i++)
	for (j = 0; j < ROWS; j++)
	    if (loc_x+i >= 30) {
		if (plane == TRUE)
		    print_char(loc_y+j, loc_x+i, picture[j][i]);
		else
		    print_char(loc_y+j, loc_x+i, ' ');
	    }
}

static void draw_bullet(int loc_x, int loc_y, int bullet) 
{
    if (bullet) 
	print_str(loc_y, loc_x, "<=");
    else
	print_str(loc_y, loc_x, "  ");
}









