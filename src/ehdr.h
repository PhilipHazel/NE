/***********************************************************
*             The E text editor - 3rd incarnation          *
***********************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2024 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: September 2024 */


/* This is the main header file, imported by all other sources. */


/***********************************************************
*                    Includes of other headers             *
***********************************************************/

#include "config.h"
#include "mytypes.h"

#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>  /* for SunOS' benefit (saves NULL redefined warnings) */
#include <unistd.h>



/***********************************************************
*                  Global Macros                           *
***********************************************************/

#define BIGNUMBER   0x7fffffff
#define MAX_RMARGIN    1000000
#define MAX_LINELENGTH  100000
#define MAX_ERRORS          50    /* Max errors without an interaction */

#define MAX_FROM            50    /* max from files */
#define BLOCK_SCROLL_MIN     6    /* minimum block size for scroll adjust */

#define MATCH_OK             0    /* returns from cmd_matchxx functions */
#define MATCH_FAILED       (+1)
#define MATCH_ERROR        (-1)

#define intbits      (sizeof(int)*8)  /* number of bits in an int */
#define qsmapsize   (32/sizeof(int))  /* number of bytes in a 256-bit map */

#define message_window       1
#define first_window         2

#define max_fkey            30    /* max function key */
#define max_keystring       60    /* max function keystring */
#define max_undelete       100    /* maximum undelete lines */
#define max_wordlen         19

#define CMD_BUFFER_SIZE    512
#define cmd_stacktop       100
#define FNAME_BUFFER_SIZE 4096    /* file names can be long */

#define back_size           20    /* number of "back" reference regions */

#define mac_skipspaces(a)  while (*a == ' ') a++

/* Graticules flags */

#define dg_none        0  /* nothing to be drawn */
#define dg_both        1  /* re-draw both */
#define dg_bottom      2  /* re-draw bottom one only */
#define dg_flags       4  /* re-draw flag info only */
#define dg_margin      8  /* re-draw margin info */
#define dg_top        16  /* re-draw top only */

/* Character types */

#define ch_ucletter     0x01
#define ch_lcletter     0x02
#define ch_letter       0x03
#define ch_digit        0x04
#define ch_qualletter   0x08
#define ch_delim        0x10
#define ch_word         0x20
#define ch_hexch        0x40
#define ch_filedelim    0x80

/* Flags for certain qualifier strings on replacements */

#define rqs_XRonly       1
#define rqs_Xonly        2

/* Flags for the if command */

#define if_prompt        1
#define if_mark          2
#define if_eol           4
#define if_sol           8
#define if_sof          16
#define if_if           32
#define if_unless       64   /* Unless must be last! */



/***********************************************************
*                     Structures                           *
***********************************************************/

/* The structures are held in a separate file for convenience */

#include "structs.h"



/***********************************************************
*                    Typedefs                              *
***********************************************************/

typedef void (*cmd_Cproc)(cmdstr *);      /* The type of compile functions */
typedef int  (*cmd_Eproc)(cmdstr *);      /* The type of execute functions */



/***********************************************************
*                    Enumerations                          *
***********************************************************/

enum { lb_align, lb_delete, lb_eraseright, lb_eraseleft, lb_closeup,
       lb_closeback, lb_rectsp, lb_alignp };

enum { mark_unset, mark_lines, mark_text, mark_rect };

enum { amark_line, amark_limit, amark_text, amark_rectangle, amark_unset,
       amark_hold };

enum { ktype_data, ktype_function };  /* Keystroke types */

enum { done_continue, done_error, done_finish, done_wait, done_loop,
       done_break, done_eof };

enum { sh_insert, sh_topline, sh_above };

enum { cuttype_text, cuttype_rect };

enum { show_ckeys = 1, show_fkeys, show_xkeys, show_allkeys,
  show_keystrings, show_buffers, show_wordcount, show_version,
  show_actions, show_commands, show_wordchars, show_settings,
  show_allsettings };

enum { abe_a, abe_b, abe_e };

enum { cbuffer_c, cbuffer_cd };

enum { set_autovscroll = 1, set_autovmousescroll, set_splitscrollrow,
  set_oldcommentstyle, set_newcommentstyle };

enum { debug_crash = 1, debug_exceedstore, debug_nullline, debug_baderror };

enum { detrail_buffer, detrail_output };

