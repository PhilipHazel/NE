/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2024 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: September 2024 */


/* This file contains screen-handling code, originally for use with termcap
functions under Unix, then extended to use terminfo as an alternative, now
the default. Termcap can be selected by configuration option. There is special
knowledge about xterm in this code. */


#include "ehdr.h"
#include "keyhdr.h"
#include "shdr.h"
#include "scomhdr.h"
#include "unixhdr.h"

#define sh  s_f_shiftbit
#define ct  s_f_ctrlbit
#define shc (s_f_shiftbit + s_f_ctrlbit)


#ifndef HAVE_TERMCAP
#define tgoto(a, b, c) tparm(CS a, c, b)
#endif



/*************************************************
*            Main terminal control data          *
*************************************************/

/* These are global values, shared with sysunix.c */

int tc_n_co;          /* columns on screen */
int tc_n_li;          /* lines on screen */

uschar *tc_s_al;      /* add new line */
uschar *tc_s_bc;      /* cursor left - used only if NoZero */
uschar *tc_s_ce;      /* clear to end of line */
uschar *tc_s_cl;      /* clear screen */
uschar *tc_s_cm;      /* cursor move */
uschar *tc_s_cs;      /* set scrolling region */
uschar *tc_s_dc;      /* delete uschar */
uschar *tc_s_dl;      /* delete line */
uschar *tc_s_ic;      /* insert character */
uschar *tc_s_ip;      /* insert padding */
uschar *tc_s_ke;      /* reset terminal */
uschar *tc_s_ks;      /* set up terminal */
uschar *tc_s_pc;      /* pad character */
uschar *tc_s_se;      /* end standout */
uschar *tc_s_sf;      /* scroll text up */
uschar *tc_s_so;      /* start standout */
uschar *tc_s_sr;      /* scroll text down */
uschar *tc_s_te;      /* end screen management */
uschar *tc_s_ti;      /* start screen management */
uschar *tc_s_up;      /* cursor up - used only if NoZero */

int tc_f_am;          /* automatic margin flag */

uschar *tc_k_trigger; /* trigger table for special keys */
uschar *tc_k_strings; /* strings for keys 0-n and specials */

int tt_special;       /* terminal special types */
int tc_int_ch;        /* interrupt char */
int NoZero;           /* never generate zero row or col */
int ioctl_fd;         /* FD to use for ioctl calls */
int window_changed;   /* SIGWINSZ received */



/*************************************************
*             Static Data                        *
*************************************************/

static int sunix_setrendition = s_r_normal;

/* Buffer for key input bytes that looked like the start of an escape sequence,
but weren't. */

static uschar kbback[20];
static int kbbackptr;

/* It turns out to be much faster to buffer up characters for output, at least
under some systems and some libraries. SunOS 4.1.3 with the GNU compiler was a
case in point. This may no longer be relevant. */

#define outbuffsize 4096

static uschar out_buffer[outbuffsize];
static int outbuffptr = 0;

static struct termios oldtermparm;

/* This table translates from Pkey special key values to logical keystrokes. */

static uschar Pkeytable[] = {
  s_f_del,                /* Table starts at char 127 */
  s_f_cup,
  s_f_cdn,
  s_f_clf,
  s_f_crt,
  s_f_cup+s_f_shiftbit,
  s_f_cdn+s_f_shiftbit,
  s_f_clf+s_f_shiftbit,
  s_f_crt+s_f_shiftbit,
  s_f_cup+s_f_ctrlbit,
  s_f_cdn+s_f_ctrlbit,
  s_f_clf+s_f_ctrlbit,
  s_f_crt+s_f_ctrlbit,
  s_f_reshow,
  s_f_del,
  s_f_del+s_f_shiftbit,
  s_f_del+s_f_ctrlbit,
  s_f_bsp,
  s_f_bsp+s_f_shiftbit,
  s_f_bsp+s_f_ctrlbit,
  s_f_hom,
  s_f_tab+s_f_ctrlbit,    /* use ctrl+tab for back tab */
  s_f_tab+s_f_ctrlbit,    /* true ctrl+tab */
  s_f_ins,
  s_f_ignore,             /* ) These special keys should never get through */
  s_f_ignore,             /* ) to this level because they are handled */
  s_f_ignore,             /* ) at a deeper level. */
  s_f_xy,                 /* Pkey_xy - mouse click */
  s_f_mscr_down,          /* Pkey_mscr_down - mouse scroll down */
  s_f_mscr_up             /* Pkey_mscr_up - mouse scroll up */
  };



