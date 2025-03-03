/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2025 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: March 2025 */


/* This file contains code for handling individual function keystrokes when
running in screen mode. */


#include "ehdr.h"
#include "cmdhdr.h"
#include "shdr.h"
#include "scomhdr.h"
#include "keyhdr.h"

/* Table of keystrokes that are allowed in read-only mode. */

static uschar key_readonly[] = {
  0, /* ka_al */
  0, /* ka_alp */
  0, /* ka_cl */
  0, /* ka_clb */
  1, /* ka_co */
  1, /* ka_csd */
  1, /* ka_csl */
  1, /* ka_csls */
  1, /* ka_csle */
  1, /* ka_csnl */
  1, /* ka_cstl */
  1, /* ka_cstr */
  1, /* ka_csr */
  1, /* ka_cssbr */
  1, /* ka_cssl */
  1, /* ka_csstl */
  1, /* ka_cstab */
  1, /* ka_csptab */
  1, /* ka_csu */
  1, /* ka_cswl */
  1, /* ka_cswr */
  0, /* ka_cu */
  0, /* ka_dal */
  0, /* ka_dar */
  0, /* ka_dc */
  0, /* ka_de */
  0, /* ka_dl */
  0, /* ka_dp */
  0, /* ka_dtwl */
  0, /* ka_dtwr */
  0, /* ka_gm */
  0, /* ka_join */
  1, /* ka_lb */
  0, /* ka_pa */
  1, /* ka_rb */
  0, /* ka_reshow */
  1, /* ka_rc */
  0, /* ka_rs */
  1, /* ka_scbot */
  1, /* ka_scdown */
  1, /* ka_scleft */
  1, /* ka_scright */
  1, /* ka_sctop */
  1, /* ka_scup */
  0, /* ka_split */
  1, /* ka_tb */
  0, /* ka_dpleft */
  1, /* ka_forced */
  0, /* ka_last */
  0, /* ka_ret */
  1, /* ka_wbot */
  1, /* ka_wleft */
  1, /* ka_wright */
  1, /* ka_wtop */
  1, /* ka_xy */
  1, /* ka_mscr_down */
  1  /* ka_mscr_up */
};



/*************************************************
*              Change keystring value            *
*************************************************/

void
key_setfkey(int n, uschar *s)
{
uschar *news;
if (s == NULL) news = NULL; else
  {
  news = store_Xget(Ustrlen(s) + 1);
  Ustrcpy(news, s);
  }
if (main_keystrings[n] != NULL) store_free(main_keystrings[n]);
main_keystrings[n] = news;
}



/*************************************************
*            Display state of the mark           *
*************************************************/

/* If either the global limit or a text mark is set, display the information in
the message window. */

static void
ShowMark(void)
{
const char *gmsg, *mmsg, *sc;

if (mark_type == mark_unset && mark_line_global == NULL) return;

gmsg = (mark_line_global != NULL)? "Global limit set" : "";
switch (mark_type)
  {
  case mark_lines: mmsg = mark_hold?
    "Bulk line operations started" : "Bulk line operation started";
  break;
  case mark_text: mmsg = "Text block started"; break;
  case mark_rect: mmsg = "Rectangular block started"; break;
  default: mmsg = ""; break;
  }

sc = (gmsg[0] != 0 && mmsg[0] != 0)? " - " : "";
s_selwindow(message_window, -1, -1);
s_cls();
s_printf("%s%s%s", gmsg, sc, mmsg);
}



/*************************************************
*             Obey a command line                *
*************************************************/

static void
key_obey_commands(uschar *cmdline)
{
passive_commands = TRUE;
for (;;)
  {
  main_pendnl = TRUE;
  main_nowait = main_repaint = FALSE;

  if ((cmd_obey(cmdline) == done_wait || !main_pendnl || main_repaint) && !main_done)
    {
    screen_forcecls = TRUE;
    if (main_nowait) break;
    if (main_screensuspended)
      {
      int n;
      sys_mprintf(msgs_fid, "NE: ");
      if (Ufgets(cmd_buffer, CMD_BUFFER_SIZE, kbd_fid) == NULL)
        cmd_buffer[0] = 0;  /* LCOV_EXCL_LINE */
      n = Ustrlen(cmd_buffer);
      if (n > 0 && cmd_buffer[n-1] == '\n') cmd_buffer[n-1] = 0;
      }
    else
      {
      scrn_rdline(TRUE, US"NE> ");
      s_move(0, 0);
      s_flush();
      }
    main_flush_interrupt();
    if (cmd_buffer[0] == 0 || cmd_buffer[0] == '\n') break;
    cmdline = cmd_buffer;
    }
  else break;
  }

if (main_screensuspended)
  {
  if (withkey_fid != NULL) sleep(withkey_sleep);  /* Testing feature */
  scrn_restore();
  }

/* passive_commands is still true if no command made a significant change to
the data nor moved the current point. In this case, we can keep the screen as
it was. */

if (passive_commands) scrn_hint(sh_topline, 0, window_vector[0]);

if (!main_done)
  {
  if (!main_leave_message)
    {
    s_selwindow(message_window, -1, -1);
    s_cls();
    }
  main_pendnl = FALSE;
  ShowMark();
  }
}



/*************************************************
*         Ensure error message gets seen         *
*************************************************/

static void
post_error(void)
{
scrn_rdline(TRUE, US"NE> ");
key_obey_commands(cmd_buffer);
screen_forcecls = TRUE;
scrn_display();
}



/*************************************************
*               Cancel Mark                      *
*************************************************/

/* At the end, clear the message window. It both types of mark were set, the
one that is not cancelled will be reshown.

Argument:  TRUE to cancel the global limit mark
           FALSE to cancel any other mark

Returns:   nothing
*/

