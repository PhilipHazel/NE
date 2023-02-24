/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */


/* This file contains the system-specific routines for Unix-like environments,
with the exception of the window-specific code, which lives in its own modules.
*/

#include <memory.h>
#include <fcntl.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include "ehdr.h"
#include "unixhdr.h"
#include "scomhdr.h"
#include "keyhdr.h"

/* FreeBSD needs this file for FIONREAD */

#ifndef FIONREAD
#include <sys/filio.h>
#endif

#define tc_keylistsize 2048


/* List of signals to be trapped for buffer dumping on crashes to be effective.
Names are for use in messages. SIGHUP is handled specially, so does not appear
here - otherwise problems with dedicated xterm windows. */

/* SIGXFSZ and SIGXCPU not on HP-UX */
/* SIGSYS not on Linux */

int signal_list[] = {
  SIGQUIT, SIGILL,  SIGIOT,
  SIGFPE,  SIGBUS,  SIGSEGV, SIGTERM,
#ifdef SIGSYS
  SIGSYS,
#endif
#ifdef SIGXCPU
  SIGXCPU,
#endif
#ifdef SIGXFSZ
  SIGXFSZ,
#endif
  -1 };

uschar *signal_names[] = {
  US"(SIGQUIT)", US"(SIGILL)",  US"(SIGIOT)",
  US"(SIGFPE)",  US"(SIGBUS)",  US"(SIGSEGV)", US"(SIGTERM)",
#ifdef SIGSYS
  US"(SIGSYS)",
#endif
#ifdef SIGXCPU
  US"(SIGXCPU)",
#endif
#ifdef SIGXFSZ
  US"(SIGXFSZ)",
#endif
  US"" };

/* Define alternative strings for termcap/info items */

#ifdef HAVE_TERMCAP
#define TCI_AL   "al"
#define TCI_BC   "bc"
#define TCI_CD   "ce"
#define TCI_CL   "cl"
#define TCI_CM   "cm"
#define TCI_CS   "cs"
#define TCI_DC   "dc"
#define TCI_DL   "dl"
#define TCI_DM   "dm"
#define TCI_IC   "ic"
#define TCI_IM   "im"
#define TCI_IP   "ip"
#define TCI_KE   "ke"
#define TCI_KS   "ks"
#define TCI_PC   "pc"
#define TCI_SE   "se"
#define TCI_SF   "sf"
#define TCI_SO   "so"
#define TCI_SR   "sr"
#define TCI_TE   "te"
#define TCI_TI   "ti"
#define TCI_UP   "up"

#define TCI_KU   "ku"
#define TCI_KD   "kd"
#define TCI_KL   "kl"
#define TCI_KR   "kr"
#define TCI_KDC  "kD"
#define TCI_KSDC "*4"

#define TCI_KHOME "kh"
#define TCI_KEND  "@7"
#define TCI_KPPAGE "kP"
#define TCI_KNPAGE "kN"
#define TCI_KINSRT "kl"

#define TCI_K0   "k0"
#define TCI_K1   "k1"
#define TCI_K2   "k2"
#define TCI_K3   "k3"
#define TCI_K4   "k4"
#define TCI_K5   "k5"
#define TCI_K6   "k6"
#define TCI_K7   "k7"
#define TCI_K8   "k8"
#define TCI_K9   "k9"
#define TCI_KK   "k;"
#define TCI_F1   "F1"
#define TCI_F2   "F2"
#define TCI_F3   "F3"
#define TCI_F4   "F4"
#define TCI_F5   "F5"
#define TCI_F6   "F6"
#define TCI_F7   "F7"
#define TCI_F8   "F8"
#define TCI_F9   "F9"
#define TCI_FA   "FA"
#define TCI_FB   "FB"
#define TCI_FC   "FC"
#define TCI_FD   "FD"
#define TCI_FE   "FE"
#define TCI_FF   "FF"
#define TCI_FG   "FG"
#define TCI_FH   "FH"
#define TCI_FI   "FI"
#define TCI_FJ   "FJ"
#define TCI_FK   "FK"

#else  /* This is for terminfo */
#define TCI_AL   "il1"
#define TCI_BC   "cub1"
#define TCI_CD   "ed"
#define TCI_CL   "clear"
#define TCI_CM   "cup"
#define TCI_CS   "csr"
#define TCI_DC   "dch1"
#define TCI_DL   "dl1"
#define TCI_DM   "smdc"
#define TCI_IC   "ich1"
#define TCI_IM   "smir"
#define TCI_IP   "ip"
#define TCI_KE   "rmkx"
#define TCI_KS   "smkx"
#define TCI_PC   "pad"
#define TCI_SE   "rmso"
#define TCI_SF   "ind"
#define TCI_SO   "smso"
#define TCI_SR   "ri"
#define TCI_TE   "rmcup"
#define TCI_TI   "smcup"
#define TCI_UP   "cuu1"

#define TCI_KU   "kcuu1"
#define TCI_KD   "kcud1"
#define TCI_KL   "kcub1"
#define TCI_KR   "kcuf1"
#define TCI_KDC  "kdch1"
#define TCI_KSDC "kDC"

#define TCI_KHOME "khome"
#define TCI_KEND  "kend"
#define TCI_KPPAGE "kpp"
#define TCI_KNPAGE "knp"
#define TCI_KINSRT "kich1"

