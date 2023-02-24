/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2022 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: December 2022 */

/* This header file contains definitions of NE logical keystrokes */

/* Specified functions. 1-31 are ctrl keys; other user-settable functions have
values less than 200. This includes the function keystrokes which start at
s_f_umax. Values greater than 200 are special keys or internally generated
actions. Not all terminal drivers support all these keys. Nor do they all pass
all these functions back to the main part of NE. */

/* These are "actual control keystrokes". Their values are in increments of 4
to allow for shift, ctrl, and shift+ctrl. */

#define s_f_shiftbit   1
#define s_f_ctrlbit    2

#define s_f_ubase      32    /* named keys base */
#define s_f_cup        32    /* cursor up */
#define s_f_cdn        36    /* cursor down */
#define s_f_clf        40    /* cursor left */
#define s_f_crt        44    /* cursor right */
#define s_f_del        48    /* delete */
#define s_f_bsp        52    /* dedicated backspace key */
#define s_f_ret        56    /* dedicated return key */
#define s_f_tab        60    /* dedicated tab key */
#define s_f_ins        64    /* insert key */
#define s_f_hom        68    /* home key */
#define s_f_pup        72    /* page up */
#define s_f_pdn        76    /* page down */
#define s_f_end        80    /* end */
#define s_f_umax       83    /* maximum user-settable key */

/* Hard-wired keys and "events" */

#define s_f_fbase      200   /* fixed keys base */
#define s_f_ignore     200   /* interaction to be ignored */
#define s_f_return     201   /* return during command reading */
#define s_f_top        202   /* cursor hit top of window */
#define s_f_bottom     203   /* cursor hit bottom of window */
#define s_f_left       204   /* cursor hit left edge */
#define s_f_right      205   /* cursor hit right edge */
#define s_f_leftdel    206   /* delete previous at left edge */
#define s_f_lastchar   207   /* data char in last window position */
#define s_f_forcedend  208   /* return of control requested */
#define s_f_reshow     209   /* reshow screen */
#define s_f_scrleft    210   /* scroll left */
#define s_f_scrright   211   /* scroll right */
#define s_f_scrup      212   /* scroll up */
#define s_f_scrdown    213   /* scroll down */
#define s_f_scrtop     214   /* scroll top */
#define s_f_scrbot     215   /* scroll bottom */
#define s_f_delrt      216   /* delete all right */
#define s_f_dellf      217   /* delete all left */
#define s_f_startline  218   /* cursor to true line start */
#define s_f_endline    219   /* cursor to true line end */
#define s_f_wordlf     220   /* cursor left by one word */
#define s_f_wordrt     221   /* cursor right by one word */
#define s_f_nextline   222   /* cursor to start next line */
#define s_f_topleft    223   /* cursor to top left of screen */
#define s_f_botright   224   /* cursor to bottom right */
#define s_f_readcom    225   /* read commands */
#define s_f_paste      226   /* paste */
#define s_f_tb         227   /* mark text */
#define s_f_rb         228   /* mark rectangle */
#define s_f_cut        229   /* cut */
#define s_f_copy       230   /* copy */
#define s_f_dmarked    231   /* delete marked */
#define s_f_dc         232   /* delete char */
#define s_f_dp         233   /* delete previous */
#define s_f_delline    234   /* delete line */
#define s_f_xy         235   /* mouse X,Y */
#define s_f_mscr_down  236   /* mouse scroll down */
#define s_f_mscr_up    237   /* mouse scroll up */
#define s_f_fmax       237   /* max fixed key */


/* Keystroke actions - values start after function keystrings - these are
logical actions into which actual function keystrokes are translated, either by
fixed or user-changeable tables. These values must all be positive.

When adding to this list, remember to update the vector in c.ekey that says
which ones are permitted in readonly buffers. */

#define mks max_keystring

#define ka_push       0      /* essentially does nothing = 'unset' */
#define ka_firstka    (mks+1)
#define ka_al         (mks+1)  /* align line with cursor */
#define ka_alp        (mks+2)  /* align line with previous line */
#define ka_cl         (mks+3)  /* close up line (to the right) */
#define ka_clb        (mks+4)  /* close up line to the left */
#define ka_co         (mks+5)  /* copy to cut buffer */

#define ka_csd        (mks+6)  /* cursor down */
#define ka_csl        (mks+7)  /* cursor left */
#define ka_csls       (mks+8)  /* cursor to (true) line start */
#define ka_csle       (mks+9)  /* cursor to (true) line end */
#define ka_csnl       (mks+10) /* cursor to next line start */
#define ka_cstl       (mks+11) /* cursor to text (on screen) left */
#define ka_cstr       (mks+12) /* cursor to text (on screen) right */
#define ka_csr        (mks+13) /* cursor right */
#define ka_cssbr      (mks+14) /* cursor to screen bottom right */
#define ka_cssl       (mks+15) /* cursor to screen left */
#define ka_csstl      (mks+16) /* cursor to screen top left */
#define ka_cstab      (mks+17) /* cursor to next tab position */
#define ka_csptab     (mks+18) /* cursor to previous tab position */
#define ka_csu        (mks+19) /* cursor up */
#define ka_cswl       (mks+20) /* cursor word left */
#define ka_cswr       (mks+21) /* cursor word right */
#define ka_cu         (mks+22) /* cut to cut buffer */

#define ka_dal        (mks+23) /* delete all to left */
#define ka_dar        (mks+24) /* delete all to right */
#define ka_dc         (mks+25) /* delete character */
#define ka_de         (mks+26) /* delete text or rectangle */
#define ka_dl         (mks+27) /* delete line */
#define ka_dp         (mks+28) /* delete previous */
#define ka_dtwl       (mks+29) /* delete to word left */
#define ka_dtwr       (mks+30) /* delete to word right */
#define ka_gm         (mks+31) /* set global mark */
#define ka_join       (mks+32) /* concatenate with previous line */
#define ka_lb         (mks+33) /* line block begin */
#define ka_pa         (mks+34) /* paste */
#define ka_rb         (mks+35) /* rectangle begin */
#define ka_reshow     (mks+36) /* reshow request */
#define ka_rc         (mks+37) /* read commands */
#define ka_rs         (mks+38) /* rectangle insert space */
#define ka_scbot      (mks+39) /* scroll bottom */
#define ka_scdown     (mks+40) /* scroll down */
#define ka_scleft     (mks+41) /* scroll left */
#define ka_scright    (mks+42) /* scroll right */
#define ka_sctop      (mks+43) /* scroll top */
#define ka_scup       (mks+44) /* scroll up */
#define ka_split      (mks+45) /* split line */
#define ka_tb         (mks+46) /* text block begin */

/* These are artifically manufactured values that can never themselves
be set as keystroke actions. */

#define ka_dpleft     (mks+47) /* delete previous at lefthand edge */
#define ka_forced     (mks+48) /* interaction forced end */
#define ka_last       (mks+49) /* last data char in window typed */
#define ka_ret        (mks+50) /* unclaimed return */
#define ka_wbot       (mks+51) /* cursor hit bot of window */
#define ka_wleft      (mks+52) /* cursor hit left of window */
#define ka_wright     (mks+53) /* cursor hit right of window */
#define ka_wtop       (mks+54) /* cursor hit top of window */
#define ka_xy         (mks+55) /* cursor set by mouse click */
#define ka_mscr_down  (mks+56) /* mouse scroll down */
#define ka_mscr_up    (mks+57) /* mouse scroll up */
#define ka_lastka     (mks+57)

/* When adding to this list, remember to update the vector in ekey.c that says
which ones are permitted in readonly buffers. */

/* End of keyhdr.h */