/*************************************************
*           Flush buffered output                *
*************************************************/

/* Make sure all output has been delivered */

static void
sunix_flush(void)
{
if (outbuffptr > 0)
  {
  if (write(ioctl_fd, out_buffer, outbuffptr)){};  /* Fudge avoid warning */
  outbuffptr = 0;
  }
}



/*************************************************
*          Get next raw keystroke                *
*************************************************/

/* This function normally reads a keystroke from stdin, but it can be subverted
to read from a file of simulated keystroke codes for testing or debugging. */

static BOOL withkey_literal = TRUE;
static BOOL withkey_dosleep = FALSE;
static uschar kseq[10];
static int kseqptr = -1;
static int kseqrep = 0;

/* Structure for list of keynames used for simulated keystrokes. Note that
there isn't a Pkey value for tab or shift/tab at present. */

typedef struct knstr {
  uschar *name;
  int value[3];  /* Plain, shift, ctrl */
} knstr;

static knstr knames[] = {
  { US"up",        { Pkey_up,     Pkey_sh_up,     Pkey_ct_up }},
  { US"down",      { Pkey_down,   Pkey_sh_down,   Pkey_ct_down }},
  { US"left",      { Pkey_left,   Pkey_sh_left,   Pkey_ct_left }},
  { US"right",     { Pkey_right,  Pkey_sh_right,  Pkey_ct_right }},
  { US"delete",    { Pkey_del127, Pkey_sh_del127, Pkey_ct_del127 }},
  { US"backspace", { Pkey_bsp,    Pkey_sh_bsp,    Pkey_ct_bsp }},
  { US"tab",       { Pkey_null,   Pkey_null,      Pkey_ct_tab }},

};

#define knames_count (int)(sizeof(knames)/sizeof(knstr))


/* Local subroutine for finding the byte sequence for a given named keystroke
or function key. */

static int
findkeystring(int keycode, int repeat, uschar *s)
{
uschar *p = tc_k_strings + 1;
for (int i = 0; i < tc_k_strings[0]; i++)
  {
  if (p[*p - 1] == keycode)
    {
    kseqrep = repeat - 1;
    Ustrcpy(kseq, p + 1);
    kseqptr = 1;
    return kseq[0];
    }
  p += *p;
  }

/* Failed to find keystroke - disastrous error. */

/* LCOV_EXCL_START */
fclose(withkey_fid);
error_moan(74, s, keycode);  /* Hard */
return 0;                    /* Keep compiler happy */
}
/* LCOV_EXCL_STOP */


/* The actual getchar function */