#define TCI_K0   "kf0"
#define TCI_K1   "kf1"
#define TCI_K2   "kf2"
#define TCI_K3   "kf3"
#define TCI_K4   "kf4"
#define TCI_K5   "kf5"
#define TCI_K6   "kf6"
#define TCI_K7   "kf7"
#define TCI_K8   "kf8"
#define TCI_K9   "kf9"
#define TCI_KK   "kf10"
#define TCI_F1   "kf11"
#define TCI_F2   "kf12"
#define TCI_F3   "kf13"
#define TCI_F4   "kf14"
#define TCI_F5   "kf15"
#define TCI_F6   "kf16"
#define TCI_F7   "kf17"
#define TCI_F8   "kf18"
#define TCI_F9   "kf19"
#define TCI_FA   "kf20"
#define TCI_FB   "kf21"
#define TCI_FC   "kf22"
#define TCI_FD   "kf23"
#define TCI_FE   "kf24"
#define TCI_FF   "kf25"
#define TCI_FG   "kf26"
#define TCI_FH   "kf27"
#define TCI_FI   "kf28"
#define TCI_FJ   "kf29"
#define TCI_FK   "kf30"
#endif



/*************************************************
*               Static data                      *
*************************************************/

#ifdef HAVE_TERMCAP
static uschar *termcap_buf = NULL;
#endif

static uschar *term_name;
static int term_type;
static BOOL use_utf8;

/* The following keystrokes are not available and so their default settings
must be squashed. Note that we leave "ctrl/tab", because esc tab maps to it and
is suitable translated for output. For known terminals some of these keys are
not squashed, because they are built into this code. There are special versions
of the table for these (nowadays only xterm). */

#define s s_f_shiftbit
#define c s_f_ctrlbit
#define sc s_f_shiftbit+s_f_ctrlbit

/* This list applies when the terminal is not an xterm. */

static uschar non_keys[] = {
  s_f_cup+s,   s_f_cup+c,     s_f_cup+sc,
  s_f_cdn+s,   s_f_cdn+c,     s_f_cdn+sc,
  s_f_clf+s,   s_f_clf+c,     s_f_clf+sc,
  s_f_crt+s,   s_f_crt+c,     s_f_crt+sc,
  0 };

/* This list applies when the terminal is an xterm, and requires some special
configuration of xterm. */

static uschar xterm_non_keys[] = {
  s_f_cup+sc,
  s_f_cdn+sc,
  s_f_clf+sc,
  s_f_crt+sc,
  0 };

#undef s
#undef c
#undef sc


/*************************************************
*           Tables of escape sequences           *
*************************************************/

typedef struct {
  uschar *string;
  int value;
} esc_item;

/* Escape sequences that are specific for xterms. These are added after the
sequences obtained from terminfo, so that terminfo ones override. In December
2013 these were changed so that I can use the default terminfo xterm (at least
for the version of X in Arch Linux). It uses some sequences that are specified
as private below for my old terminfo. These are left in, and will kick in if
not superseded by anything in the terminfo.  */

static esc_item xterm_escapes[] = {

  /* Could use some recent capabilities for some of these, but there
  isn't a complete set defined yet, and the termcaps/terminfos don't
  always have them in. */

  { US "\033[1;2D", Pkey_sh_left },
  { US "\033[1;2C", Pkey_sh_right },
  { US "\033[1;2a", Pkey_sh_up },
  { US "\033[1;2b", Pkey_sh_down },
  { US "\033[3;5~", Pkey_ct_del127 },

  { US "\033Ot", Pkey_sh_left },
  { US "\033Ov", Pkey_sh_right },
  { US "\033Ox", Pkey_sh_up },
  { US "\033Or", Pkey_sh_down },

  /* These allocations require suitable configuration of the xterm */

  { US "\033[2;2d", Pkey_ct_left },
  { US "\033[2;2c", Pkey_ct_right },
  { US "\033[2;2a", Pkey_ct_up },
  { US "\033[2;2b", Pkey_ct_down },
  { US "\033[4;t", Pkey_ct_tab },
  { US "\033[3;b", Pkey_sh_bsp },
  { US "\033[4;b", Pkey_ct_bsp },

  { US "\033OT", Pkey_ct_left },
  { US "\033OV", Pkey_ct_right },
  { US "\033OX", Pkey_ct_up },
  { US "\033OR", Pkey_ct_down },
  { US "\033OM", Pkey_ct_tab },
  { US "\033OP", Pkey_sh_del127 },
  { US "\033ON", Pkey_ct_del127 },
  { US "\033OQ", Pkey_sh_bsp },
  { US "\033OO", Pkey_ct_bsp },

  /* This recognizes the start of the sequence that returns mouse clicks */

  { US "\033[M", Pkey_xy },
  { NULL, 0}
};

/* Other escape sequences that are built into NE */

static esc_item ne_escapes[] = {
  { US "\0330", Pkey_f0+10 },
  { US "\0331", Pkey_f0+1 },
  { US "\0332", Pkey_f0+2 },
  { US "\0333", Pkey_f0+3 },
  { US "\0334", Pkey_f0+4 },
  { US "\0335", Pkey_f0+5 },
  { US "\0336", Pkey_f0+6 },
  { US "\0337", Pkey_f0+7 },
  { US "\0338", Pkey_f0+8 },
  { US "\0339", Pkey_f0+9 },

  { US "\033\0330", Pkey_f0+20 },
  { US "\033\0331", Pkey_f0+11 },
  { US "\033\0332", Pkey_f0+12 },
  { US "\033\0333", Pkey_f0+13 },
  { US "\033\0334", Pkey_f0+14 },
  { US "\033\0335", Pkey_f0+15 },
  { US "\033\0336", Pkey_f0+16 },
  { US "\033\0337", Pkey_f0+17 },
  { US "\033\0338", Pkey_f0+18 },
  { US "\033\0339", Pkey_f0+19 },

  { US "\033\033", Pkey_data },
  { US "\033\177", Pkey_null },
  { US "\033\015", Pkey_reshow },
  { US "\033\t",   Pkey_backtab },
  { US "\033s",    19 },             /* ctrl/s */
  { US "\033q",    17 },             /* ctrl/q */
  { US "\033u",    Pkey_utf8 },      /* UTF-8 by no. */
  { NULL, 0 }
};