static void
CancelMark(BOOL globalmark)
{
usint markedcol;
linestr *markedline;
int old_window = s_window();
BOOL samechar = mark_line_global == mark_line && mark_col_global == mark_col;

if (globalmark)
  {
  markedcol = mark_col_global;
  markedline = mark_line_global;
  mark_line_global = NULL;
  }
else
  {
  markedcol = mark_col;
  markedline = mark_line;
  mark_line = NULL;
  mark_type = mark_unset;
  }

if (markedline != NULL)
  {
  usint row = BIGNUMBER;
  for (usint i = 0; i <= window_depth; i++) if (markedline == window_vector[i])
    {
    row = i;
    break;
    }
  if (row != BIGNUMBER && markedcol >= cursor_offset && markedcol < cursor_max)
    {
    if ((markedline->flags & lf_eof) != 0) markedline->flags |= lf_shn;
    else if (!samechar) scrn_invertchars(markedline, row, markedcol, 1, FALSE);
    }
  }

s_selwindow(message_window, -1, -1);
s_cls();
s_selwindow(old_window, -1, -1);
}



/*************************************************
*               Set Mark                         *
*************************************************/

/* This is used for all execpt the global mark. If the relevant mark is already
set, unset it. */

static int
SetMark(int marktype)
{
if (mark_type == mark_unset)
  {
  mark_line = main_current;
  mark_col = cursor_col;
  mark_type = marktype;
  mark_hold = FALSE;
  scrn_invertchars(main_current, cursor_row, cursor_col, 1, TRUE);
  return TRUE;
  }

if (mark_type == marktype) CancelMark(FALSE); else
  {
  error_moan(43, mark_type_names[marktype], mark_type_names[mark_type]);
  post_error();
  }

return FALSE;
}



/*************************************************
*         Handle one data keystroke              *
*************************************************/

/* Called from the system-dependent key functions. */

void
key_handle_data(int key)
{
int blen;
usint display_col = cursor_col;
usint byteoffset = line_offset(main_current, cursor_col);
BOOL waseof = (main_current->flags & lf_eof) != 0;
uschar bp[8];

if (main_readonly)
  {
  error_moan(53);
  post_error();
  return;
  }

/* In UTF-8 mode, convert a character > 127 into a multibyte string. */

if (allow_wide && key > 127)
  {
  blen = ord2utf8(key, bp);
  }

/* Not UTF-8 mode or key <= 127 - must be a single-byte character  */

else if (key <= 255)
  {
  *bp = key;
  blen = 1;
  }

/* Unsupported wide character; beep and ignore */

else
  {
  /* LCOV_EXCL_START */
  sys_beep();
  return;
  /* LCOV_EXCL_STOP */
  }

/* Handle overstriking within the existing line length */

if (main_overstrike && byteoffset < main_current->len)
  {
  int clen = line_bytecount(main_current->text + byteoffset, 1);
  if (clen == blen)
    {
    memcpy(main_current->text + byteoffset, bp, blen);
    }
  else
    {
    line_deletech(main_current, cursor_col, 1, TRUE);
    line_insertbytes(main_current, cursor_col, -1, bp, blen, 0);
    }
  cmd_recordchanged(main_current, cursor_col);
  }

/* Not overstrike, or beyond the existing line length */

else line_insertbytes(main_current, cursor_col, -1, bp, blen, 0);

/* This is all rather dodgy ... but useful! In binary mode, we allow editing to
occur in both the list of hex values and the interpreted display on the right
hand side, so at this point we have to update whichever one wasn't changed. */

if (main_binary)
  {
  /* LCOV_EXCL_START - not (yet?) tested in automatic tests */
  int len = main_current->len;
  uschar *p = main_current->text;
  uschar *h;

  /* Advance past the file offset value */

  while (len > 0 && isxdigit(*p)) { p++; len--; }
  while (len > 0 && *p == ' ') { p++; len--; }

  h = p;  /* First hex pair */

  /* Advance over all hex digits and spaces */

  while (len > 0 && (isxdigit(*p) || *p == ' ')) { p++; len--; }

  /* We should now have hit the asterisk that marks the start of the literal
  display area. If it's not there, do nothing. */

  if (len > 0 && *p == '*')
    {
    uschar *c = p + 2;

    p = main_current->text + cursor_col;  /* Where we inserted */

    /* We now have h at the first hex pair, c at the first literal, and p at
    the newly-edited character. Advance h and c in step until one or other
    comes to an end. Take appropriate action for which ever one reaches the
    insertion point. */

    while (c < main_current->text + main_current->len && *h != '*')
      {
      if (p == h || p == h+1)        /* Edited a hex pair */
        {
        int temp = tolower(h[0]);
        int cc = ((isdigit(temp))? temp - '0' : temp - 'a' + 10) << 4;
        temp = tolower(h[1]);
        cc += (isdigit(temp))? temp - '0' : temp - 'a' + 10;
        *c = isprint(cc)? cc : '.';  /* Update displayed literal */
        break;
        }

      if (p == c)                    /* Edited a literal character */
        {
        int temp = h[2];
        sprintf(CS h, "%2x", *c);    /* Update hex pair */
        h[2] = temp;
        display_col = h - main_current->text;
        break;
        }

      /* Advance both pointers to next character. */

      c++;
      h += 2;
      while (*h == ' ') h++;
      }
    }
  /* LCOV_EXCL_STOP */
  }

/* The line is now updated.Obey the hit-margin or hit_rhs-of-window functions
if appropriate. Otherwise, display the updated line. */

main_current->flags |= lf_shn;
if (cursor_col == main_rmargin) key_handle_function(s_f_lastchar);
else if (cursor_col >= cursor_max) key_handle_function(s_f_right);
else
  {
  if (waseof)   /* Display from left and display new eof line */
    {
    scrn_displayline(main_current, cursor_row, cursor_offset);
    if (cursor_row < window_depth)
      scrn_displayline(main_bottom, cursor_row+1, cursor_offset);
    }
  else scrn_displayline(main_current, cursor_row, display_col);
  s_move(++cursor_col - cursor_offset, cursor_row);
  }

ShowMark();        /* Might change indication of file changed */
s_selwindow(first_window, cursor_col - cursor_offset, cursor_row);
}