enum { backup_files };

enum { ci_move, ci_type, ci_read, ci_cmd, ci_delete, ci_scan, ci_loop };

enum { of_other, of_existence };



/***********************************************************
*              Macros for character handling               *
***********************************************************/

/* When allow_wide is false, one character equals one byte. Otherwise, we look
for valid UTF-8 byte sequences, checking for the availability of additional
UTF-8 bytes. If the sequence isn't valid, or there are not enough bytes, a
single byte is assumed to be one character. The validity check used here is a
simple one: the first byte must contain the 0xc0 bits, and the appropriate
number of subsequent bytes must have their top two bits set to 0x80. */

/* Get the next character, not advancing the pointer. */

#define GETCHAR(c, ptr, eptr) \
  c = *ptr; \
  if (allow_wide && c >= 0xc0) \
    { \
    int gcaa = utf8_table4[c & 0x3f];  /* Number of additional bytes */ \
    if (ptr + gcaa < eptr)             /* Check they are available */ \
      { \
      int gci; \
      for (gci = 1; gci <= gcaa; gci++)         /* Check their top 2 bits */ \
        if ((ptr[gci] & 0xc0) != 0x80) break; \
      if (gci > gcaa)                           /* Valid UTF-8 sequence */ \
        { \
        int gcss = 6*gcaa; \
        c = (c & utf8_table3[gcaa]) << gcss; \
        for (gci = 1; gci <= gcaa; gci++) \
          { \
          gcss -= 6; \
          c |= (ptr[gci] & 0x3f) << gcss; \
          } \
        } \
      } \
    }

/* Get the next character, advancing the pointer. */

#define GETCHARINC(c, ptr, eptr) \
  c = *ptr++; \
  if (allow_wide && c >= 0xc0) \
    { \
    int gcaa = utf8_table4[c & 0x3f];  /* Number of additional bytes */ \
    if (ptr + gcaa - 1 < eptr)         /* Check they are available */ \
      { \
      int gci; \
      for (gci = 0; gci < gcaa; gci++)         /* Check their top 2 bits */ \
        if ((ptr[gci] & 0xc0) != 0x80) break; \
      if (gci >= gcaa)                         /* Valid UTF-8 sequence */ \
        { \
        int gcss = 6*gcaa; \
        c = (c & utf8_table3[gcaa]) << gcss; \
        while (gcaa-- > 0) \
          { \
          gcss -= 6; \
          c |= (*ptr++ & 0x3f) << gcss; \
          } \
        } \
      } \
    }

/* Advance over one character, given pointer */

#define SKIPCHAR(ptr, eptr) \
  if (*ptr++ >= 0xc0 && allow_wide) \
    { \
    int gcaa = utf8_table4[ptr[-1] & 0x3f];   /* Number of additional bytes */ \
    if (ptr + gcaa - 1 < eptr)                /* Check they are available */ \
      { \
      int gci; \
      for (gci = 0; gci < gcaa; gci++)        /* Check their top 2 bits */ \
        if ((ptr[gci] & 0xc0) != 0x80) break; \
      if (gci >= gcaa) ptr += gcaa;           /* Valid UTF-8 sequence */ \
      } \
    }

/* Advance over one character, given offset */

#define SKIPCHAROFFSET(p, bptr, pmax) \
  if (p++ < pmax && bptr[p-1] >= 0xc0 && allow_wide) \
    { \
    int gcaa = utf8_table4[bptr[p-1] & 0x3f]; /* Number of additional bytes */ \
    if (p + gcaa - 1 < pmax)                  /* Check they are available */ \
      { \
      int gci; \
      for (gci = 0; gci < gcaa; gci++)        /* Check their top 2 bits */ \
        if ((bptr[p+gci] & 0xc0) != 0x80) break; \
      if (gci >= gcaa) p += gcaa;             /* Valid UTF-8 sequence */ \
      } \
    }

/* Retreat over one character, given pointer */

#define BACKCHAR(ptr, bptr) \
  if (ptr > bptr && (*(--ptr) & 0xc0) == 0x80 && allow_wide) \
    { \
    uschar *bctt = ptr; \
    while (bctt > bptr && (*(--bctt) & 0xc0) == 0x80); \
    if (*bctt >= 0xc0 && \
         ptr - bctt == utf8_table4[*bctt & 0x3f]) ptr = bctt; \
    }