/* Wide character escapes */

static esc_item wide_escapes[] = {
  { US "\033A`", 0x00c0 },
  { US "\033A'", 0x00c1 },
  { US "\033A^", 0x00c2 },
  { US "\033A~", 0x00c3 },
  { US "\033A.", 0x00c4 },
  { US "\033Ao", 0x00c5 },
  { US "\033AE", 0x00c6 },
  { US "\033C,", 0x00c7 },
  { US "\033E`", 0x00c8 },
  { US "\033E'", 0x00c9 },
  { US "\033E^", 0x00ca },
  { US "\033E.", 0x00cb },
  { US "\033I`", 0x00cc },
  { US "\033I'", 0x00cd },
  { US "\033I^", 0x00ce },
  { US "\033I.", 0x00cf },

  { US "\033D-", 0x00d0 },
  { US "\033N~", 0x00d1 },
  { US "\033O`", 0x00d2 },
  { US "\033O'", 0x00d3 },
  { US "\033O^", 0x00d4 },
  { US "\033O~", 0x00d5 },
  { US "\033O.", 0x00d6 },
  { US "\033O/", 0x00d8 },
  { US "\033U`", 0x00d9 },
  { US "\033U'", 0x00da },
  { US "\033U^", 0x00db },
  { US "\033U.", 0x00dc },
  { US "\033Y'", 0x00dd },
  { US "\033ss", 0x00df },

  { US "\033a`", 0x00e0 },
  { US "\033a'", 0x00e1 },
  { US "\033a^", 0x00e2 },
  { US "\033a~", 0x00e3 },
  { US "\033a.", 0x00e4 },
  { US "\033ao", 0x00e5 },
  { US "\033ae", 0x00e6 },
  { US "\033c,", 0x00e7 },
  { US "\033e`", 0x00e8 },
  { US "\033e'", 0x00e9 },
  { US "\033e^", 0x00ea },
  { US "\033e.", 0x00eb },
  { US "\033i`", 0x00ec },
  { US "\033i'", 0x00ed },
  { US "\033i^", 0x00ee },
  { US "\033i.", 0x00ef },

  { US "\033d-", 0x00f0 },
  { US "\033n~", 0x00f1 },
  { US "\033o`", 0x00f2 },
  { US "\033o'", 0x00f3 },
  { US "\033o^", 0x00f4 },
  { US "\033o~", 0x00f5 },
  { US "\033o.", 0x00f6 },
  { US "\033o/", 0x00f8 },
  { US "\033u`", 0x00f9 },
  { US "\033u'", 0x00fa },
  { US "\033u^", 0x00fb },
  { US "\033u.", 0x00fc },
  { US "\033y'", 0x00fd },
  { US "\033y.", 0x00ff },

  { US "\033A-", 0x0100 },
  { US "\033a-", 0x0101 },
  { US "\033Au", 0x0102 },
  { US "\033au", 0x0103 },
  { US "\033C'", 0x0106 },
  { US "\033c'", 0x0107 },
  { US "\033Cv", 0x010c },
  { US "\033cv", 0x010d },
  { US "\033D-", 0x0110 },
  { US "\033d-", 0x0111 },
  { US "\033E-", 0x0112 },
  { US "\033e-", 0x0113 },
  { US "\033E.", 0x0116 },
  { US "\033e.", 0x0117 },
  { US "\033Ev", 0x011a },
  { US "\033ev", 0x011b },
  { US "\033l/", 0x0142 },
  { US "\033N'", 0x0143 },
  { US "\033n'", 0x0144 },
  { US "\033Nv", 0x0147 },
  { US "\033nv", 0x0148 },
  { US "\033O-", 0x014c },
  { US "\033o-", 0x014d },
  { US "\033OE", 0x0152 },
  { US "\033eo", 0x0153 },
  { US "\033R'", 0x0154 },
  { US "\033r'", 0x0155 },
  { US "\033Rv", 0x0158 },
  { US "\033rv", 0x0159 },
  { US "\033S'", 0x015a },
  { US "\033s'", 0x015b },
  { US "\033Sv", 0x0160 },
  { US "\033sv", 0x0161 },
  { US "\033U-", 0x016a },
  { US "\033u-", 0x016b },
  { US "\033Uo", 0x016e },
  { US "\033uo", 0x016f },
  { US "\033Y.", 0x0178 },
  { US "\033Z'", 0x0179 },
  { US "\033z'", 0x017a },
  { US "\033Z.", 0x017b },
  { US "\033z.", 0x017c },
  { US "\033Zv", 0x017d },
  { US "\033zv", 0x017e },

  { US "\033$",  0x20ac },
  { NULL, 0 },
};



/*************************************************
*     Read a termcap or terminfo string entry    *
*************************************************/

/* The argument is a termcap or terminfo identification string; the yield is
the corresponding escape sequence or NULL. */

static uschar *
my_tgetstr(uschar *key)
{
#ifdef HAVE_TERMCAP
uschar *yield, *p;
uschar workbuff[256];
p = workbuff;
if (tgetstr(key, (char **)(&p)) == 0 || workbuff[0] == 0) return NULL;
yield = (uschar *)store_Xget(Ustrlen(workbuff)+1);
Ustrcpy(yield, workbuff);
return yield;

#else
uschar *yield = (uschar *)tigetstr(CS key);
if (yield == NULL || yield == (uschar *)(-1) || yield[0] == 0) return NULL;
return yield;
#endif
}



/*************************************************
*      Add an escape key sequence to the list    *
*************************************************/

/* If the static variable use_utf8 is true, and the value is greater than 160,
we have a data character than must be expressed as two or more UTF-8 bytes.
This is a fudge, of course. I wouldn't do it this way from scratch. There are
some single-byte values greater than 160 (the function keys), which is why we
have to do it this way.

Arguments:
  s         the key sequence
  keyvalue  the value of the sequence
  acount    points to count of such sequences
  aptr      points to the pointer in the constructed table

Returns:    nothing
*/