static int
sunix_getchar(void)
{
int c;
BOOL withkey_slept;

/* Return a real keystroke */

if (withkey_fid == NULL) return getchar();

/* Pause after previous simulated keystroke if required. */

if (withkey_dosleep)
  {
  sleep(withkey_sleep);
  withkey_dosleep = FALSE;
  withkey_slept = TRUE;
  }
else withkey_slept = FALSE;

/* Handle buffered up sequence, possibly repeated. */

if (kseqptr >= 0)
  {
  c = kseq[kseqptr++];
  if (kseq[kseqptr] == 0)
    {
    if (kseqrep-- > 0)
      {
      kseqptr = 0;
      }
    else
      {
      kseqptr = -1;
      withkey_dosleep = TRUE;
      }
    }
  return c;
  }

/* Get next from file */

for (;;)
  {
  c = getc(withkey_fid);
  if (c == EOF) goto ENDFILE;

  if (withkey_literal && c != '\\')
    {
    if (c == '\n') c = '\r';   /* Convert newline in file to CR keystroke */
    return c;
    }

  /* We have a backslash or are already in the interpreting state. Ignore a
  newline, but reset to literal state. */

  if (c == '\n')
    {
    withkey_literal = TRUE;
    continue;
    }

  /* Ignore any other white space in a non-literal line, but treat any
  character other than backslash as literal. */

  withkey_literal = FALSE;
  if (isspace(c)) continue;  /* Ignore white space */
  if (c != '\\') return c;   /* Treat as literal; no sleep */

  /* Process escape sequence */

  c = getc(withkey_fid);
  if (c == EOF) goto ENDFILE;

  /* Ignore rest of line as comment */

  if (c == '*')
    {
    for (;;)
      {
      c = getc(withkey_fid);
      if (c == EOF) goto ENDFILE;
      if (c == '\n') break;
      }
    withkey_literal = TRUE;   /* Continue with next line */
    continue;
    }

  /* Pause if not already done so. */

  if (!withkey_slept) sleep(withkey_sleep);

  /* Handle hexadecimal byte value - lower case letters only */

  if (Ustrchr("abcdef0123456789", c) != NULL)
    {
    int d;
    c = (c > '9')? c - 'a' + 10 : c - '0';
    d = getc(withkey_fid);
    if (Ustrchr("abcdef0123456789", d) != NULL)
      {
      d = (d > '9')? d - 'a' + 10 : d - '0';
      c = (c << 4) | d;
      }
    else ungetc(d, withkey_fid);
    break;
    }

  /* Backslash with upper or @[\]^_ gives CTRL values 0-31 */

  else if (c >= '@' && c <= '_')
    {
    c &= ~0x40;
    break;
    }

  /* Handle function key */

  else if (c == '#')
    {
    int n = 0;
    for (;;)
      {
      c = getc(withkey_fid);
      if (isdigit(c)) n = n * 10 + c - '0';
        else { ungetc(c, withkey_fid); break; }
      }
    return findkeystring(Pkey_f0+n, 1, US"function key");
    }

  /* Handle named keystroke. Allow for repetition and Shift/Cntrl */

  else if (c == '=')
    {
    uschar ss[64];
    uschar *s = ss;
    uschar *p = ss;
    int keycode = -1;
    int sc = 0;
    int repeat = 0;

    for (;;)
      {
      c = getc(withkey_fid);
      if (c == EOF) break;
      if (c == '\\' || isspace(c)) { ungetc(c, withkey_fid); break; }
      *p++ = c;
      }
    *p = 0;

    if (isdigit(*s)) repeat = (int)strtol(CS s, CSS &s, 10);

    if (s[1] == '+')
      {
      sc = (*s == 's')? 1 : (*s == 'c')? 2 : 0;
      if (sc != 0) s += 2;
      }

    for (int i = 0; i < knames_count; i++)
      {
      if (Ustrcmp(s, knames[i].name) == 0)
        {
        keycode = knames[i].value[sc];
        break;
        }
      }

    if (keycode < 0)
      {
      /* LCOV_EXCL_START */
      scrn_suspend();
      error_moan(73, s);
      fclose(withkey_fid);
      exit(main_rc);
      /* LCOV_EXCL_STOP */
      }

    return findkeystring(keycode, repeat, s);
    }

  /* Treat anything else as literal */

  else break;
  }

withkey_dosleep = TRUE;
return c;

/* Reached the end of the simulated keystroke file. Go back to reading from
stdin. */

/* LCOV_EXCL_START */
ENDFILE:
fclose(withkey_fid);
withkey_fid = NULL;
return getchar();
/* LCOV_EXCL_STOP */
}



/*************************************************
*      Get keystroke and convert to standard     *
*************************************************/

/* This function has to take into account function key sequences and yield the
next logical keystroke. If it reads a number of bytes and then discovers that
they do not form an escape sequence, it returns the first of them, leaving the
remainder on a stack which is "read" before next doing a real read.

We should never get EOF from a raw mode terminal input stream, but its possible
to get NE confused into thinking it is interactive when it isn't. So code for
this case.

Argument:  where to return the keystroke type - data or function
Returns:   keystroke value
*/

