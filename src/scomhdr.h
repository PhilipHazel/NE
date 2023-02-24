/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */

/* This header file is the interface between the "common" screen handling
functions and the system-dependent screen driver. */


/* Format of one entry in the screen buffer */

typedef struct {
  usint  ch;
  uschar rend;
} sc_buffstr;

/*  Functions */

extern void scommon_select(void);

extern void  (*sys_w_cls)(int, int, int, int);
extern void  (*sys_w_flush)(void);
extern void  (*sys_w_move)(int, int);
extern void  (*sys_w_rendition)(int);
extern void  (*sys_w_putc)(int);
extern void  (*sys_w_hscroll)(int, int, int, int, int);
extern void  (*sys_w_vscroll)(int, int, int);

/* End of scomhdr.h */