static void
addkeystr(uschar *s, int keyvalue, int *acount, uschar **aptr)
{
uschar *ptr = *aptr;
int len = Ustrlen(s);

/* If the string consists of just one byte, we can put the key value directly
into the trigger table (the values are always < 256). */

if (len == 1)
  {
  tc_k_trigger[s[0]] = keyvalue;
  return;
  }

/* Otherwise, set the trigger value to 254 and update the data table. */

tc_k_trigger[s[0]] = 254;
*acount += 1;
Ustrcpy((uschar *)(ptr + 1), s);

/* Test for a special UTF-8 sequence. */

if (use_utf8 && keyvalue >= 160)
  {
  uschar buff[8];
  int ulen = ord2utf8(keyvalue, buff);
  *ptr = len + 2 + ulen;
  Ustrncpy(ptr + len + 2, buff, ulen);
  }

/* Not a special sequence */

else
  {
  *ptr = len + 3;
  ptr[len+2] = keyvalue;
  }

/* Update the current pointer in the list */

*aptr = ptr + *ptr;
}



/*************************************************
*      Add a list of escape key sequences        *
*************************************************/

/* The list is a vector of esc_item structures, terminated by one whose string
pointer is NULL.

Arguments:
  p         points to first structure in the list
  acount    points to count of escape key sequences
  aptr      points to the pointer in the constructed table

Returns:    nothing
*/

static void
addkeystr_list(esc_item *p, int *acount, uschar **aptr)
{
for (; p->string != NULL; p++) addkeystr(p->string, p->value, acount, aptr);
}



/*************************************************
* Read a termcap/info key entry and set up data  *
*************************************************/

/* This function adds an escape sequence from termcap/info to the list of
escape sequences.

Arguments
  s         name of termcap/info item
  acount    points to count of valid escape sequences
  aptr      points to the pointer in the constructed table
  keyvalue  Pkey value generated by this sequence

Returns:    TRUE if entry found and added; FALSE if not
*/

static BOOL
tgetkeystr(uschar *s, int *acount, uschar **aptr, int keyvalue)
{
#ifdef HAVE_TERMCAP
uschar workbuff[256];
uschar *p = workbuff;
if (tgetstr(s, (char **)(&p)) == 0 || workbuff[0] == 0) return FALSE;
#else
uschar *workbuff = (uschar *)tigetstr(CS s);
if (workbuff == NULL || workbuff == (uschar *)(-1) || workbuff[0] == 0)
  return FALSE;
#endif
addkeystr(workbuff, keyvalue, acount, aptr);
return TRUE;
}



/*************************************************
*            Check terminal type                 *
*************************************************/

/* Search the termcap or terminfo database to determine the capability of the
terminal. Nowadays only two kinds of terminal are recognized.

Arguments:  none

Returns:    term_screen: terminal can do screen editing
            term_other:  terminal can't do screen editing
*/