static int
sunix_nextchar(int *type)
{
int scount, kbptr, c, k;
uschar *sp;
uschar kbbuff[20];

sunix_flush();       /* Deliver buffered output */
*type = ktype_data;  /* Default to data */

/* Get next key */

if (kbbackptr > 0) c = kbback[--kbbackptr]; else
  {
  c = sunix_getchar();
  if (c == EOF) return -1;
  }

/* Keys that have values > 127 are always treated as data; if the terminal is
configured for UTF-8 we have to do UTF-8 decoding. */

if (c > 127)
  {
  if (main_utf8terminal && c >= 0xc0)
    {
    uschar buff[8];
    buff[0] = c;
    for (int i = 1; i <= utf8_table4[c & 0x3f]; i++)
      buff[i] = (kbbackptr > 0)? kbback[--kbbackptr] : sunix_getchar();
    (void)utf82ord(buff, &c);
    }
  return c;
  }

/* Else look in the table of possible sequences */

k = tc_k_trigger[c];

/* An entry of 254 in the table indicates the start of a multi-byte escape
sequence; 255 means nothing special; other values are single byte control
sequences. */

if (k != 254)
  {
  if (k != 255) c = k;
  if (c < 32) *type = ktype_function;
  else if (c >= 127)
    {
    /* LCOV_EXCL_START - on an xterm these never occur */
    *type = ktype_function;
    c = (c >= Pkey_f0)? s_f_umax + k - Pkey_f0 : Pkeytable[k - 127];
    /* LCOV_EXCL_STOP */
    }
  return c;
  }

/* We are at the start of a possible multi-character sequence. */

sp = tc_k_strings + 1;
scount = tc_k_strings[0];
kbptr = 0;

/* Loop reading keystrokes and checking strings against those in the list. When
we find one that matches, if the data is more than one byte long, it is
interpreted as a UTF-8 data character. */

for (;;)
  {
  if (c == sp[1+kbptr])       /* Next byte matches */
    {
    if (sp[2 + kbptr] != 0)   /* Not reached the end, save and continue */
      {
      kbbuff[kbptr++] = c;
      c = (kbbackptr > 0)? kbback[--kbbackptr] : sunix_getchar();
      continue;
      }

    /* Matched an escape sequence. More than one value byte is always a UTF-8
    data character. */

    if (sp[0] > kbptr + 4)
      {
      (void)utf82ord(sp + 3 + kbptr, &c);
      return c;
      }

    /* Pick up the escaped value */

    c = sp[3 + kbptr];

    /* Handle "next key is literal" */

    if (c == Pkey_data)  /* Special case for literal */
      {
      c = (kbbackptr > 0)? kbback[--kbbackptr] : sunix_getchar();
      if (c < 127) c &= ~0x60;
      }

    /* Handle information about a mouse click. There follows three bytes,
    starting with an event indication, coded as a value + 32. A wheel mouse
    gives event 0x40 for "scroll up" and 0x41 for "scroll down". For these
    events, the other two bytes (typically zero) are not used. For the other
    events, the second and third bytes are the x,y coordinates, also coded as a
    value + 32. They are based at 1,1 which is why we have to subtract another
    1 from the column and the row. */

    else if (c == Pkey_xy)
      {
      int event = ((kbbackptr > 0)? kbback[--kbbackptr] : sunix_getchar()) - 32;

      /* Do not do the -33 subtraction here because mouse_col and mouse_row are 
      unsigned and the values may be zero for scroll operations. */ 
       
      mouse_col = (kbbackptr > 0)? kbback[--kbbackptr] : sunix_getchar();
      mouse_row = (kbbackptr > 0)? kbback[--kbbackptr] : sunix_getchar();

      /* Pay attention only to scroll and a button 1 press, which has event
      value 0. */

      *type = ktype_function;
      switch (event)
        {
        case 0x40: return Pkey_mscr_up;
        case 0x41: return Pkey_mscr_down;

        case 0:
        mouse_col -= 33;
        mouse_row -= 33;
        return Pkey_xy;

        default:
        return Pkey_null;
        }
      }

    /* Handle a Unicode code point specified by number. */

    else if (c == Pkey_utf8)
      {
      c = 0;
      for (int i = 0; i < 5; i++)
        {
        k = (kbbackptr > 0)? kbback[--kbbackptr] : sunix_getchar();
        if (!isxdigit(k))
          {
          if (k != 0x1b) kbback[kbbackptr++] = k;  /* Use if not ESC */
          break;
          }
        k = toupper(k);
        c = (c << 4) + (isalpha(k)? k - 'A' + 10 : k - '0');
        }
      }

    else *type = ktype_function;

    /* Always break out of the loop, returning c */

    return c;
    }

  /* Failed to match current escape sequence. Stack characters
  read so far and either advance to next sequence, or yield one
  character. */

  kbbuff[kbptr++] = c;
  while (kbptr > 1) kbback[kbbackptr++] = kbbuff[--kbptr];
  c = kbbuff[--kbptr];
  if (--scount > 0) sp += sp[0]; else break;
  }

return c;
}



