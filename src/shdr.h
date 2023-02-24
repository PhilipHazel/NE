/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991- 2016 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: January 2016 */

/* This header file is the interface to the screen handling
functions. */


enum { s_r_normal, s_r_inverse };


extern void (*s_cls)(void);
extern void (*s_defwindow)(int, int, int);
extern void (*s_eraseright)(void);
extern void (*s_flush)(void);
extern void (*s_hscroll)(int, int, int, int, int);
extern void (*s_init)(int, int, BOOL);
extern int  (*s_maxx)(void);
extern int  (*s_maxy)(void);
extern void (*s_move)(int, int);
extern void (*s_printf)(const char *, ...);
extern void (*s_putc)(int);
extern void (*s_rendition)(int);
extern void (*s_selwindow)(int, int, int);
extern void (*s_terminate)(void);
extern void (*s_vscroll)(int, int, int);
extern int  (*s_window)(void);
extern int  (*s_x)(void);
extern int  (*s_y)(void);

/* End of shdr.h */