static int
CheckTerminal(void)
{
uschar *p;
uschar *keyptr;
int erret;
int keycount = 0;
struct winsize parm;

/* Set up a file descriptor to the terminal for use in various ioctl calls. */

ioctl_fd = open("/dev/tty", O_RDWR);

#ifdef HAVE_TERMCAP
if (termcap_buf == NULL) termcap_buf = (uschar *)store_Xget(1024);
if (tgetent(termcap_buf, term_name) != 1) return term_other;
#else  /* terminfo */
if (setupterm(CS term_name, ioctl_fd, &erret) != OK || erret != 1)
  return term_other;
#endif

/* First, investigate terminal size. See if we can get the values from an ioctl
call. If not, we take them from termcap/info. */

tc_n_li = 0;
tc_n_co = 0;

if (ioctl(ioctl_fd, TIOCGWINSZ, &parm) == 0)
  {
  if (parm.ws_row != 0) tc_n_li = parm.ws_row;
  if (parm.ws_col != 0) tc_n_co = parm.ws_col;
  }

#ifdef HAVE_TERMCAP
if (tc_n_li == 0) tc_n_li = tgetnum("li");  /* number of lines on screen */
if (tc_n_co == 0) tc_n_co = tgetnum("co");  /* number of columns on screen */
#else
if (tc_n_li == 0) tc_n_li = tigetnum("lines"); /* number of lines on screen */
if (tc_n_co == 0) tc_n_co = tigetnum("cols");  /* number of columns on screen */
#endif

if (tc_n_li <= 0 || tc_n_co <= 0) return term_other;

/* Terminal must be capable of moving the cursor to arbitrary positions */

if ((p = tc_s_cm = my_tgetstr(US TCI_CM)) == 0) return term_other;

/* Have a look at the "cm" string. If it contains the characters "%." ("%c" for
TERMINFO) then cursor positions are required to be output in binary. Set the
flag to cause the positioning routines never to generate a move to row or
column zero, since binary zero is used to mark the end of the string. (Also
various bits of communications tend to eat binary zeros.) */

NoZero = FALSE;
while (*p)
  {
#ifdef HAVE_TERMCAP
  if (*p == '%' && p[1] == '.') NoZero = TRUE;
#else
  if (*p == '%' && p[1] == 'c') NoZero = TRUE;
#endif
  p++;
  }

/* If NoZero is set, we need cursor up and cursor left movements; they are not
otherwise used. If the "bc" element is not set, cursor left is backspace. */

if (NoZero)
  {
  if ((tc_s_up = my_tgetstr(US TCI_UP)) == 0) return term_other;
  tc_s_bc = my_tgetstr(US TCI_BC);
  }

/* Set the automatic margins flag */

#ifdef HAVE_TERMCAP
tc_f_am = tgetflag("am");
#else
tc_f_am = tigetflag("am");
#endif

/* Some facilities are optional - NE will use them if present }, but will use
alternatives if they are not. */

tc_s_al = my_tgetstr(US TCI_AL);  /* add line */
tc_s_ce = my_tgetstr(US TCI_CD);  /* clear EOL */
tc_s_cl = my_tgetstr(US TCI_CL);  /* clear screen */
tc_s_cs = my_tgetstr(US TCI_CS);  /* set scroll region */
tc_s_dl = my_tgetstr(US TCI_DL);  /* delete line */
tc_s_ip = my_tgetstr(US TCI_IP);  /* insert padding */
tc_s_ke = my_tgetstr(US TCI_KE);  /* unsetup keypad */
tc_s_ks = my_tgetstr(US TCI_KS);  /* setup keypad */
tc_s_pc = my_tgetstr(US TCI_PC);  /* pad char */
tc_s_se = my_tgetstr(US TCI_SE);  /* end standout */
tc_s_sf = my_tgetstr(US TCI_SF);  /* scroll up */
tc_s_so = my_tgetstr(US TCI_SO);  /* start standout */
tc_s_sr = my_tgetstr(US TCI_SR);  /* scroll down */
tc_s_te = my_tgetstr(US TCI_TE);  /* end screen management */
tc_s_ti = my_tgetstr(US TCI_TI);  /* init screen management */

/* NE requires either "set scroll region" with "scroll down", or "delete line"
with "add line" in order to implement scrolling. */

if ((tc_s_cs == NULL || tc_s_sr == NULL) &&
  (tc_s_dl == NULL || tc_s_al == NULL))
    return term_other;

/* If no highlighting, set to null strings */

if (tc_s_se == NULL || tc_s_so == NULL) tc_s_se = tc_s_so = US"";

/* NE is prepared to use "insert char" provided it does not have to switch into
an "insert mode", and similarly for "delete char". */

if ((tc_s_ic = my_tgetstr(US TCI_IC)) != NULL)
  {
  uschar *tv = my_tgetstr(US TCI_IM);
  if (tv != NULL && *tv != 0) tc_s_ic = NULL;
#ifdef HAVE_TERMCAP
  if (tv != NULL) store_free(tv);
#endif
  }

if ((tc_s_dc = my_tgetstr(US TCI_DC)) != NULL)
  {
  uschar *tv = my_tgetstr(US TCI_DM);
  if (tv != NULL && *tv != 0) tc_s_dc = NULL;
#ifdef HAVE_TERMCAP
  if (tv != NULL) store_free(tv);
#endif
  }

/* Now we must scan for the strings sent by special keys and construct a data
structure for sunix to scan. Only the cursor keys are mandatory. Key values
greater 127 are always data. */

tc_k_strings = (uschar *)store_Xget(tc_keylistsize);
keyptr = tc_k_strings + 1;

tc_k_trigger = (uschar *)store_Xget(128);
memset((void *)tc_k_trigger, 255, 128);  /* all unset */

use_utf8 = FALSE;

/* Mandatory keys */

if (!tgetkeystr(US TCI_KU, &keycount, &keyptr, Pkey_up)) return term_other;
if (!tgetkeystr(US TCI_KD, &keycount, &keyptr, Pkey_down)) return term_other;
if (!tgetkeystr(US TCI_KL, &keycount, &keyptr, Pkey_left)) return term_other;
if (!tgetkeystr(US TCI_KR, &keycount, &keyptr, Pkey_right)) return term_other;

/* Optional keys */

(void)tgetkeystr(US TCI_KDC, &keycount, &keyptr, Pkey_del127);
(void)tgetkeystr(US TCI_KSDC, &keycount, &keyptr, Pkey_sh_del127);

(void)tgetkeystr(US TCI_KHOME, &keycount, &keyptr, Pkey_ct_up);
(void)tgetkeystr(US TCI_KEND, &keycount, &keyptr, Pkey_ct_down);
(void)tgetkeystr(US TCI_KPPAGE, &keycount, &keyptr, Pkey_sh_up);
(void)tgetkeystr(US TCI_KNPAGE, &keycount, &keyptr, Pkey_sh_down);
(void)tgetkeystr(US TCI_KINSRT, &keycount, &keyptr, Pkey_insert);

(void)tgetkeystr(US TCI_K0, &keycount, &keyptr, Pkey_f0);
(void)tgetkeystr(US TCI_K1, &keycount, &keyptr, Pkey_f0+1);
(void)tgetkeystr(US TCI_K2, &keycount, &keyptr, Pkey_f0+2);
(void)tgetkeystr(US TCI_K3, &keycount, &keyptr, Pkey_f0+3);
(void)tgetkeystr(US TCI_K4, &keycount, &keyptr, Pkey_f0+4);
(void)tgetkeystr(US TCI_K5, &keycount, &keyptr, Pkey_f0+5);
(void)tgetkeystr(US TCI_K6, &keycount, &keyptr, Pkey_f0+6);
(void)tgetkeystr(US TCI_K7, &keycount, &keyptr, Pkey_f0+7);
(void)tgetkeystr(US TCI_K8, &keycount, &keyptr, Pkey_f0+8);
(void)tgetkeystr(US TCI_K9, &keycount, &keyptr, Pkey_f0+9);
(void)tgetkeystr(US TCI_KK, &keycount, &keyptr, Pkey_f0+10);
(void)tgetkeystr(US TCI_F1, &keycount, &keyptr, Pkey_f0+11);
(void)tgetkeystr(US TCI_F2, &keycount, &keyptr, Pkey_f0+12);
(void)tgetkeystr(US TCI_F3, &keycount, &keyptr, Pkey_f0+13);
(void)tgetkeystr(US TCI_F4, &keycount, &keyptr, Pkey_f0+14);
(void)tgetkeystr(US TCI_F5, &keycount, &keyptr, Pkey_f0+15);
(void)tgetkeystr(US TCI_F6, &keycount, &keyptr, Pkey_f0+16);
(void)tgetkeystr(US TCI_F7, &keycount, &keyptr, Pkey_f0+17);
(void)tgetkeystr(US TCI_F8, &keycount, &keyptr, Pkey_f0+18);
(void)tgetkeystr(US TCI_F9, &keycount, &keyptr, Pkey_f0+19);
(void)tgetkeystr(US TCI_FA, &keycount, &keyptr, Pkey_f0+20);
(void)tgetkeystr(US TCI_FB, &keycount, &keyptr, Pkey_f0+21);
(void)tgetkeystr(US TCI_FC, &keycount, &keyptr, Pkey_f0+22);
(void)tgetkeystr(US TCI_FD, &keycount, &keyptr, Pkey_f0+23);
(void)tgetkeystr(US TCI_FE, &keycount, &keyptr, Pkey_f0+24);
(void)tgetkeystr(US TCI_FF, &keycount, &keyptr, Pkey_f0+25);
(void)tgetkeystr(US TCI_FG, &keycount, &keyptr, Pkey_f0+26);
(void)tgetkeystr(US TCI_FH, &keycount, &keyptr, Pkey_f0+27);
(void)tgetkeystr(US TCI_FI, &keycount, &keyptr, Pkey_f0+28);
(void)tgetkeystr(US TCI_FJ, &keycount, &keyptr, Pkey_f0+29);
(void)tgetkeystr(US TCI_FK, &keycount, &keyptr, Pkey_f0+30);

/* Some terminals have more facilities than can be described by termcap/info.
Knowledge of some of them is screwed in to this code. If termcap/info ever
expands, this can be generalized. There was a lot of history here, but I've now
cut out all the specials except xterm. */

tt_special = tt_special_none;
if (Ustrncmp(term_name, "xterm",5) == 0)
  {
  tt_special = tt_special_xterm;
  if (tc_s_ti != NULL) main_nlexit = FALSE;  /* No NL needed if scrn managed */
  addkeystr_list(xterm_escapes, &keycount, &keyptr);
  }

/* There are certain escape sequences that are built into NE. We put them last
so that they are only matched if those obtained from termcap/terminfo do not
contain the same sequences. */

addkeystr_list(ne_escapes, &keycount, &keyptr);

/* Finally, the wide characters that are recognized by escape sequences. These
must be added as multibyte UTF-8 strings, so we set use_utf8 to ensure that
happens. */

use_utf8 = TRUE;
addkeystr_list(wide_escapes, &keycount, &keyptr);

/* Set the count of strings in the first byte */

tc_k_strings[0] = keycount;

/* Remove the default actions for various shift+ctrl keys that are not
settable. This will prevent them from being displayed. */

if (tt_special == tt_special_xterm)
  {
  keycount = 0;
  while (xterm_non_keys[keycount] != 0)
    key_table[xterm_non_keys[keycount++]] = 0;
  }
else
  {
  keycount = 0;
  while (non_keys[keycount] != 0) key_table[non_keys[keycount++]] = 0;
  }

/* Yield screen editing terminal type */

return term_screen;
}