/*************************************************
*         Check scrolling possibility            *
*************************************************/

/* Separate tests for up/down, but the argument and result are always positive.

Argument:  maximum amount we'd like to scroll
Returns:   possible scroll amount
*/

static usint
tryscrollup(int amount)
{
usint cando = 0;
linestr *top = window_vector[0];
while (amount--)
  {
  top = top->prev;
  if (top == NULL) break;
  cando++;
  }
return cando;
}


static usint
tryscrolldown(int amount)
{
usint cando = 0;
linestr *bot = window_vector[window_depth];
if (bot != NULL) while (amount--)
  {
  bot = bot->next;
  if (bot == NULL) break;
  cando++;
  }
return cando;
}



/*************************************************
*  Common code after horiz scroll/window change  *
*************************************************/

/* This can be called after lines have been deleted, in which case the
window_vector pointer is set to +1 to indicate "something unknown is on the
screen". NULL cannot be used, as it implies that the line is blank. */

static void
scrn_afterhscroll(void)
{
for (usint i = 0; i <= window_depth; i++)
  {
  linestr *line = window_vector[i];
  if (line != NULL && (intptr_t)line > 1) line->flags |= lf_shn;
  }
main_drawgraticules |= dg_both;
scrn_display();
}



/*************************************************
*       Common code after vertical scroll        *
*************************************************/

static void
aftervscroll(void)
{
main_current->flags |= lf_shn;
for (usint i = 0; i <= window_depth; i++) window_vector[i] = NULL;
window_vector[cursor_row] = main_current;
}



/*************************************************
*       Common cursor to true line start code    *
*************************************************/

static void
do_csls(void)
{
cursor_col = 0;
if (cursor_offset > 0)
  {
  cursor_offset = 0;
  cursor_max = window_width;
  scrn_afterhscroll();
  }
}



/*************************************************
*           Adjust scroll so cursor shows        *
*************************************************/

/* This function is called after a cursor adjustment which might require the
window to be scrolled left or right. */

static void
adjustscroll(void)
{
if (cursor_col < cursor_offset)
  {
  while (cursor_col < cursor_offset) cursor_offset -= main_hscrollamount;
  cursor_max = cursor_offset + window_width;
  scrn_afterhscroll();
  }

else if (cursor_col > cursor_max)
  {
  while (cursor_col > cursor_max)
    {
    cursor_offset += main_hscrollamount;
    cursor_max = cursor_offset + window_width;
    }
  scrn_afterhscroll();
  }
}



/*************************************************
*      Common cursor hit window bottom code      *
*************************************************/

/* We do not need to set cursor_row, because it is set from the current line by
scrn_display(). This function is also called for mouse scroll down. For a
non-mouse scroll (actually hit the window bottom), always advance the current
line. For a mouse scroll, if the current line is going to disappear off the
top, advance it to the new top line.

Argument:  TRUE if a mouse scroll
Returns:   nothing
*/

static void
do_wbot(BOOL mouse_scroll)
{
usint cando = tryscrolldown(mouse_scroll? main_vmousescroll:main_vcursorscroll);
if (cando > window_depth) cando = window_depth;

if (cando != 0)
  {
  linestr *nextline = (window_vector[window_depth] == NULL)?
    NULL : window_vector[window_depth]->next;

  if (!mouse_scroll) main_current = main_current->next; else
    {
    for (usint i = 0; i < cando; i++)
      {
      if (main_current == window_vector[i])
        {
        /* LCOV_EXCL_START - hard to test automatically */
        main_current = window_vector[cando];
        break;
        /* LCOV_EXCL_STOP */
        }
      }
    }

  s_vscroll(window_depth, 0, -((int)cando));
  for (usint i = 0; i <= window_depth - cando; i++)
    window_vector[i] = window_vector[i+cando];

  for (usint i = window_depth - cando + 1; i <= window_depth; i++)
    {
    window_vector[i] = nextline;
    scrn_displayline(nextline, i, cursor_offset);
    if (nextline != NULL) nextline = nextline->next;
    }
  }
}



/*************************************************
*           Act on mouse scroll up               *
*************************************************/

/* This can be from a wheel mouse or a click in the upper graticule. */

static void
do_mouse_scroll_up(void)
{
usint cando = tryscrollup(main_vmousescroll);
if (cando > window_depth) cando = window_depth;
if (cando != 0)
  {
  linestr *prev = window_vector[0]->prev;
  for (usint i = window_depth; i > window_depth - cando; i--)
    {
    if (main_current == window_vector[i])
      {
      main_current = window_vector[window_depth - cando];
      break;
      }
    }
  s_vscroll(window_depth, 0, cando);
  for (usint i = window_depth; i >= cando; i--)
    window_vector[i] = window_vector[i-cando];
  for (usint i = cando - 1;; i--)
    {
    window_vector[i] = prev;
    scrn_displayline(prev, i, cursor_offset);
    prev = prev->prev;
    if (i == 0) break;
    }
  }
}



/*************************************************
*         Read and obey command line             *
*************************************************/

static void
do_read_commands(void)
{
scrn_invertchars(main_current, cursor_row, cursor_col, 1, TRUE);
scrn_rdline(TRUE, US"NE> ");
s_selwindow(first_window, -1, -1);
scrn_invertchars(main_current, cursor_row, cursor_col, 1, FALSE);
s_selwindow(message_window, 0, 0);
s_flush();
key_obey_commands(cmd_buffer);
}



/*************************************************
*           Act on a group of lines              *
*************************************************/

/* This function is used for the Line Operations as seen by the user, and also
for some of the rectangular operations.

Argument:  action required
Returns:   nothing
*/