/* Retreat over one character, given offset */

#define BACKCHAROFFSET(p, bptr) \
  if (p > 0 && (bptr[--p] & 0xc0) == 0x80 && allow_wide) \
    { \
    uschar *bctt = bptr + p; \
    while (bctt > bptr && (*(--bctt) & 0xc0) == 0x80); \
    if (*bctt >= 0xc0 && \
         bptr + p - bctt == utf8_table4[*bctt & 0x3f]) p = bctt - bptr; \
    }



/***********************************************************
*                  Global variables                        *
***********************************************************/

extern FILE *crash_logfile;            /* For logging info on crashes */
extern FILE *debug_file;               /* For general debugging */

extern FILE *cmdin_fid;                /* command input */
extern FILE *msgs_fid;                 /* message ougput */
extern FILE *kbd_fid;                  /* keyboard input */
extern FILE *withkey_fid;              /* simulated keystroke data */
extern usint withkey_sleep;            /* simulated keystrokes sleep time */

extern BOOL    allow_wide;             /* TRUE to recognized UTF-8 */

extern uschar *arg_from_name;          /* Names on command line */
extern uschar *arg_to_name;
extern uschar *arg_ver_name;
extern uschar *arg_with_name;

extern bufferstr *currentbuffer;

extern uschar ch_displayable[];        /* Table of screen-displayable chars */
extern uschar ch_tab[];                /* Table for identifying char types */

extern cmd_Cproc cmd_Cproclist[];      /* List of command compile functions */
extern cmd_Eproc cmd_Eproclist[];      /* List of command execute functions */

extern int     cmd_bracount;           /* Bracket level */
extern int     cmd_breakloopcount;     /* Break count */
extern uschar *cmd_buffer;             /* Buffer for reading cmd lines */
extern BOOL    cmd_casematch;          /* Case match switch */
extern linestr *cmd_cbufferline;       /* Next line in cbuffer */
extern int     cmd_clineno;            /* Line number in C or CBUFFER */
extern uschar *cmd_cmdline;            /* Current command line being compiled */
extern uschar *cmd_list[];             /* List of command names */
extern int     cmd_listsize;           /* Size of command list */
extern BOOL    cmd_faildecode;         /* Failure flag */
extern int     cmd_ist;                /* Delimiter for inserting in cmd concats */
extern BOOL    cmd_eoftrap;            /* Trapping eof */
extern BOOL    cmd_onecommand;         /* Command line is a single command */
extern uschar *cmd_ptr;                /* Current pointer */
extern BOOL    cmd_refresh;            /* Refresh needed */
extern uschar *cmd_qualletters;        /* Qualifier letters */
extern uschar *cmd_stack[];            /* Stack of old command lines */
extern int     cmd_stackptr;           /* Pointer in command stack */
extern uschar  cmd_word[];             /* Word buffer */

extern BOOL    crash_handler_chatty;   /* To supress for SIGHUP */

extern usint   cursor_row;             /* Relative screen row */
extern usint   cursor_col;             /* Absolute column within line */
extern usint   cursor_max;             /* Max abs value on current screen */
extern usint   cursor_rh_adjust;       /* Adjust for auto scrolling */
extern usint   cursor_offset;          /* Left most screen column */

extern linestr *cut_buffer;            /* start of cut buffer */
extern linestr *cut_last;              /* last line in same */
extern BOOL    cut_pasted;             /* set when buffer is pasted */
extern int     cut_type;               /* type of last cut */

extern int     default_rmargin;        /* For making/reinitializing windows */

extern int     error_count;
extern jmp_buf error_jmpbuf;           /* For disastrous errors */
extern BOOL    error_werr;             /* Force window-type error */

extern filewritstr *files_written;     /* Chain of written file names */

extern uschar *key_actionnames[];
extern int     key_actnamecount;       /* size of following table */
extern keynamestr key_actnames[];      /* names for key actions */
extern uschar  key_codes[256];
extern usint   key_controlmap;         /* map for existing keys */
extern short int key_fixedtable[];     /* tables for interpreting keys */
extern usint   key_functionmap;
extern keynamestr key_names[];         /* names for special keys */
extern usint   key_specialmap[4];
extern uschar *key_specialnames[];
extern short int key_table[];