/*************************************************
*         Additional special key text            *
*************************************************/

/* This function is called in screen mode after outputting info about special
keys, to allow any system-specific comments to be included. The second argument
supplies a function that checks for space on the screen and, if there isn't
enough, pauses with "Press RETURN to continue".

Arguments:
  acount           points to current number of displayed lines
  check_function   screen space check function

Returns:           nothing
*/

void
sys_specialnotes(usint *acount, void(*check_function)(usint, usint *))
{
check_function(8, acount);
*acount += 7;
error_printf("\n");
error_printf("home         synonym for ctrl/up       end            synonym for ctrl/down\n");
error_printf("page up      synonym for shift/up      page down      synonym for shift/down\n");
error_printf("esc-q        synonym for ctrl/q (XON)  esc-s          synonym for ctrl/s (XOFF)\n");
error_printf("esc-digit    functions 1-10            esc-esc-digit  functions 11-20\n");
error_printf("esc-return   re-display screen         esc-tab        backwards tab\n");
error_printf("esc-esc-char control char as data\n");
}



/* LCOV_EXCL_START */
/*************************************************
*          Handle window size change             *
*************************************************/

static void
sigwinch_handler(int sig)
{
(void)sig;
window_changed = TRUE;
signal(SIGWINCH, sigwinch_handler);
}
/* LCOV_EXCL_STOP */



/* LCOV_EXCL_START */
/*************************************************
*          Handle (ignore) SIGHUP                *
*************************************************/

/* SIGHUP's are usually ignored, as otherwise there are problems with running
NE in a dedicated xterm window. Ignored in the sense of not trying to put out
fancy messages, that is.

December 1997: Not quite sure what the above comment is getting at, but
modified NE so as to do the dumping thing without trying to write anything to
the terminal. Otherwise it was getting stuck if a connection hung up. */

static void
sighup_handler(int sig)
{
(void)sig;
crash_handler_chatty = FALSE;
crash_handler(sig);
}
/* LCOV_EXCL_STOP */



/*************************************************
*              Local initialization              *
*************************************************/

/* This is called first thing in main() for doing vital system-specific early
things. */

void
sys_init1(void)
{
uschar *tabs = US getenv("NETABS");
signal(SIGHUP, sighup_handler);
if (tabs != NULL && tabs[0] != 0) main_tabs = tabs;
}


/* This is called after argument decoding is complete to allow any system-
specific overriding to take place. Main_screenmode will be TRUE unless -line
or -with was specified. The argument points to a buffer into which the name of
an initialization file is placed. */