/*************************************************
*       Get keystroke for command line           *
*************************************************/

/* This function is called when reading a command line in screen mode. We must
notice the user's interrupt key by hand. Luckily it is usually ^C, so unlikely
to be needed for editing a command line in the default key binding.

Argument:  where to return keystroke type - data or function
Returns:   keystroke value
*/

int
sys_cmdkeystroke(int *type)
{
int key = sunix_nextchar(type);
if (key == oldtermparm.c_cc[VINTR]) main_escape_pressed = TRUE;
if (*type == ktype_function && key >= 127)
  key = (key >= Pkey_f0)? s_f_umax + key - Pkey_f0 : Pkeytable[key - 127];
return key;
}



/*************************************************
*            Output termcap/info string          *
*************************************************/

/* General buffered output routine */

#ifndef MY_PUTC_ARG_TYPE
#define MY_PUTC_ARG_TYPE int
#endif

static int
my_putc(MY_PUTC_ARG_TYPE c)
{
if (outbuffptr > outbuffsize - 2)
  {
  /* LCOV_EXCL_START */
  if (write(ioctl_fd, out_buffer, outbuffptr)){};  /* Fudge avoid warning */
  outbuffptr = 0;
  /* LCOV_EXCL_STOP */
  }
out_buffer[outbuffptr++] = c;
return c;
}


#ifdef HAVE_TERMCAP
static void
outTCstring(uschar *s, int amount)
{
int pad = 0;
while ('0' <= *s && *s <= '9') pad = pad*10 + *s++ - '0';
pad *= 10;
if (*s == '.' && '0' <= *(++s) && *s <= '9') pad += *s++ - '0';
if (*s == '*') { s++; pad *= amount; }
while (*s) my_putc(*s++);
if (pad)
  {
  int pc = (tc_s_pc == NULL)? 0 : *tc_s_pc;
  if ((pad % 10) >= 5) pad += 10;
  pad /= 10;
  while (pad--) my_putc(pc);
  }
}

#else
#define outTCstring(s, amount) tputs(CCS (s), amount, my_putc)
#endif



/*************************************************
*         Interface routines to scommon          *
*************************************************/

static void
sunix_move(int x, int y)
{
if (!NoZero || (x > 0 && y > 0))
  {
  outTCstring(tgoto(CS tc_s_cm, x, y), 0);
  }
else
  {
  /* LCOV_EXCL_START */
  int left = (x == 0)? 1 : 0;
  int up =   (y == 0)? 1 : 0;
  outTCstring(tgoto(CS tc_s_cm, x+left, y+up), 0);
  if (up) outTCstring(tc_s_up, 0);
  if (left)
    {
    if (tc_s_bc == NULL) my_putc(8); else outTCstring(tc_s_bc, 0);
    }
  /* LCOV_EXCL_STOP */
  }
}


static void
sunix_rendition(int r)
{
if (r != sunix_setrendition)
  {
  sunix_setrendition = r;
  outTCstring((r == s_r_normal)? tc_s_se : tc_s_so, 0);
  }
}


static void
sunix_putc(int c)
{
if (c > 0xffff || (ch_displayable[c/8] & (1<<(c%8))) != 0) c = screen_subchar;
if (c < 128)
  {
  my_putc(c);
  return;
  }
if (main_utf8terminal)
  {
  int i;
  uschar buff[8];
  int len = ord2utf8(c, buff);
  for (i = 0; i < len; i++) my_putc(buff[i]);
  }
else my_putc((main_eightbit && c < 256)? c : '?');  /* LCOV_EXCL_LINE */
}