extern sestr *last_se;                 /* last search expression */
extern sestr *last_abese;              /* last se for abe */
extern qsstr *last_abent;              /* last replacement for abe */
extern sestr *last_gse;                /* last se for global */
extern qsstr *last_gnt;                /* last replacement qs for global */

extern linestr *main_bottom;           /* last line */
extern linestr *main_current;          /* current line */
extern linestr *main_top;              /* first line */

extern bufferstr *main_bufferchain;

extern BOOL    main_appendswitch;      /* cut append option */
extern BOOL    main_attn;              /* attention on/off switch */
extern BOOL    main_AutoAlign;
extern backstr *main_backlist;         /* list of "back" positions */
extern usint   main_backnext;          /* next backup to use */
extern usint   main_backtop;           /* topmost recorded position */
extern usint   main_backregionsize;    /* size of "back" region */
extern BOOL    main_backupfiles;       /* flag for auto-backing up */
extern BOOL    main_binary;            /* handle lines as binary */
extern int     main_cicount;           /* check interrupt count */
extern BOOL    main_detrail_output;    /* detrail all output lines */
extern BOOL    main_done;              /* job is finished */
extern int     main_drawgraticules;    /* an option value */
extern BOOL    main_eightbit;          /* display option */
extern uschar *main_einit;             /* initializing file name */
extern BOOL    main_eoftrap;           /* until eof flag */
extern BOOL    main_escape_pressed;    /* a human interrupt */
extern uschar *main_fromlist[];        /* additional input files */
extern BOOL    main_initialized;       /* initial cmds obeyed */
extern BOOL    main_filechanged;       /* file has been edited */
extern uschar *main_filealias;
extern uschar *main_filename;
extern usint   main_hscrollamount;     /* left/right scroll value */
extern int     main_ilinevalue;        /* boundary between up/down scroll */
extern BOOL    main_interactive;       /* set if interactive */
extern int     main_imax;              /* number of last line read */
extern int     main_imin;              /* number of last insert */
extern usint   main_linecount;         /* number of lines in current buffer */
extern BOOL    main_logging;           /* turns on debugging logging */
extern BOOL    main_nlexit;            /* needs NL on exit */
extern BOOL    main_noinit;            /* don't obey init string */
extern uschar *main_keystrings[];      /* variable keystrings */
extern linestr *main_lastundelete;     /* last undelete structure */
extern BOOL    main_leave_message;     /* leave msg in bottom window after cmds */
extern int     main_nextbufferno;      /* next number to use */
extern int     main_oldcomment;        /* old-style comments flag */
extern BOOL    main_nowait;            /* no wait before screen refresh */
extern BOOL    main_oneattn;           /* had one suppressed attention */
extern uschar *main_opt;               /* the "opt" argument */
extern BOOL    main_overstrike;
extern BOOL    main_pendnl;            /* pending nl if more line-by-line output */
extern procstr *main_proclist;         /* function chain */
extern int     main_rc;                /* The final return code */
extern BOOL    main_readonly;          /* Buffer is read only */
extern BOOL    main_repaint;           /* Force screen repaint after command */
extern usint   main_rmargin;           /* current margin */
extern BOOL    main_screenmode;        /* true if full-screen operation */
extern BOOL    main_screenOK;          /* screen mode and not suspended */
extern BOOL    main_screensuspended;   /* screen temporarily suspended */
extern BOOL    main_selectedbuffer;    /* true if buffer has changed */
extern BOOL    main_shownlogo;         /* FALSE if need to show logo on error */
extern BOOL    main_tabflag;           /* Flag tabbed input lines */
extern BOOL    main_tabin;             /* the tabin option */
extern BOOL    main_tabout;            /* the tabout option */
extern uschar *main_tabs;              /* the default tabs text option */
extern linestr *main_undelete;         /* first undelete structure */
extern int     main_undeletecount;     /* count of lines */
extern BOOL    main_utf8terminal;      /* Terminal is UTF-8 */
extern int     main_vcursorscroll;     /* Vertical scroll amount */
extern int     main_vmousescroll;      /* Ditto, for mouse scroll */
extern BOOL    main_verified_ptr;      /* Non-null > has been output */
extern BOOL    main_verify;            /* Auto verification */
extern BOOL    main_warnings;          /* warnings option */