static void
LineBlockOp(int op)
{
linestr *line = ((mark_type == mark_lines) || (op == lb_rectsp))?
  mark_line : main_current;
linestr *endline = main_current;
int above = line_checkabove(line);
int row = (int)cursor_row - above;
int left = 0;     /* Otherwise compilers complain */
int right = 0;
int rectwidth = 0;

/* Set up left & right for rectangular operations */

if (op == lb_rectsp)
  {
  if (cursor_col < mark_col)
    {
    left = cursor_col;
    right = mark_col;
    }
  else
    {
    left = mark_col;
    right = cursor_col;
    }
  rectwidth =  right - left;
  }

/* Sort out top and bottom */

if (above < 0)
  {
  linestr *temp = line;
  line = main_current;
  endline = temp;
  row = cursor_row;
  }

/* Set up alignment point for align with previous */

if (op == lb_alignp)
  {
  linestr *prevline = line->prev;
  cursor_col = 0;
  if (prevline != NULL)
    {
    uschar *p = prevline->text;
    for (usint i = 0; i < prevline->len; i++)
      if (p[i] != ' ') { cursor_col = i; break; }
    }
  }

/* Remove mark when required */

if (op == lb_rectsp ||
    (mark_type == mark_lines && (!mark_hold || op == lb_delete)))
  CancelMark(FALSE);

/* Deleting a block of lines is handled specially */

if (op == lb_delete)
  {
  int done = 1;
  usint count = 0;

  while (done > 0)
    {
    if ((line->flags & lf_eof) != 0)
      {
      if (count == 0) return;          /* Not deleted anything */
      done = 0;
      }
    else
      {
      if (line == endline) done = -1;
      line = line_delete(line, TRUE);
      count++;
      }
    }

  main_current = line;

  /* If at eof, allow complete re-format. Otherwise, if block was all on
  screen, scroll it up. */

  if (done == 0) screen_autoabove = FALSE; else
    {
    usint botrow = cursor_row;
    usint toprow = (cursor_row + 1) - count;
    if (above < 0)
      {
      botrow += count - 1;
      toprow = cursor_row;
      }

    if (botrow < window_depth && count <= botrow + 1)
      {
      s_vscroll(window_depth, toprow, -((int)count));
      for (usint i = botrow+1; i <= window_depth; i++)
        window_vector[i-count] = window_vector[i];
      for (usint i = window_depth - count + 1; i <= window_depth; i++)
        window_vector[i] = NULL;
      }
    else     /* new current line in old place */
      {
      main_current->flags |= lf_shn;
      window_vector[cursor_row] = main_current;
      }
    }

  scrn_display();
  }


/* The other operations can share code */

else for (;;)
  {
  BOOL longline = line_charcount(line->text, line->len) > cursor_max + 1;
  BOOL onscreen = (row >= 0 && (usint)row <= window_depth);
  int action;

  if ((line->flags & lf_eof) == 0) switch (op)
    {
    case lb_align:
    case lb_alignp:
      {
      if (longline && onscreen)
        scrn_invertchars(line, row, cursor_max, 1, FALSE);
      line_leftalign(line, cursor_col, &action);
      if (onscreen)
        {
        if (cursor_offset > 0 || cursor_col > cursor_max ||
            (longline && action < 0))
          {
          line->flags |= lf_shn;
          scrn_display();
          }
        else if (action > 0)
          {
          s_hscroll(0, row, window_width, row,action);
          if (line_charcount(line->text, line->len) > cursor_max + 1)
            scrn_invertchars(line, row, cursor_max, 1, TRUE);
          }
        else s_hscroll(0,row,window_width,row,action);
        }
      }
    break;

    case lb_eraseright:
    if (line_offset(line, cursor_col) < line->len)
      {
      line_deletech(line, cursor_col, line->len - cursor_col, TRUE);
      if (onscreen)
        {
        s_move(cursor_col - cursor_offset, row);
        s_eraseright();
        }
      }
    break;

    case lb_eraseleft:
    line_deletech(line, cursor_col, cursor_col, FALSE);
    if (onscreen && cursor_col != cursor_offset)
      {
      if (cursor_offset > 0)
        {
        cursor_offset = 0;
        cursor_max = window_width + cursor_offset;
        scrn_afterhscroll();
        }
      else if (longline) line->flags |= lf_shn;
      else s_hscroll(0, row, window_width, row,
        (int)cursor_offset - (int)cursor_col);
      }
    break;

    case lb_closeup:
      {
      int count = 0;
      uschar *p = line->text;
      for (usint i = line_offset(line, cursor_col); i < line->len; i++)
        if (p[i] == ' ') count++; else break;
      line_deletech(line, cursor_col, count, TRUE);
      if (onscreen && count > 0)
        {
        if (longline) line->flags |= lf_shn;
        else s_hscroll((int)cursor_col - (int)cursor_offset, row, window_width,
          row, -count);
        }
      }
    break;

    case lb_closeback:
      {
      int count = 0;
      usint bcol = line_offset(line, cursor_col);
      uschar *p = line->text;

      if (bcol > line->len) bcol = line->len;
      for (int i = bcol - 1; i >= 0; i--)
        if (p[i] == ' ') count++; else break;
      line_deletebytes(line, bcol - count, count, TRUE);

      if (onscreen && count > 0)
        {
        if (longline || cursor_col - count < cursor_offset)
          line->flags |= lf_shn;
        else
          s_hscroll(cursor_col-cursor_offset-count,row,window_width,row,-count);
        }

      if (line == endline)
        {
        cursor_col -= count;
        if (cursor_col < cursor_offset)
          {
          while (cursor_col < cursor_offset)
            cursor_offset -= main_hscrollamount;
          cursor_max = window_width + cursor_offset;
          scrn_afterhscroll();
          }
        }
      }
    break;

    case lb_rectsp:
    line_insertbytes(line, left, -1, NULL, 0, rectwidth);
    if (onscreen) line->flags |= lf_shn;
    break;
    }

  row++;
  if (line == endline) break;
  line = line->next;
  }
}