static void
sunix_cls(int bottom, int left, int top, int right)
{
if (bottom != (int)screen_max_row || left != 0 || top != 0 ||
    right != (int)screen_max_col || tc_s_cl == NULL)
  {
  for (int i = top; i <= bottom;  i++)
    {
    sunix_move(left, i);
    if (tc_s_ce != NULL) outTCstring(tc_s_ce, 0); else
      {
      /* LCOV_EXCL_START - never obeyed for xterm */
      sunix_rendition(s_r_normal);
      for (int j = left; j <= right; j++) sunix_putc(' ');
      /* LCOV_EXCL_STOP */
      }
    }
  }
else outTCstring(tc_s_cl, 0);
}


/* Either "set scrolling region" and "scroll reverse" will be available, or
"delete line" and "insert line" will be available. (It is possible that "set
scrolling region", "delete line" and "insert line" will be available without
"scroll reverse". We can handle this too.)

Experience shows that some terminals don't do what you expect if the region is
set to one line only; if these have delete/insert line, we can use that instead
(the known cases do). */

static void
sunix_vscroll(int bottom, int top, int amount)
{
if (amount > 0)
  {
  if (tc_s_cs != NULL && tc_s_sr != NULL)
    {
    outTCstring(tgoto(CS tc_s_cs, bottom, top), 0);
    sunix_move(0, top);
    for (int i = 0; i < amount; i++) outTCstring(tc_s_sr, 0);
    outTCstring(tgoto(CS tc_s_cs, screen_max_row, 0), 0);
    }
  /* LCOV_EXCL_START - not used for xterm */
  else for (int i = 0; i < amount; i++)
    {
    sunix_move(0, bottom);
    outTCstring(tc_s_dl, screen_max_row - bottom);
    sunix_move(0, top);
    outTCstring(tc_s_al, screen_max_row - top);
    }
  /* LCOV_EXCL_STOP */
  }

else
  {
  amount = -amount;
  if (tc_s_cs != NULL && (top != bottom || tc_s_dl == NULL))
    {
    outTCstring(tgoto(CS tc_s_cs, bottom, top), 0);
    sunix_move(0, bottom);
    for (int i = 0; i < amount; i++)
      if (tc_s_sf == NULL) my_putc('\n');
        else outTCstring(tc_s_sf, 0);
    outTCstring(tgoto(CS tc_s_cs, screen_max_row, 0), 0);
    }

  else for (int i = 0; i < amount; i++)
    {
    sunix_move(0, top);
    outTCstring(tc_s_dl, screen_max_row - top);
    if (bottom != (int)screen_max_row)
      {
      sunix_move(0, bottom);
      outTCstring(tc_s_al, screen_max_row - bottom);
      }
    }
  }
}



/*************************************************
*            Enable/disable mouse                *
*************************************************/

/* Supports only xterm terminals. */

void
sys_mouse(BOOL enable)
{
if (tt_special != tt_special_xterm) return;
if (enable && mouse_enable && write(ioctl_fd, "\x1b[?1000h", 8));
  else if (write(ioctl_fd, "\x1b[?1000l", 8)){};
}



/*************************************************
*                 Set terminal state             *
*************************************************/

/* The set state is raw mode (cbreak mode having been found impossible to work
with). Once set up, output the ks string if it exists. */

static void
setupterminal(void)
{
struct winsize parm;
struct termios newparm;
newparm = oldtermparm;
newparm.c_iflag &= ~(usint)(IGNCR | ICRNL);
newparm.c_lflag &= ~(usint)(ICANON | ECHO | ISIG);
newparm.c_cc[VMIN] = 1;
newparm.c_cc[VTIME] = 0;
newparm.c_cc[VSTART] = 0;
newparm.c_cc[VSTOP] = 0;
#ifndef NO_VDISCARD
newparm.c_cc[VDISCARD] = 0;
#endif

tcsetattr(ioctl_fd, TCSANOW, &newparm);

/* Get the screen size again in case it's changed */

if (ioctl(ioctl_fd, TIOCGWINSZ, &parm) == 0)
  {
  if (parm.ws_row != 0) tc_n_li = parm.ws_row;
  if (parm.ws_col != 0) tc_n_co = parm.ws_col;
  }

/* Set max rows/cols */

screen_max_row = tc_n_li -1;
screen_max_col = tc_n_co -1;

/* Start screen management and enable keypad if necessary, and enable mouse
usage where supported. */

if (tc_s_ti != NULL) outTCstring(tc_s_ti, 0);   /* start screen management */
if (tc_s_ks != NULL) outTCstring(tc_s_ks, 0);   /* enable keypad */
sys_mouse(TRUE);
}



