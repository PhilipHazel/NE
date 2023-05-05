/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: March 2023 */


/* This file contains all the global variables. */


#include "ehdr.h"
#include "keyhdr.h"


/*************** Debug things *************/

FILE *crash_logfile = NULL;
FILE *debug_file = NULL;



/***************  Scalars  ****************/

FILE *cmdin_fid = NULL;
FILE *msgs_fid;
FILE *kbd_fid;
FILE *withkey_fid = NULL;
usint withkey_sleep = 0;

BOOL    allow_wide = FALSE;

uschar *arg_from_name = NULL;         /* Names on command line */
uschar *arg_to_name = NULL;
uschar *arg_ver_name = NULL;
uschar *arg_with_name = NULL;

bufferstr *currentbuffer = NULL;

int      cmd_bracount;
int      cmd_breakloopcount;
uschar  *cmd_buffer;
BOOL     cmd_casematch = FALSE;
int      cmd_clineno;
linestr *cmd_cbufferline;
uschar  *cmd_cmdline;
BOOL     cmd_eoftrap;
BOOL     cmd_faildecode;
int      cmd_ist;
BOOL     cmd_onecommand;
uschar  *cmd_ptr;
BOOL     cmd_refresh;
uschar  *cmd_stack[cmd_stacktop+1];
int      cmd_stackptr;

uschar   cmd_word[max_wordlen + 1];

BOOL     crash_handler_chatty = TRUE;

usint    cursor_row;
usint    cursor_col;
usint    cursor_max;
usint    cursor_rh_adjust = 0;
usint    cursor_offset;

linestr *cut_buffer;
linestr *cut_last;
int      cut_type;
BOOL     cut_pasted = TRUE;

int      default_rmargin = 79;

int      error_count = 0;
BOOL     error_werr = FALSE;

filewritstr *files_written = NULL;

uschar   key_codes[256];
usint    key_controlmap;
usint    key_functionmap;
usint    key_specialmap[4];

sestr   *last_se;
sestr   *last_abese;
qsstr   *last_abent;
sestr   *last_gse;
qsstr   *last_gnt;

linestr *main_bottom;
linestr *main_current;
linestr *main_lastundelete;
linestr *main_top;
linestr *main_undelete;

bufferstr *main_bufferchain = NULL;
backstr   *main_backlist;
procstr   *main_proclist = NULL;

BOOL    main_appendswitch = FALSE;
BOOL    main_attn = TRUE;
BOOL    main_AutoAlign = FALSE;
usint   main_backnext = 0;
usint   main_backtop = 0;
usint   main_backregionsize = 12;
BOOL    main_backupfiles = FALSE;
BOOL    main_binary = FALSE;
int     main_cicount = 0;
BOOL    main_detrail_output = FALSE;
BOOL    main_done = FALSE;
int     main_drawgraticules;
BOOL    main_eightbit = FALSE;
uschar *main_einit = NULL;
BOOL    main_eoftrap = FALSE;
BOOL    main_escape_pressed = FALSE;
uschar *main_filealias;
BOOL    main_filechanged;
uschar *main_filename;
uschar *main_fromlist[MAX_FROM];
usint   main_hscrollamount = 10;
int     main_ilinevalue = 3;
int     main_imax;
int     main_imin;
BOOL    main_initialized = FALSE;
BOOL    main_interactive = TRUE;
BOOL    main_leave_message = FALSE;
usint   main_linecount = 0;
BOOL    main_logging = FALSE;
int     main_nextbufferno;
BOOL    main_nlexit = TRUE;
BOOL    main_noinit = FALSE;
BOOL    main_nowait;
BOOL    main_oldcomment = FALSE;
BOOL    main_oneattn = FALSE;
uschar *main_opt;
BOOL    main_overstrike = FALSE;
BOOL    main_pendnl = FALSE;
int     main_rc = 0;                  /* The final return code */
BOOL    main_readonly = FALSE;
BOOL    main_repaint;
usint   main_rmargin = 79;            /* Default for line-by-line */
BOOL    main_screenmode = TRUE;
BOOL    main_screenOK = FALSE;
BOOL    main_screensuspended = FALSE;
BOOL    main_selectedbuffer;
BOOL    main_shownlogo = FALSE;       /* FALSE if need to show logo on error */
size_t  main_storetotal = 0;          /* Total store used */
BOOL    main_tabflag = FALSE;
BOOL    main_tabin = FALSE;
BOOL    main_tabout = FALSE;
uschar *main_tabs = US"tabs";
int     main_undeletecount = 0;
BOOL    main_utf8terminal = FALSE;
int     main_vcursorscroll = 1;
int     main_vmousescroll = 1;
BOOL    main_verified_ptr = FALSE;
BOOL    main_verify = TRUE;
BOOL    main_warnings = TRUE;

