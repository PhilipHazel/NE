/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: January 2023 */

/* This file contains code for miscellaneous screen-handling subroutines */


#include "ehdr.h"
#include "shdr.h"


/*************************************************
*                Initialize                      *
*************************************************/

/* Initialize for screen editing, changing the margin according to the screen 
size if requested. */

void 
scrn_init(BOOL changemargin)
{
window_top = 1;
window_bottom = screen_max_row - 2;
window_width = screen_max_col;
window_depth = window_bottom - window_top;
main_drawgraticules = dg_both;
cursor_max = cursor_offset + window_width;
if (changemargin) main_rmargin = window_width;
window_vector = store_Xget((screen_max_row+1)*sizeof(linestr *));
for (usint i = 0; i <= screen_max_row; i++) window_vector[i] = NULL;
}



/*************************************************
*           Define relevant windows              *
*************************************************/

/* Set up the three "windows" that are the only ones now used by NE. */

void 
scrn_windows(void)
{
s_defwindow(message_window, screen_max_row, screen_max_row);
s_defwindow(first_window, window_bottom, window_top);
s_defwindow(first_window+1, window_bottom+1, window_bottom+1);
}



/*************************************************
*        Change screen (overall window) size     *
*************************************************/

/* New values will be in screen_max_row and screen_max_col */

/* LCOV_EXCL_START - never happens in testing */
void 
scrn_setsize(void)
{
linestr *topline = window_vector[0];
store_free(window_vector);
window_vector = NULL;
s_terminate();
s_init(screen_max_row, screen_max_col, TRUE);
scrn_init(FALSE);
scrn_windows();

if (main_vcursorscroll > (int)window_depth) 
  main_vcursorscroll = window_depth;
if (main_vmousescroll > (int)window_depth) 
  main_vmousescroll = window_depth;

while (cursor_col > cursor_max)                                     
  {                                                                      
  cursor_offset += main_hscrollamount;                                       
  cursor_max = cursor_offset + window_width;                           
  }                  

main_current->flags |= lf_shn;
scrn_hint(sh_topline, 0, topline);
scrn_display();
}
/* LCOV_EXCL_STOP */



/*************************************************
*             Suspend screen editing             *
*************************************************/

void 
scrn_suspend(void)
{
store_free(window_vector);
window_vector = NULL;
if (sys_resetterminal != NULL) sys_resetterminal();
main_screensuspended = TRUE;
main_screenOK = FALSE;
}



/*************************************************
*             Restore screen editing             *
*************************************************/

void 
scrn_restore(void)
{
if (sys_setupterminal != NULL) sys_setupterminal();
scrn_init(FALSE);
scrn_windows();
screen_forcecls = TRUE;
main_screensuspended = FALSE;
main_screenOK = TRUE;
}

/* End of escrnsub.c */