void
sys_init2(uschar *init_file_buffer)
{
int i;
uschar *nercname;
uschar *filechars = US"+-*/,.:!?";       /* leave only " and ' */
struct stat statbuf;

term_name = US getenv("TERM");

/* Set up terminal type and terminal-specific things */

if (!main_screenmode) term_type = term_other; else
  {
  term_type = CheckTerminal();
  switch (term_type)
    {
    case term_screen:
    screen_max_row = tc_n_li - 1;
    screen_max_col = tc_n_co - 1;
    scommon_select();                    /* connect the common screen driver */
    signal(SIGWINCH, sigwinch_handler);
    break;

    default:
    printf("This terminal (%s) cannot support screen editing in NE;\n",
      term_name);
    printf("therefore entering line mode:\n\n");
    main_screenmode = main_screenOK = FALSE;
    break;
    }
  }

/* Look for the the name of a user-specific initialization file, defaulting to
~/.nerc. Put the name into the supplied buffer, but point main_einit at it only
if the file exists. */

nercname = US getenv("NERC");
if (nercname == NULL)
  {
  Ustrcpy(init_file_buffer, getenv("HOME"));
  Ustrcat(init_file_buffer, "/.nerc");
  }
else Ustrcpy(init_file_buffer, nercname);

if (Ustat(init_file_buffer, &statbuf) == 0) main_einit = init_file_buffer;

/* Remove legal file characters from file delimiters list */

for (i = 0; i < (int)Ustrlen(filechars); i++)
  ch_tab[filechars[i]] &= ~ch_filedelim;
}



/*************************************************
*           Tidy up at exit time                 *
*************************************************/

void
sys_tidy_up(void)
{
if (!main_screenmode) return;
#ifndef HAVE_TERMCAP
del_curterm(cur_term);
#endif
}



/*************************************************
*                Munge return code               *
*************************************************/

/* This function is called just before exiting, to enable a return code to be
set according to the OS's conventions. */

int
sys_rc(int rc)
{
return rc;
}



/* LCOV_EXCL_START - next few functions are not triggered in tests */


/*************************************************
*                   Beep                         *
*************************************************/

/* Called only when interactive. Makes the terminal beep.

Arguments:   none
Returns:     nothing
*/

void
sys_beep(void)
{
uschar buff[1];
int tty = open("/dev/tty", O_WRONLY);
buff[0] = 7;
if (write(tty, buff, 1)){};  /* Fudge; avoid warning */
close(tty);
}



/*************************************************
*        Decode ~ at start of file name          *
*************************************************/

/* Called both when opening a file, and when completing a file name.

Arguments:
  name         the current name
  len          length of same
  buff         a buffer in which to return the expansion

Returns:       nothing
*/

static void
sort_twiddle(uschar *name, int len, uschar *buff)
{
int i;
uschar logname[20];
struct passwd *pw;

/* For ~/thing, convert by reading the HOME variable */

if (name[1] == '/')
  {
  Ustrcpy(buff, getenv("HOME"));
  Ustrncat(buff, name+1, len - 1);
  return;
  }

/* Otherwise we must get the home directory from the password file. */

for (i = 2;; i++) if (i >= len || name[i] == '/') break;
Ustrncpy(logname, name+1, i-1);
logname[i-1] = 0;
pw = getpwnam(CS logname);
if (pw == NULL) Ustrncpy(buff, name, len); else
  {
  Ustrcpy(buff, pw->pw_dir);
  Ustrncat(buff, name + i, len - i);
  }
return;
}



/*************************************************
*            Complete a file name                *
*************************************************/

/* This function is called when TAB is pressed in a command line while
interactively screen editing. It tries to complete the file name.

Arguments:
  p             the current offset in the command buffer
  pmaxptr       pointer to the current high-water mark in the command buffer

Returns:        possibly updated value of p
                pmax may be updated via the pointer
*/

int
sys_fcomplete(int p, int *pmaxptr)
{
int pb = p - 1;
int pe = p;
int pmax = *pmaxptr;
int len, endlen;
uschar buffer[256];
uschar insert[256];
uschar leafname[64];
BOOL insert_found = FALSE;
BOOL beep = TRUE;
uschar *s;
DIR *dir;
struct dirent *dent;

if (p < 1 || cmd_buffer[pb] == ' ' || p > pmax) goto RETURN;
while (pb > 0 && cmd_buffer[pb - 1] != ' ') pb--;

/* One day we may implement completing in the middle of names, but for the
moment, this hack up just completes ends. */

/* while (pe < pmax && cmd_buffer[pe] != ' ') pe++; */
if (pe < pmax && cmd_buffer[pe] != ' ') goto RETURN;

len = pe - pb;

if (cmd_buffer[pb] == '~') sort_twiddle(cmd_buffer + pb, len, buffer);
else if (cmd_buffer[pb] != '/')
  {
  Ustrcpy(buffer, "./");
  Ustrncat(buffer, cmd_buffer + pb, len);
  }
else
  {
  Ustrncpy(buffer, cmd_buffer + pb, len);
  buffer[len] = 0;
  }

len = Ustrlen(buffer);

/* There must be at least one '/' in the string, because of the checking done
above. */

s = buffer + len - 1;
while (s > buffer && *s != '/') s--;

*s = 0;
endlen = len - (s - buffer) - 1;
dir = opendir(CS buffer);
if (dir == NULL) goto RETURN;

while ((dent = readdir(dir)) != NULL)
  {
  if (Ustrcmp(dent->d_name, ".") == 0 || Ustrcmp(dent->d_name, "..") == 0)
    continue;
  if (Ustrncmp(dent->d_name, s + 1, endlen) == 0)
    {
    if (!insert_found)
      {
      Ustrcpy(insert, dent->d_name + endlen);
      Ustrcpy(leafname, dent->d_name);
      insert_found = TRUE;
      beep = FALSE;
      }
    else
      {
      usint i;
      beep = TRUE;
      for (i = 0; i < Ustrlen(insert); i++)
        {
        if (insert[i] != dent->d_name[endlen + i]) break;
        }
      insert[i] = 0;
      }
    }
  }
closedir(dir);

if (insert_found && insert[0] != 0)
  {
  int inslen;

  if (!beep)
    {
    struct stat statbuf;
    Ustrcat(buffer, "/");
    Ustrcat(buffer, leafname);
    if (Ustat(buffer, &statbuf) == 0 && S_ISDIR(statbuf.st_mode))
      Ustrcat(insert, "/");
     }

  inslen = Ustrlen(insert);
  memmove(cmd_buffer + p + inslen, cmd_buffer + p, Ustrlen(cmd_buffer + p));
  memcpy(cmd_buffer + p, insert, inslen);
  *pmaxptr = pmax + inslen;
  p += inslen;
  }

RETURN:
if (beep) sys_beep();
return p;
}