extern usint   mark_col;               /* Column for non-global marks */
extern usint   mark_col_global;        /* Column for global mark */
extern BOOL    mark_hold;              /* TRUE for hold lines mark */
extern int     mark_type;              /* Type of non-global mark */
extern linestr *mark_line;             /* Non-global marked line */
extern linestr *mark_line_global;      /* Global marked line */
extern const char *mark_type_names[];  /* Names of mark types */

extern usint   match_end;
extern BOOL    match_L;                /* Leftwards match */
extern usint   match_leftpos;
extern usint   match_start;
extern usint   match_rightpos;

extern usint   mouse_col;
extern usint   mouse_row;
extern BOOL    mouse_enable;
extern BOOL    msgs_tty;               /* TRUE if msgs_fid is a tty */

extern sestr  *par_begin;
extern sestr  *par_end;
extern BOOL    passive_commands;

extern pcre2_general_context *re_general_context;
extern pcre2_compile_context *re_compile_context;
extern pcre2_match_data *re_match_data;

extern BOOL    screen_autoabove;
extern BOOL    screen_forcecls;        /* Force a complete refresh */
extern int     screen_subchar;         /* Substitute character */
extern BOOL    screen_suspend;         /* Set to cause suspension over * commands */

extern usint   screen_max_col;         /* Applies to full screen */
extern usint   screen_max_row;

extern int     signal_list[];          /* Crash signals */
extern uschar *signal_names[];         /* and their names */

extern int     sys_openfail_reason;    /* Reason for open failure */

extern int     topbit_minimum;         /* lowest top-bit char */

extern const   int utf8_table3[];      /* Globally used UTF-8 table */
extern const   uschar utf8_table4[];   /* Globally used UTF-8 table */

extern uschar *version_copyright;      /* Copyright string */
extern uschar  version_date[];         /* Identity date */
extern uschar *version_string;         /* Identity of program version */
extern uschar  version_pcre[];         /* Which PCRE2 is in use */

extern linestr **window_vector;        /* points to vector of lines being displayed */

/* The row values count downwards from the top of the screen, with the first
line on the screen being row 0. */

extern usint   window_bottom;          /* row number of bottom text line */
extern usint   window_depth;           /* bottom - top (0-based bottom text row) */
extern usint   window_top;             /* row number of first line (always 1) */
extern usint   window_width;


/***********************************************************
*                     Global functions                     *
***********************************************************/

/* Note that we have to use char * rather than uschar * for those functions
that are declared PRINTF_FUNCTION because the compiler isn't clever enough. It
grumbles "format string argument not a string type", even if you explicitly
write "unsigned char *". */

extern void    debug_printf(const char *, ...) PRINTF_FUNCTION;
extern void    debug_screen(void);
extern void    debug_writelog(const char *, ...) PRINTF_FUNCTION;

extern BOOL    cmd_atend(void);
extern cmdstr *cmd_compile(void);
extern int     cmd_confirmoutput(uschar *, BOOL, BOOL, int, uschar **);
extern void   *cmd_copyblock(cmdblock *);
extern BOOL    cmd_emptybuffer(bufferstr *, uschar *);
extern bufferstr *cmd_findbuffer(int);
extern BOOL    cmd_findproc(uschar *, procstr **);
extern void    cmd_freeblock(cmdblock *);
extern cmdstr *cmd_getcmdstr(int);
extern BOOL    cmd_joinline(BOOL);
extern BOOL    cmd_makeCRE(qsstr *);
extern int     cmd_matchqsR(qsstr *, linestr *, int);
extern int     cmd_matchse(sestr *, linestr *);
extern int     cmd_obey(uschar *);
extern int     cmd_obeyline(cmdstr *);
extern int     cmd_readnumber(void);
extern BOOL    cmd_readprocname(stringstr **name);
extern BOOL    cmd_readqualstr(qsstr **, int);
extern BOOL    cmd_readse(sestr **);
extern int     cmd_readstring(stringstr **);
extern int     cmd_readUstring(stringstr **);
extern void    cmd_readword(void);
extern linestr *cmd_ReChange(linestr *, uschar *, usint, BOOL, BOOL, BOOL);
extern void    cmd_recordchanged(linestr *, int);
extern BOOL    cmd_yesno(const char *, ...) PRINTF_FUNCTION;

extern void    crash_handler(int);