usint   mark_col;
usint   mark_col_global;
BOOL    mark_hold;
int     mark_type;
linestr *mark_line;
linestr *mark_line_global;

usint match_end;
BOOL  match_L;
usint match_leftpos;
usint match_rightpos;
usint match_start;

usint mouse_col;
usint mouse_row;
BOOL  mouse_enable = TRUE;
BOOL  msgs_tty;

BOOL  no_signal_traps = FALSE;

sestr *par_begin = NULL;
sestr *par_end = NULL;
BOOL  passive_commands;

#ifndef USE_PCRE1
pcre2_general_context *re_general_context = NULL;
pcre2_compile_context *re_compile_context = NULL;
pcre2_match_data *re_match_data = NULL;
#endif

sestr *saved_se = NULL;

BOOL  screen_autoabove;
BOOL  screen_forcecls = FALSE;
usint screen_max_col = 0;            /* Applies to whole screen */
usint screen_max_row = 0;
int   screen_subchar = '?';         /* Substitute character */
BOOL  screen_suspend = TRUE;        /* Suspend for * commands */

int   sys_openfail_reason = of_other;

int   topbit_minimum = 160;         /* minimum top-bit uschar */

uschar *version_copyright;          /* Copyright string */
uschar  version_date[20];           /* Identity date */
uschar *version_string;             /* Identity of program version */
uschar  version_pcre[32];           /* Which PCRE is in use */

linestr **window_vector = NULL;

usint window_bottom;
usint window_depth;
usint window_top;
usint window_width;

void (*s_cls)(void);
void (*s_defwindow)(int, int, int);
void (*s_eraseright)(void);
void (*s_flush)(void);
void (*s_hscroll)(int, int, int, int, int);
void (*s_init)(int, int, BOOL);
void (*s_maxx)(void);
void (*s_maxy)(void);
void (*s_move)(int, int);
void (*s_overstrike)(int);
void (*s_printf)(uschar *, ...);
void (*s_putc)(int);
void (*s_rendition)(int);
void (*s_selwindow)(int, int, int);
void (*s_terminate)(void);
void (*s_vscroll)(int, int, int, BOOL);
void (*s_window)(void);
void (*s_x)(void);
void (*s_y)(void);

void  (*sys_resetterminal)(void);
void  (*sys_setupterminal)(void);


/****************  Vectors  *****************/

/* Names of keystroke actions */

keynamestr key_actnames[] = {
  { US"al", ka_al },
  { US"alp", ka_alp },
  { US"cat", ka_join },
  { US"cl", ka_cl },
  { US"clb", ka_clb },
  { US"co", ka_co },
  { US"csd", ka_csd },
  { US"csl", ka_csl },
  { US"csle", ka_csle },
  { US"csls", ka_csls },
  { US"csnl", ka_csnl },
  { US"csr", ka_csr },
  { US"cssbr", ka_cssbr },
  { US"cssl", ka_cssl },
  { US"csstl", ka_csstl },
  { US"csptb", ka_csptab },
  { US"cstb", ka_cstab },
  { US"cstl", ka_cstl },
  { US"cstr", ka_cstr },
  { US"csu", ka_csu },
  { US"cswl", ka_cswl },
  { US"cswr", ka_cswr },
  { US"cu", ka_cu },
  { US"dal", ka_dal },
  { US"dar", ka_dar },
  { US"dc", ka_dc },
  { US"de", ka_de },
  { US"dl", ka_dl },
  { US"dp", ka_dp },
  { US"dtwl", ka_dtwl },
  { US"dtwr", ka_dtwr },
  { US"gm", ka_gm },
  { US"lb", ka_lb },
  { US"pa", ka_pa },
  { US"rb", ka_rb },
  { US"rc", ka_rc },
  { US"rf", ka_reshow },
  { US"rs", ka_rs },
  { US"sb", ka_scbot },
  { US"sd", ka_scdown },
  { US"sl", ka_scleft },
  { US"sp", ka_split },
  { US"sr", ka_scright },
  { US"st", ka_sctop },
  { US"su", ka_scup },
  { US"tb", ka_tb }
};