/*************************************************
*        Handle one function keystroke           *
*************************************************/

/* Called from both system-dependent and system-independent code. The first
input value is either in the range 0-31, or is one of the values defined in
keyhdr.h for machine-independence.

The functions supplied by the terminal drivers are "actual control keystrokes".
Those with values less than s_f_umax represent "ctrl" type keystrokes and
special keys. Those between s_f_umax and 200 are "function" keystrokes. Both of
these kinds are user settable, and are translated into "logical control
keystrokes" via the vector keytable. Keystrokes with values >= 200 are fixed in
meaning and cannot be changed by the user. We nevertheless translate via a
fixed table, to get them into the same space as the others. */

/* s_f_umax is the highest used special key function */
/* s_f_fbase is the base of fixed keys (= 200) */
/* s_f_fmax is the highest used fixed key */

void
key_handle_function(int function)
{
int cursor_byte;

if (function <= s_f_umax+max_fkey) function = key_table[function];
  else if (s_f_fbase <= function && function <= s_f_fmax)
    function = key_fixedtable[function - s_f_fbase];
  else function = ka_push;    /* LCOV_EXCL_LINE - unknown are ignored */

if (main_readonly && function >= ka_firstka && function <= ka_lastka &&
    !key_readonly[function - ka_firstka])
  {
  error_moan(53);
  post_error();
  return;
  }

/* Now process the function */

switch (function)
  {
  case ka_xy:                 /* mouse left click */
  if (mouse_row < 1) do_mouse_scroll_up();
  else if (mouse_row == screen_max_row - 1) do_wbot(TRUE);
  else if (mouse_row >= screen_max_row) do_read_commands();

  /* Adjust mouse_row to data window row */

  else if (window_vector[--mouse_row] != NULL)
    {
    cursor_col = mouse_col + cursor_offset;
    cursor_row = mouse_row;
    main_current = window_vector[mouse_row];
    }
  break;

  case ka_csl:                /* cursor left */
  if (cursor_col > cursor_offset) cursor_col--;
    else key_handle_function(s_f_left);
  break;

  case ka_csr:                /* cursor right */
  if (cursor_col - cursor_offset < screen_max_col) cursor_col++;
    else key_handle_function(s_f_right);
  break;

  case ka_cstab:              /* next tab */
  do { cursor_col++; }  while (cursor_col % 8 != 0);
  adjustscroll();
  break;

  case ka_csptab:             /* previous tab */
  do { cursor_col--; }  while (cursor_col % 8 != 0);
  adjustscroll();
  break;

  case ka_csu:                /* cursor up */
  if (cursor_row > 0)
    {
    cursor_row--;
    main_current = main_current->prev;
    }
  else key_handle_function(s_f_top);
  break;

  case ka_csd:                /* cursor down */
  if (cursor_row < window_depth)
    {
    if (main_current->next != NULL)
      {
      cursor_row++;
      main_current = main_current->next;
      }
    }
  else key_handle_function(s_f_bottom);
  break;

  case ka_cssl:               /* cursor to screen left */
  cursor_col = cursor_offset;
  break;

  case ka_cstl:               /* cursor to text left on screen */
    {
    uschar *p = main_current->text;
    int len = main_current->len;
    cursor_col = cursor_offset;
    cursor_byte = (p == NULL)? 0 : line_soffset(p, p + len, cursor_col);
    while (cursor_byte < len && p[cursor_byte] == ' ' &&
           cursor_col < cursor_offset + window_width)
      {
      cursor_col++;
      cursor_byte++;
      }
    if (cursor_byte >= len || p[cursor_byte] == ' ')
      cursor_col = cursor_offset;
    }
  break;

  case ka_cstr:               /* cursor to text right on screen */
    {
    usint clen = line_charcount(main_current->text, main_current->len);
    if (clen <= cursor_offset) cursor_col = cursor_offset; else
      {
      cursor_col = cursor_offset + window_width;
      if (cursor_col >= clen) cursor_col = clen;
      }
    }
  break;

  /* Original fancy optimization for overstrike removed: (a) it lost the
  character - a bug - and (b) chars can now be of different lengths. */

  case ka_dc:                 /* delete character forwards */
  line_deletech(main_current, cursor_col, 1, TRUE);
  if (main_overstrike && cursor_col < line_charcount(main_current->text,
      main_current->len))
    line_insertbytes(main_current, cursor_col, -1, US" ", 1, 0);
  scrn_displayline(main_current, cursor_row, cursor_col);
  break;

  /* Original fancy optimization for overstrike removed: (a) it lost the
  character - a bug - and (b) chars can now be of different lengths. */

  case ka_dp:                 /* delete character backwards */
  if (cursor_col == 0) key_handle_function(s_f_leftdel); else
    {
    line_deletech(main_current, cursor_col--, 1, FALSE);
    if (main_overstrike && cursor_col < line_charcount(main_current->text,
        main_current->len))
      line_insertbytes(main_current, cursor_col, -1, US" ", 1, 0);
    if (cursor_col < cursor_offset) adjustscroll();
      else scrn_displayline(main_current, cursor_row, cursor_col);
    }
  break;

  case ka_csnl:               /* cursor to start of next line */
  if (cursor_row == window_depth) do_wbot(FALSE); else
    {
    linestr *next = main_current->next;
    if (next != NULL) main_current = next;
    }
  do_csls();     /* move to true line start */
  break;

  case ka_cssbr:              /* cursor to screen bottom right */
    {
    int i = window_depth;
    main_current = window_vector[i];
    while (main_current == NULL) main_current = window_vector[--i];
    cursor_row = i;
    cursor_col = window_width + cursor_offset;
    }
  break;

  case ka_csstl:              /* cursor to screen top left */
    {
    main_current = window_vector[0];
    cursor_col = cursor_offset;
    cursor_row = 0;
    }
  break;

  /* Assume all "word" chars are ASCII */

  case ka_cswl:               /* cursor left by one word */
    {
    uschar *p = main_current->text;
    int len = main_current->len;
    cursor_byte = line_soffset(p, p+len, cursor_col);
    if (cursor_byte >= len) cursor_byte = len;
    for (;;)                  /* repeat for when backing up to previous line */
      {
      if (cursor_byte > 0)
        while (--cursor_byte > 0 && (ch_tab[p[cursor_byte]] & ch_word) == 0);
      if (cursor_byte == 0)
        {
        if (main_current->prev == NULL) break;
        main_current = main_current->prev;
        p = main_current->text;
        cursor_byte = main_current->len;
        if (cursor_row > 0) cursor_row--; else scrn_display();
        }
      else
        {
        while (cursor_byte > 0 && (ch_tab[p[cursor_byte]] & ch_word) != 0)
          cursor_byte--;
        if ((ch_tab[p[cursor_byte]] & ch_word) == 0) cursor_byte++;
        break;
        }
      }
    }
  cursor_col = line_charcount(main_current->text, cursor_byte);
  adjustscroll();
  break;

  /* Assume all "word" chars are ASCII */

  case ka_cswr:               /* cursor right by one word */
    {
    uschar *p = main_current->text;
    int len = main_current->len;
    BOOL first = TRUE;
    cursor_byte = line_soffset(p, p+len, cursor_col);
    for (;;)                  /* loop for moving on to next line */
      {
      if ((main_current->flags & lf_eof) != 0) break;
      if (first)
        while (cursor_byte < len && (ch_tab[p[cursor_byte]] & ch_word) != 0)
          cursor_byte++;
      while (cursor_byte < len && (ch_tab[p[cursor_byte]] & ch_word) == 0)
        cursor_byte++;
      first = FALSE;
      if (cursor_byte >= len)
        {
        cursor_byte = 0;
        main_current = main_current->next;
        p = main_current->text;
        len = main_current->len;
        scrn_display();
        }
      else break;
      }
    }
  cursor_col = line_charcount(main_current->text, cursor_byte);
  adjustscroll();
  break;

  case ka_csls:               /* cursor to true line start */
  do_csls();
  break;

  case ka_csle:               /* cursor to true line end */
  cursor_col = line_charcount(main_current->text, main_current->len);
  adjustscroll();
  break;

  case ka_split:    /* split line */
    {
    linestr *lineold = main_current;
    usint row = cursor_row;
    usint iline = window_depth - main_ilinevalue - 1;

    main_current = line_split(main_current, cursor_col);
    cursor_col = 0;
    if (main_AutoAlign)
      {
      int dummy;
      uschar *p = lineold->text;
      for (usint i = 0; i < lineold->len; i++)
        if (p[i] != ' ') { cursor_col = i; break; }
      if (cursor_col != 0) line_leftalign(main_current, cursor_col, &dummy);
      }

    if (row > iline)
      for (usint i = 1; i <= row; i++) window_vector[i-1] = window_vector[i];
    else
      {
      for (usint i = window_depth; i > row + 2; i--)
        window_vector[i] = window_vector[i-1];
      cursor_row++;
      }

    window_vector[cursor_row] = main_current;

    if (cursor_offset == 0)
      {
      if (row > iline) s_vscroll(row, 0, -1);
        else s_vscroll(window_depth, row+1, 1);
      scrn_displayline(lineold, cursor_row - 1, cursor_col);
      scrn_displayline(main_current, cursor_row, 0);
      }
    else
      {
      adjustscroll();
      scrn_displayline(lineold, cursor_row - 1, cursor_col);
      scrn_displayline(main_current, cursor_row, cursor_col);
      }
    }
  break;

  case ka_last:  /* rightmost char typed - split the line */
    {
    usint c;
    uschar *p = main_current->text;
    linestr *lineold = main_current;

    cursor_byte = line_soffset(p, p + main_current->len, cursor_col);
    s_move(cursor_col - cursor_offset, cursor_row);
    GETCHAR(c, (p + cursor_byte), p + main_current->len);
    s_putc(c);

    for (int sp = cursor_byte; sp > 0; sp--)
      {
      if (p[sp] == ' ')
        {
        cursor_col = line_charcount(p, sp) + 1;
        cursor_byte = sp;
        break;
        }
      }

    if (cursor_col >= cursor_offset && cursor_col <= cursor_max)
      {
      s_move(cursor_col - cursor_offset, cursor_row);
      s_eraseright();
      }

    main_current = line_split(main_current, cursor_col);
    cursor_col = main_rmargin - cursor_col + 1;

    if (main_AutoAlign)
      {
      int dummy;
      int indent = -1;
      uschar *bp = lineold->text;
      for (usint i = 0; i < lineold->len; i++)
        if (bp[i] != ' ') { indent = i; break; }
      if (indent > 0)
        {
        line_leftalign(main_current, indent, &dummy);
        cursor_col += indent;
        }
      }

    if (cursor_row > window_depth - 3)
      {
      for (usint i = 1; i <= cursor_row; i++)
        window_vector[i-1] = window_vector[i];
      s_vscroll(cursor_row, 0, -1);
      }
    else
      {
      for (usint i = window_depth; i > cursor_row + 1; i--)
        window_vector[i] = window_vector[i-1];
      s_vscroll(window_depth, cursor_row++, 1);
      }

    window_vector[cursor_row] = main_current;

    if (cursor_col >= cursor_offset && cursor_col <= cursor_max)
      {
      main_current->flags |= lf_shn;
      scrn_display();
      }
    else adjustscroll();
    }
  break;

  case ka_wleft:      /* hit left of screen window */
  if (cursor_offset > 0)
    {               /* part way into line */
    cursor_col--;
    adjustscroll();
    break;           /* that's all */
    }
  else               /* at true line start -- back up */
    {
    linestr *prev = main_current->prev;
    if (prev != NULL)
      {
      cursor_col = line_charcount(prev->text, prev->len);
      if (cursor_col > cursor_max)
        {
        cursor_offset = cursor_col/main_hscrollamount;
        if (cursor_offset > 0)
          cursor_offset = (cursor_offset - 1) * main_hscrollamount;
        }
      if (cursor_offset > 0)
        {
        cursor_max = cursor_offset + window_width;
        scrn_afterhscroll();
        }
      if (cursor_row != 0)
        {
        main_current = prev;
        cursor_row--;
        break;       /* not at top of screen */
        }
      }
    }
  /* Fall through */

  case ka_wtop:       /* hit top of screen */
    {
    usint cando = tryscrollup(main_vcursorscroll);
    if (cando > window_depth) cando = window_depth;
    if (cando > 0)
      {
      linestr *prev = window_vector[0]->prev;
      main_current = main_current->prev;
      s_vscroll(window_depth, 0, cando);
      for (usint i = window_depth; i >= cando; i--)
        window_vector[i] = window_vector[i-cando];
      for (usint i = cando - 1;; i--)
        {
        window_vector[i] = prev;
        scrn_displayline(prev, i, cursor_offset);
        prev = prev->prev;
        if (i == 0) break;
        }
      }
    }
  break;

  case ka_mscr_up:    /* mouse scroll up */
  do_mouse_scroll_up();
  break;

  case ka_wright:            /* hit right of window */
  cursor_col++;
  adjustscroll();
  break;

  case ka_mscr_down:         /* mouse scroll down */
  do_wbot(TRUE);
  break;

  case ka_wbot:              /* hit bottom of screen */
  do_wbot(FALSE);
  break;

  case ka_reshow:            /* reshow screen */
  scrn_windows();
  s_selwindow(0, -1, -1);    /* select whole screen */
  s_cls();                   /* clear it */
  scrn_afterhscroll();
  break;

  case ka_dpleft:     /* delete previous at left edge */
  if (cursor_offset > 0)
    {
    /* If scrolled right, delete a char and scroll left. Currently, this code
    is never executed, because this function is called for this action only
    when cursor_col == 0, and in that case, cursor_offset must also be 0.
    However, leave it in place in case things change. */

    /* LCOV_EXCL_START */
    if ((main_current->flags & lf_eof) == 0)
      {
      line_deletech(main_current, cursor_col, 1, FALSE);
      cursor_col--;
      adjustscroll();
      break;
      }
    /* LCOV_EXCL_STOP */
    }
  /* Fall through */

  /* If fully scrolled left, "delete the newline" by concatenating with the
  previous line (falling through). A separate comment (above) is needed to
  stop compiler complaints. */

  case ka_join:    /* concatenate with previous line */
  if (mark_type != mark_lines || mark_line != main_current)
    {
    linestr *prev = main_current->prev;
    if (prev != NULL)
      {
      cursor_col = line_charcount(prev->text, prev->len);
      if ((main_current->flags & lf_eof) != 0)
        {
        main_current = prev;
        cursor_row--;
        if (cursor_col == 0)          /* delete if previous empty line */
          LineBlockOp(lb_delete);
        adjustscroll();
        scrn_display();
        }
      else
        {
        main_current = line_concat(main_current, 0);
        if (cursor_row != 0 && cursor_col >= cursor_offset && cursor_col < cursor_max)
          {
          window_vector[cursor_row-1] = main_current;
          for (usint i = cursor_row; i < window_depth; i++)
            window_vector[i] = window_vector[i+1];
          window_vector[window_depth] = NULL;
          s_vscroll(window_depth, cursor_row, -1);
          cursor_row--;
          scrn_displayline(main_current, cursor_row, cursor_col);
          }
        else adjustscroll();
        }
      }
    }
  else
    {
    error_moan(39);  /* Grumble and do nothing if the line has the line mark */
    post_error();
    }
  break;

  case ka_al:            /* align line with cursor */
  LineBlockOp(lb_align);
  break;

  case ka_alp:           /* align with previous line */
  LineBlockOp(lb_alignp);
  adjustscroll();
  break;

  case ka_cl:            /* close up line(s) */
  LineBlockOp(lb_closeup);
  break;

  case ka_clb:           /* close up lines(s) to the left */
  LineBlockOp(lb_closeback);
  break;

  /* Cut, copy, or delete an area of text or a rectangle. We remember the mark
  data, then delete the mark, then do the action. */

  case ka_cu:
  case ka_co:
  case ka_de:
  if (mark_type == mark_text || mark_type == mark_rect)
    {
    linestr *markline = mark_line;
    int markcol = mark_col;
    int type = mark_type;
    CancelMark(FALSE);
    cut_cut(markline, markcol, type, (function == ka_co), (function == ka_de));
    adjustscroll();
    }
  break;

  case ka_dl:            /* delete line */
  LineBlockOp(lb_delete);
  break;

  case ka_dal:           /* erase left */
  LineBlockOp(lb_eraseleft);
  cursor_col = cursor_offset;
  break;

  case ka_dar:           /* erase right */
  LineBlockOp(lb_eraseright);
  break;

  case ka_dtwl:          /* delete to word left */
  e_dtwl(NULL);
  break;

  case ka_dtwr:          /* delete to word right */
  e_dtwr(NULL);
  break;

  case ka_lb:            /* mark/unmark line block */
  if (mark_type == mark_lines && !mark_hold) mark_hold = TRUE;
    else SetMark(mark_lines);
  break;

  case ka_gm:            /* mark/unmark global limit */
  if (mark_line_global == NULL)
    {
    mark_line_global = main_current;
    mark_col_global = cursor_col;
    scrn_invertchars(main_current, cursor_row, cursor_col, 1, TRUE);
    }
  else CancelMark(TRUE);
  break;

  /* Paste data from the cut buffer back into the file; it is either a string
  of text or a rectangle. Keep the current line where it is on the screen,
  except when the total file is small, or the pasting is big and near the top
  of the screen. */

  case ka_pa:
  if (cut_buffer != NULL)
    {
    if (cut_type == cuttype_text)
      {
      int row = cursor_row;
      if ((cut_pastetext() > row && row < 10) ||
        main_linecount < window_depth) screen_autoabove = FALSE;
      }
    else cut_pasterect();
    }
  else
    {
    error_moan(55);
    post_error();
    }
  adjustscroll();
  break;

  case ka_rb:            /* start rectangular operation */
  SetMark(mark_rect);
  break;

  case ka_rc:            /* read commands */
  do_read_commands();
  break;

  case ka_rs:            /* insert rectangle of spaces */
  if (mark_type == mark_rect) LineBlockOp(lb_rectsp);
  break;

  case ka_tb:            /* start text string operation */
  SetMark(mark_text);
  break;

  case ka_scleft:        /* scroll left */
  if (cursor_offset > 0)
    {
    if (cursor_offset >= main_hscrollamount)
      cursor_offset -= main_hscrollamount;
    else cursor_offset = 0;  /* LCOV_EXCL_LINE - never reachable */
    cursor_max = window_width + cursor_offset;
    if (cursor_col > cursor_max) cursor_col = cursor_max;
    scrn_afterhscroll();
    }
  else cursor_col = 0;
  break;

  case ka_scright:    /* scroll right */
  cursor_offset += main_hscrollamount;
  cursor_max = cursor_offset + window_width;
  if (cursor_col < cursor_offset) cursor_col = cursor_offset;
  scrn_afterhscroll();
  break;

  case ka_scup:         /* scroll up */
    {
    usint cando = tryscrollup(window_depth);

    /* No upward scrolling possible. Move current to top line. */

    if (cando == 0) main_current = main_top;

    /* Scroll up by screen - 2 lines, or until hit the top. Move the
    current line back by one scroll amount if possible. */

    else
      {
      linestr *oldcurrent = main_current;
      linestr *top = window_vector[0];
      BOOL hittop = FALSE;
      BOOL changecurrent = TRUE;

      /* Don't change current if it will be visible on new, fully-scrolled
      screen. */

      if (cursor_row <= 1)
        {
        cursor_row += window_depth - 1;
        changecurrent = FALSE;
        }

      /* Find new top line, moving current back too, if required. */

      for (usint i = 1; i < window_depth; i++)
        {
        linestr *prev = top->prev;
        if (prev == NULL) hittop = TRUE; else top = prev;
        if (changecurrent && main_current->prev != NULL)
          main_current = main_current->prev;
        }

      /* If hit the top, force it to be the top displayed line. If the
      previous current is still visible, re-instate it. */

      if (hittop)
        {
        scrn_hint(sh_topline, 0, top);
        for (usint i = 1; i <= window_depth && top != NULL; i++)
          {
          top = top->next;
          if (top == oldcurrent)
            {
            main_current = oldcurrent;
            break;
            }
          }
        }
      else aftervscroll();
      }

    scrn_display();
    }
  break;

  case ka_scdown:        /* scroll down */
    {
    usint cando = tryscrolldown(window_depth);

    /* No downward scrolling possible. Move current to bottom line. */

    if (cando == 0)
      {
      main_current = main_bottom;
      screen_autoabove = FALSE;
      }

    /* Scroll down by screen - 2 lines, or until hit the bottom. Move the
    current line forward by one scroll amount if possible. */

    else
      {
      BOOL hitbot = FALSE;
      int changecurrent = TRUE;
      linestr *oldcurrent = main_current;
      linestr *bot = window_vector[window_depth];

      /* Don't change current if it will be visible on new, fully-scrolled
      screen. */

      if (cursor_row >= window_depth-1)
        {
        cursor_row -= (cando == window_depth)? cando-1 : cando;
        changecurrent = FALSE;
        }

      /* Find new bottom line, moving current forwards too, if required. */

      for (usint i = 1; i < window_depth; i++)
        {
        if ((bot->flags & lf_eof) != 0) hitbot = TRUE; else bot = bot->next;
        if (changecurrent && main_current->next != NULL)
          main_current = main_current->next;
        }

      /* If hit the bottom, force it to be the bottom displayed line. If the
      previous current is still visible, re-instate it. */

      if (hitbot)
        {
        linestr *top = bot;
        for (usint i = 1; i <= window_depth; i++)
          {
          if (top->prev == NULL) break;
          top = top->prev;
          if (top == oldcurrent) main_current = oldcurrent;
          }
        scrn_hint(sh_topline, 0, top);
        }
      else aftervscroll();
      }

    scrn_display();
    }
  break;

  case ka_sctop:         /* scroll top */
  main_current = main_top;
  scrn_display();
  break;

  case ka_scbot:         /* scroll bottom */
  main_current = main_bottom;
  scrn_display();
  break;

  /* The definitions of the ka_xxx keystrokes are arranged so that their values
  are all greater than the maximum function keystring. Hence we can now just
  check for a number in the relevant range and take it to be a string. */

  default:
  if (1 <= function && function <= max_keystring)
    {
    uschar *keydata = main_keystrings[function];
    if (keydata != NULL)
      {
      s_selwindow(message_window, 0, 0);
      s_cls();
      s_printf("NE> %s", keydata);
      s_move(0, 0);    /* indicate being obeyed */
      s_flush();       /* flush buffered output */
      key_obey_commands(keydata);
      if (!main_done && currentbuffer != NULL) scrn_display();
      }
    }
  break;
  }

if (!main_done && currentbuffer != NULL)
  {
  scrn_display();   /* Adjusts cursor_row */
  ShowMark();
  s_selwindow(first_window, cursor_col - cursor_offset, cursor_row);
  }
}

/* End of ekey.c */