/*************************************************
*                  Unset terminal state          *
*************************************************/

/* Disable reception of X,Y positions of mouse clicks. Do this always as a
fail-safe, even if mouse_enable is false. */

static void
resetterminal(void)
{
sys_mouse(FALSE);
if (tc_s_ke != NULL) outTCstring(tc_s_ke, 0);
if (tc_s_te != NULL) outTCstring(tc_s_te, 0);
sunix_flush();
tcsetattr(ioctl_fd, TCSANOW, &oldtermparm);
}



/*************************************************
*         Position to screen bottom for crash    *
*************************************************/

/* This function is called when about to issue a disaster error message. It
should position the cursor to a clear point at the screen bottom, and reset the
terminal if necessary. */

void
sys_crashposition(void)
{
if (main_screenOK)
  {
  /* LCOV_EXCL_START - no disasters in the test suite */
  sunix_rendition(s_r_normal);
  sunix_move(0, screen_max_row);
  resetterminal();
  /* LCOV_EXCL_STOP */
  }
main_screenmode = FALSE;
}



/*************************************************
*           Entry point for screen run           *
*************************************************/

/* Called from NE initialization to oversee a screen-editing run. */

void
sys_runscreen(void)
{
FILE *fid = NULL;
uschar *fromname = arg_from_name;
uschar *toname = (arg_to_name == NULL)? arg_from_name : arg_to_name;

/* Connect the Unix-specific screen-handling functions. */

sys_w_cls = sunix_cls;
sys_w_flush = sunix_flush;
sys_w_move = sunix_move;
sys_w_rendition = sunix_rendition;
sys_w_putc = sunix_putc;

sys_w_hscroll = NULL;
sys_w_vscroll = sunix_vscroll;

sys_setupterminal = setupterminal;
sys_resetterminal = resetterminal;

/* If there's an input file, open it before initializing the screen so that if
there's a problem, the screen is not disturbed. */

if (fromname != NULL && fromname[0] != 0 && Ustrcmp(fromname, "-") != 0)
  {
  fid = sys_fopen(fromname, US"r");
  if (fid == NULL)
    {
    /* LCOV_EXCL_START */
    fprintf(stderr, "** The file \"%s\" could not be opened: %s\n", fromname,
      strerror(errno));
    fprintf(stderr, "** NE abandoned.\n");
    main_rc = 16;
    return;
    /* LCOV_EXCL_STOP */
    }
  }

/* Set up maps for which keys may be used. The default is minimal. */

key_controlmap    = 0xFFFFFFFEu;    /* exclude nothing */
key_functionmap   = 0x7FFFFFFEu;    /* allow 1-30 */
key_specialmap[0] = 0x0000001Fu;    /* del, arrows */
key_specialmap[1] = 0x00000000u;    /* nothing shifted */
key_specialmap[2] = 0x00000000u;    /* nothing ctrled */
key_specialmap[3] = 0x00000000u;    /* nothing shifted+ctrled */

/* Additional keys on some special terminals - only one nowadays. */

if (tt_special == tt_special_xterm)
  {
  key_specialmap[0] = 0x0000011Fu;  /* insert, del, arrows */
  key_specialmap[1] = 0x0000009Fu;  /* shifted tab, delete, shifted arrows */
  key_specialmap[2] = 0x0000009Fu;  /* ctrl tab, delete, ctrl arrows */
  }

/* Read and save original terminal state before seting up. */

tcgetattr(ioctl_fd, &oldtermparm);
tc_int_ch = oldtermparm.c_cc[VINTR];
setupterminal();

/* If we are in an xterm, see whether it is UTF-8 enabled. The only way I've
found of doing this is to write two bytes which independently are two 8859
characters, but together make up a UTF-8 character, then read the cursor
position. The column will be one less for UTF-8. The two bytes we send are 0xc3
and 0xa1. As a UTF-8 sequence, these two encode 0xe1. The control sequence to
read the cursor position is ESC [ 6 n where ESC [ is the two-byte encoding of
CSI. */

if (tt_special == tt_special_xterm)
  {
  char buff[8];
  outTCstring(tc_s_cl, 0);                  /* clear the screen */
  outTCstring(tgoto(CS tc_s_cm, 1, 1), 0);  /* move to top left */
  sunix_flush();
  if (write(ioctl_fd, "\xc3\xa1\x1b\x5b\x36\x6e", 6)){};
  if (read(ioctl_fd, buff, 6)){};
  main_utf8terminal = buff[4] == '3';
  }

/* Now set up the screen */

s_init(screen_max_row, screen_max_col, TRUE);
scrn_init(TRUE);
scrn_windows();                     /* cause windows to be defined */
default_rmargin = main_rmargin;     /* first one = default */
main_screenOK = TRUE;               /* screen handling is set up */

/* Load the first file (already open) and any others named on the command line.
The output name for the first file is needed so that it can be displayed - it
may not be the same as the input name. */

if (init_init(fid, fromname, toname))
  {
  /* If main_rc is not zero, there was some non-fatal error while initializing,
  for example, splitting a line longer than the absolute maximum line length.
  Allow the user to see the error message. */

  if (main_rc != 0) scrn_rdline(FALSE, US "Press RETURN to continue ");

  /* Obey an initializer file before setting main_initialized - this makes any
  errors in the initializer hard.  */

  if (!main_noinit && main_einit != NULL) obey_init(main_einit);
  main_initialized = TRUE;

  /* Now obey any commands given by -opt on the command line. */

  if (main_opt != NULL)
    {
    int yield;
    scrn_display();
    s_selwindow(message_window, 0, 0);
    s_rendition(s_r_normal);
    s_flush();
    yield = cmd_obey(main_opt);
    if (yield != done_continue && yield != done_finish)
      {
      /* LCOV_EXCL_START - not encountered in tests */
      screen_forcecls = TRUE;
      scrn_rdline(FALSE, US "Press RETURN to continue ");
      /* LCOV_EXCL_STOP */
      }
    }

  /* Neither an initializer nor -opt ended the session. Display the screen and
  add the version information in the message space. */

  if (!main_done)
    {
    int x, y;
    scrn_display();
    x = s_x();
    y = s_y();
    s_selwindow(message_window, 0, 0);
    if (screen_max_col > 36)
      s_printf("NE %s %s using PCRE2 %s", version_string, version_date,
        version_pcre);
    main_shownlogo = TRUE;
    if (key_table[7] == ka_rc && screen_max_col > 64) /* The default config */
      s_printf(" - To exit, type ^G, W, Return");
    s_selwindow(first_window, x, y);
    }

  /* Keystroke buffer is empty, window hasn't changed size. */

  kbbackptr = 0;
  window_changed = FALSE;

  /* The main key-processing loop. */

  while (!main_done)
    {
    int type;
    int key = sunix_nextchar(&type);

    /* Wipe out error status at each interaction. */

    main_rc = error_count = 0;

    /* Deal with a change of window size. The signal handler sets the flag; we
    don't do anything until we have the next keystroke. */

    if (window_changed)
      {
      /* LCOV_EXCL_START */
      struct winsize parm;
      if (ioctl(ioctl_fd, TIOCGWINSZ, &parm) == 0)
        {
        if (parm.ws_row != 0) tc_n_li = parm.ws_row;
        if (parm.ws_col != 0) tc_n_co = parm.ws_col;
        screen_max_row = tc_n_li - 1;
        screen_max_col = tc_n_co - 1;
        scrn_setsize();
        }
      window_changed = FALSE;
      /* LCOV_EXCL_STOP */
      }

    /* Handle the keystroke */

    if (type == ktype_function)
      {
      if (key < 32) key_handle_function(key);
      else if (key >= Pkey_f0) key_handle_function(s_f_umax + key - Pkey_f0);
      else key_handle_function(Pkeytable[key - 127]);
      }
    else key_handle_data(key);
    }
  }

/* End of screen editing run */

sunix_rendition(s_r_normal);
resetterminal();
close(ioctl_fd);
}

/* End of sunix.c */