int key_actnamecount = sizeof(key_actnames)/sizeof(keynamestr);


/* Names for special keys */

keynamestr key_names[] = {
  { US"up",        s_f_cup },
  { US"down",      s_f_cdn },
  { US"left",      s_f_clf },
  { US"right",     s_f_crt },
  { US"del",       s_f_del },
  { US"delete",    s_f_del },
  { US"bsp",       s_f_bsp },
  { US"backsp",    s_f_bsp },
  { US"backspace", s_f_bsp },
  { US"ret",       s_f_ret },
  { US"return",    s_f_ret },
  { US"tab",       s_f_tab },
  { US"ins",       s_f_ins },
  { US"insert",    s_f_ins },
  { US"home",      s_f_hom },
  { US"pup",       s_f_pup },
  { US"pageup",    s_f_pup },
  { US"pdown",     s_f_pdn },
  { US"pagedown",  s_f_pdn },
  { US"pagedn",    s_f_pdn },
  { US"end",       s_f_end },
  { US"",          0 }
};

/* Names of mark types */

const char *mark_type_names[] = { "unset", "lines", "text", "rectangle" };

/* Character type table. Some of this could be done with standard C functions,
but it's easier just to carry over the classification used in the previous BCPL
implementation of E. In any case, there are some private things that would need
doing specially. The table is set up dynamically, so as to be independent of
the character set, and changeable by the "word" command. */

uschar ch_tab[256];

/* Vector of pointers to keystrings */

uschar *main_keystrings[max_keystring+1];

/* The default setting for the keystroke translation table. The user can cause
the contents of this table to be changed. */

short int key_table[] = {
  0,        /* dummy  */
ka_al,      /* ctrl/a */
ka_lb,      /* ctrl/b */
ka_cl,      /* ctrl/c */
ka_reshow,  /* ctrl/d */
ka_co,      /* ctrl/e */
57,         /* ctrl/f */
ka_rc,      /* ctrl/g */
ka_scleft,  /* ctrl/h */
ka_cstab,   /* ctrl/i */
ka_scdown,  /* ctrl/j */
ka_scup,    /* ctrl/k */
ka_scright, /* ctrl/l */
ka_split,   /* ctrl/m = RETURN */
ka_gm,      /* ctrl/n */
60,         /* ctrl/o calls keystring 60 */
ka_pa,      /* ctrl/p */
ka_de,      /* ctrl/q */
ka_rb,      /* ctrl/r */
ka_rs,      /* ctrl/s */
ka_tb,      /* ctrl/t */
ka_dl,      /* ctrl/u */
ka_dar,     /* ctrl/v */
ka_cu,      /* ctrl/w */
ka_dal,     /* ctrl/x */
ka_dc,      /* ctrl/y */
ka_alp,     /* ctrl/z */
0,          /* ctrl/[ */
ka_cssl,    /* ctrl/\ */
0,          /* ctrl/] */
58,         /* ctrl/^ calls keystring 58 */
59,         /* ctrl/_ calls keystring 59 (ctrl with / also generates 31) */

ka_csu, ka_scup,    ka_sctop, 0,            /* cursor up */
ka_csd, ka_scdown,  ka_scbot, 0,            /* cursor down */
ka_csl, ka_scleft,  ka_cstl,  ka_csls,      /* cursor left */
ka_csr, ka_scright, ka_cstr,  ka_csle,      /* cursor right */

ka_dp, ka_clb, ka_dal, 0,   /* del */
ka_dp, ka_clb, ka_dal, 0,   /* backspace */
ka_split, 0, 0, 0,          /* return */
ka_cstab, 0, ka_csptab, 0,  /* tab */
ka_pa, 0, 0, 0,             /* ins */
0, 0, 0, 0,                 /* home */
0, 0, 0, 0,                 /* page up */
0, 0, 0, 0,                 /* page down */
0, 0, 0, 0,                 /* end */

1,           /* F1 */
2,           /* F2 */
3,           /* F3 */
4,           /* F4 */
5,           /* F5 */
6,           /* F6 */
7,           /* F7 */
8,           /* F8 */
9,           /* F9 */
10,          /* F10 */

11,          /* F11 */
12,          /* F12 */
13,          /* F13 */
14,          /* F14 */
15,          /* F15 */
16,          /* F16 */
17,          /* F17 */
18,          /* F18 */
19,          /* F19 */
20,          /* F20 */

ka_dp,       /* F21 */  /* Because DELETE does this in xterm */
22,          /* F22 */
23,          /* F23 */
24,          /* F24 */
25,          /* F25 */
26,          /* F26 */
27,          /* F27 */
28,          /* F28 */
29,          /* F29 */
30};         /* F30 */