/*************************************************
*             Generate crash file name           *
*************************************************/

/*
Argument:   TRUE for crash buffer data, FALSE for the log
Returns:    pointer to the name
*/

uschar *
sys_crashfilename(int which)
{
return which? US"NEcrash" : US"NEcrashlog";
}
/*LCOV_EXCL_STOP */



/*************************************************
*              Open a file                       *
*************************************************/

/* When opening an output file, automatic backing up is supported.

Arguments:
  name       file name - may begin with ~
  type       type for fopen()

Returns:     an open FILE or NULL
*/

FILE *
sys_fopen(uschar *name, uschar *type)
{
uschar buff[256];

if (name[0] == '~')
  {
  sort_twiddle(name, Ustrlen(name), buff);
  name = buff;
  }

/* Handle optional automatic backup for output files. We add "~" to the name,
as is common on Unix. */

if (main_backupfiles && Ustrcmp(type, "w") == 0 && !file_written(name))
  {
  uschar bakname[80];
  Ustrcpy(bakname, name);
  Ustrcat(bakname, "~");
  remove(CS bakname);
  rename(CS name, CS bakname);
  file_setwritten(name);
  }

return Ufopen(name, type);
}



/*************************************************
*              Check file name                   *
*************************************************/

/* This tests for validity. NE doesn't handle file names that contain spaces or
non-printing or non-ASCII characters. Trailing spaces, however, are ignored.

Argument:    potential file name
Returns:     NULL if OK, otherwise an error reason
*/

uschar *
sys_checkfilename(uschar *s)
{
for (uschar *p = s; *p != 0; p++)
  {
  if (*p == ' ')
    {
    while (*(++p) != 0)
      {
      if (*p != ' ') return US"(contains a space)";
      }
    break;
    }
  if (*p < ' ' || *p > '~') return US"(contains a non-printing character)";
  }
return NULL;
}



/*************************************************
*            Write to message stream             *
*************************************************/

/* Output to msgs_fid is passed through here, so that it can be handled
system-specifically if necessary. */

void
sys_mprintf(FILE *f, const char *format, ...)
{
va_list ap;
va_start(ap, format);
vfprintf(f, format, ap);
va_end(ap);
}



/*************************************************
*            Give reason for key refusal         *
*************************************************/

/* This function gives reasons for the setting of bits in the maps that forbid
certain keys being set. */

uschar *
sys_keyreason(int key)
{
switch (key & ~(s_f_shiftbit+s_f_ctrlbit))
  {
  case s_f_bsp: return US"\n   (\"backspace\" is the same as ctrl/h)";
  case s_f_ret: return US"\n   (\"return\" is the same as ctrl/m)";
  case s_f_tab: return US"\n   (\"tab\" is the same as ctrl/i)"; 
  case s_f_hom: return US"\n   (\"home\" is the same as ctrl/up)"; 
  case s_f_end: return US"\n   (\"end\" is the same as ctrl/down)"; 
  case s_f_pup: return US"\n   (\"page up\" is the same as shift/up)"; 
  case s_f_pdn: return US"\n   (\"page down\" is the same as shift/down)"; 
  default:      return US"";
  }
}



/*************************************************
*           System-specific interrupt check      *
*************************************************/

/* This is called from main_interrupted(). This in turn is called only when the
main part of NE is in control, not during screen editing operations. We have to
check for ^C by steam, as we are running in raw terminal mode.

It turns out that the ioctl can be quite slow, so we don't actually want to do
it for every line in, e.g., an m command. NE now maintains a count of calls to
main_interrupted(), reset when a command line is read, and it also passes over
the kind of processing that is taking place:

  ci_move     m, f, bf, or g command
  ci_type     t or tl command
  ci_read     reading command line or data for i command
  ci_cmd      about to obey a command
  ci_delete   deleting all lines of a buffer
  ci_scan     scanning lines (e.g. for show wordcount)
  ci_loop     about to obey the body of a loop

These are successive integer values, starting from zero. */

/* This vector of masks is used to mask the counter; if the result is
zero, the ioctl is done. Thereby we have different intervals for different
types of activity with very little cost. The numbers are pulled out of the
air... */

static int ci_masks[] = {
    1023,    /* ci_move - every 1024 lines*/
    0,       /* ci_type - every time */
    0,       /* ci_read - every time */
    15,      /* ci_cmd  - every 16 commands */
    127,     /* ci_delete - every 128 lines */
    1023,    /* ci_scan - every 1024 lines */
    15       /* ci_loop - every 16 commands */
};

void
sys_checkinterrupt(int type)
{
if (main_screenOK && !main_escape_pressed &&
    (main_cicount & ci_masks[type]) == 0)
  {
  int c = 0;
  ioctl(ioctl_fd, FIONREAD, &c);
  while (c-- > 0)
    {
    if (getchar() == tc_int_ch) main_escape_pressed = TRUE;  /* LCOV_EXCL_LINE */
    }
  }
}

/* End of sysunix.c */