extern BOOL    cut_cut(linestr *, usint, int, BOOL, BOOL);
extern void    cut_pasterect(void);
extern BOOL    cut_overwrite(uschar *);
extern int     cut_pastetext(void);

extern void    error_moan(int, ...);
extern void    error_moan_decode(int, ...);
extern void    error_moanqse(int, sestr *);
extern void    error_printf(const char *, ...) PRINTF_FUNCTION;
extern void    error_printflush(void);

extern linestr *file_nextline(FILE *, size_t *);
extern BOOL    file_save(uschar *);
extern void    file_setwritten(uschar *);
extern BOOL    file_written(uschar *);
extern int     file_writeline(linestr *, FILE *);

extern void    init_buffer(bufferstr *, int, uschar *, uschar *, FILE *);
extern BOOL    init_init(FILE *, uschar *, uschar *);
extern void    init_selectbuffer(bufferstr *);

extern void    key_handle_data(int);
extern void    key_handle_function(int);
extern BOOL    key_set(uschar *, BOOL);
extern void    key_setfkey(int, uschar *);

extern int     line_bytecount(uschar *, int);
extern usint   line_charcount(uschar *, usint);
extern int     line_checkabove(linestr *);
extern linestr *line_concat(linestr *, int);
extern linestr *line_copy(linestr *);
extern linestr *line_cutpart(linestr *, int, int, BOOL);
extern linestr *line_delete(linestr *, BOOL);
extern void    line_deletech(linestr *, int, int, BOOL);
extern void    line_deletebytes(linestr *, int, int, BOOL);
extern void    line_formatpara(BOOL);
extern void    line_insertbytes(linestr *, int, int, uschar *, int, usint);
extern void    line_leftalign(linestr *, int, int *);
extern usint   line_offset(linestr *, int);
extern int     line_soffset(uschar *, uschar *, int);
extern linestr *line_split(linestr *, usint);
extern void    line_verify(linestr *, BOOL, BOOL);

extern void    main_flush_interrupt(void);
extern BOOL    main_interrupted(int);

extern BOOL    no_signal_traps;

extern void    obey_init(uschar *);
extern int     ord2utf8(int, uschar *);

extern int rdargs(int, char **, uschar *, arg_result *);

extern void    scrn_display(void);
extern void    scrn_displayline(linestr *, int, int);
extern void    scrn_hint(int, usint, linestr *);
extern void    scrn_init(BOOL);
extern void    scrn_invertchars(linestr *, int, int, int, int);
extern void    scrn_rdline(BOOL, uschar *);
extern void    scrn_restore(void);
extern void    scrn_scrollby(int);
extern void    scrn_setsize(void);
extern void    scrn_suspend(void);
extern void    scrn_windows(void);

extern int     setup_dbuffer(bufferstr *);

extern void    store_chop(void *, size_t);
extern void   *store_copy(void *);
extern linestr *store_copyline(linestr *);
extern uschar *store_copystring(uschar *);
extern uschar *store_copystring2(uschar *, uschar *);
extern void    store_free(void *);
extern void    store_freequeuecheck(void);
extern void    store_free_all(void);
extern void   *store_get(size_t);
extern void   *store_getlbuff(size_t);
extern void    store_init(void);
extern void   *store_Xget(size_t);

extern uschar *sys_argstring(uschar *);
extern void    sys_beep(void);
extern uschar *sys_checkfilename(uschar *);
extern void    sys_checkinterrupt(int);
extern int     sys_cmdkeystroke(int *);
extern uschar *sys_crashfilename(int);
extern void    sys_crashposition(void);
extern void    sys_display_cursor(int);
extern int     sys_fcomplete(int, int *);
extern FILE   *sys_fopen(uschar *, uschar *);
extern void    sys_init1(void);
extern void    sys_init2(uschar *);
extern uschar *sys_keyreason(int);
extern void    sys_mprintf(FILE *, const char *, ...) FPRINTF_FUNCTION;
extern void    sys_mouse(BOOL);
extern int     sys_rc(int);
extern void    sys_runscreen(void);
extern void    sys_runwindow(void);
extern void    sys_specialnotes(usint *, void(*)(usint, usint *));
extern void    sys_tidy_up(void);
extern int     utf82ord(uschar *, int *);
extern void    version_init(void);

/* Function variables */

extern void    (*sys_resetterminal)(void);
extern void    (*sys_setupterminal)(void);

/* End of ehdr.h */