/* Table for actual control keys that are not changeable by the user. We have
this here so that it is next to the previous one (for convenience). The only
purpose of this translation is to get values into the same value space (ka_xxx)
as other keys. */

short int key_fixedtable[] = {
  0,               /* s_f_ignore */
  ka_ret,          /* s_f_return */
  ka_wtop,         /* s_f_top */
  ka_wbot,         /* s_f_bottom */
  ka_wleft,        /* s_f_left */
  ka_wright,       /* s_f_right */
  ka_dpleft,       /* s_f_leftdel */
  ka_last,         /* s_f_lastchar */
  ka_forced,       /* s_f_forcedend */
  ka_reshow,       /* s_f_reshow */
  ka_scleft,       /* s_f_scrleft */
  ka_scright,      /* s_f_scrright */
  ka_scup,         /* s_f_scrup */
  ka_scdown,       /* s_f_scrdown */
  ka_sctop,        /* s_f_scrtop */
  ka_scbot,        /* s_f_scrbot */
  ka_dar,          /* s_f_delrt */
  ka_dal,          /* s_f_dellf */
  ka_csls,         /* s_f_startline */
  ka_csle,         /* s_f_endline */
  ka_cswl,         /* s_f_wordlf */
  ka_cswr,         /* s_f_wordrt */
  ka_csnl,         /* s_f_nextline */
  ka_csstl,        /* s_f_topleft */
  ka_cssbr,        /* s_f_botright */
  ka_rc,           /* s_f_readcom */
  ka_pa,           /* s_f_paste */
  ka_tb,           /* s_f_tb */
  ka_rb,           /* s_f_rb */
  ka_cu,           /* s_f_cut */
  ka_co,           /* s_f_copy */
  ka_de,           /* s_f_dmarked */
  ka_dc,           /* s_f_dc */
  ka_dp,           /* s_f_dp */
  ka_dl,           /* s_f_delline */
  ka_xy,           /* s_f_xy */
  ka_mscr_down,    /* s_f_mscr_down */
  ka_mscr_up       /* s_f_mscr_up */
  };

/* Names of key actions, for printing out */

uschar *key_actionnames[] = {
  US"align line(s) with cursor",
  US"align line(s) with previous",
  US"close up spaces to right",
  US"close up spaces to left",
  US"copy text or rectangle",
  US"cursor down",
  US"cursor left",
  US"cursor to line start",
  US"cursor to line end",
  US"cursor to next line",
  US"cursor to left of text",
  US"cursor to right of text",
  US"cursor right",
  US"cursor to bottom right",
  US"cursor to left of screen",
  US"cursor to top left",
  US"cursor to next tab stop",
  US"cursor to previous tab",
  US"cursor up",
  US"cursor to previous word",
  US"cursor to next word",
  US"cut text or rectangle",
  US"delete left in line(s)",
  US"delete right in line(s)",
  US"delete character at cursor",
  US"delete text or rectangle",
  US"delete line(s)",
  US"delete previous character",
  US"delete to start word left",
  US"delete to start word right",
  US"set global mark",
  US"concatenate lines",
  US"start bulk line operation",
  US"paste text or rectangle",
  US"start rectangular operation",
  US"refresh screen",
  US"prompt for command line",
  US"insert rectangle of spaces",
  US"scroll to end of buffer",
  US"scroll down",
  US"scroll left",
  US"scroll right",
  US"scroll to top of buffer",
  US"scroll up",
  US"split line",
  US"start text operation"
};


/* Names of special keys */

uschar *key_specialnames[] = {
  US"up     ",
  US"down   ",
  US"left   ",
  US"right  ",
  US"delete ",
  US"backsp ",
  US"return ",
  US"tab    ",
  US"insert ",
  US"home   ",
  US"pageup ",
  US"pagedn ",
  US"end    "
};

/* End of globals_c */
