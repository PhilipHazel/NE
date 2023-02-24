/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: January 2023 */


/* This file contains code for reading a command line off the screen. */


#include "ehdr.h"
#include "keyhdr.h"
#include "shdr.h"
#include "scomhdr.h"


static usint promptlen;
static usint scrolled;
static int pmax;



/*************************************************
*              Reshow altered line               *
*************************************************/

/*
Arguments:
  p            byte offset in the command buffer from which to show
  changed      TRUE if the line has been changed; FALSE if (e.g. just scroll)
  prompt       length of prompt

Returns: nothing
*/

static void
reshow(int p, BOOL changed, uschar *prompt)
{
usint i, k;
usint cp = line_charcount(cmd_buffer, p);    /* Character offset */
uschar *bp = cmd_buffer;
uschar *be = cmd_buffer + pmax;

/* Scroll left if necessary */

if (cp + promptlen < scrolled)
  {
  if (cp == 0)     /* At LHS; reshow the entire line */
    {
    scrolled = 0;
    s_cls();
    s_printf("%s", prompt);
    for (i = 0; bp < be && i + promptlen <= window_width; i++)
      {
      GETCHARINC(k, bp, be);
      s_putc(k);
      }
    }
  else             /* Not at LHS; scroll left one character */
    {
    bp += line_soffset(cmd_buffer, be, scrolled - promptlen);
    while (cp + promptlen < scrolled)
      {
      scrolled--;
      s_hscroll(0, 0, window_width, 0, 1);
      s_move(0, 0);
      BACKCHAR(bp, cmd_buffer);
      GETCHAR(k, bp, be);
      s_putc(k);
      }
    }
  }

/* Otherwise, if the line has changed, reshow to the right of the change */

else if (changed)
  {
  bp += p;
  s_move(cp + promptlen - scrolled, 0);
  for (i = cp; bp < be && i + promptlen - scrolled <= window_width; i++)
    {
    GETCHARINC(k, bp, be);
    s_putc(k);
    }
  if (i + promptlen - scrolled < window_width) s_eraseright();
  }

/* Scroll right if necessary */

if (cp + promptlen + 1 - scrolled > window_width)
  {
  bp = cmd_buffer +
       line_soffset(cmd_buffer, be, scrolled + window_width + 1 - promptlen);
  while (cp + promptlen + 1 - scrolled > window_width)
    {
    scrolled++;
    s_hscroll(0, 0, window_width, 0, -1);
    if (bp < be)
      {
      s_move(window_width, 0);
      GETCHARINC(k, bp, be);
      s_putc(k);
      }
    }
  }
}



/*************************************************
*             Read Command Line                  *
*************************************************/

/* This has been highly hacked about to support wide characters; no doubt it
could be done more tidily.

Arguments:
  stack_flag    TRUE if up/down should use the command stack
  prompt        the prompt string

Returns:        nothing; the line is placed in cmd_buffer
*/

void
scrn_rdline(BOOL stack_flag, uschar *prompt)
{
BOOL interactend = FALSE;
/* These three variables must be signed */
int p = 0;                  /* Byte offset */
int cp = 0;                 /* Character offset */
int sp = cmd_stackptr;

main_rc = error_count = 0;  /* In case 40 errors without return to top level! */

if (sp == 0) sp = -1;
pmax = 0;                   /* Max byte offset */
scrolled = 0;

if (main_pendnl) sys_mprintf(msgs_fid, "\r\n"); else
  {
  s_selwindow(message_window, 0, 0);
  s_cls();
  }

s_move(0, 0);
s_printf("%s", prompt);
promptlen = s_x();

main_flush_interrupt();

while (!interactend)
  {
  int type;
  int key = sys_cmdkeystroke(&type);

  /* An interrupt terminates, leaving the flag set */

  if (main_escape_pressed)
    {
    cmd_buffer[pmax] = 0;
    return;
    }

  /* A data key causes a character to be added to the command line, which
  may have to scroll left. In wide character mode, code points greater than 127
  are treated as UTF-8. */

  if (type == ktype_data)
    {
    int i, n;  /* Must be signed */
    uschar buff[8];

    /* Fill up to cursor position with spaces. */

    if (p > pmax)
      {
      for (i = pmax; i < p; i++) cmd_buffer[i] = ' ';
      pmax = p;
      }

    /* Generate byte sequence for the key */

    if (allow_wide) n = ord2utf8(key, buff);
    else if (key > 255)
      {
      sys_beep();
      continue;
      }
    else
      {
      buff[0] = key;
      n = 1;
      }

    /* Make space, copy in the data, update end of line, and show */

    for (i = pmax - 1; i >= p; i--) cmd_buffer[i + n] = cmd_buffer[i];
    for (i = 0; i < n; i++) cmd_buffer[p++] = buff[i];
    cp++;
    pmax += n;
    reshow(p - n, TRUE, prompt);
    }

  /* A function key is translated in the same fashion as normal, then
  handled specially, except for the RETURN key, which is always taken
  as "end of command". */

  else
    {
    if (key == '\r') key = ka_ret;
      else if (key <= s_f_umax + max_fkey) key = key_table[key];
        else if (s_f_fbase <= key && key <= s_f_fmax)
          key = key_fixedtable[key-s_f_fbase];
        else key = ka_push;      /* LCOV_EXCL_LINE - unknown are ignored */

    switch (key)
      {
      case ka_ret:               /* RETURN pressed */
      cmd_buffer[pmax] = 0;
      interactend = TRUE;
      break;

      case ka_csu:               /* Cursor up */
      if (stack_flag)
        {
        if (sp == 0) sp = cmd_stackptr;
        if (sp > 0)
          {
          Ustrcpy(cmd_buffer, cmd_stack[--sp]);     /* Commands saved */
          p = Ustrlen(cmd_buffer);                  /* from .nerc may */
          while (p > 0 && cmd_buffer[p-1] == '\n')  /* end with \n. */
            cmd_buffer[--p] = 0;                    /* LCOV_EXCL_LINE */
          pmax = p;
          cp = line_charcount(cmd_buffer, p);

          if (cp > (int)(window_width - promptlen))
            {
            /* LCOV_EXCL_START - not currently tested */
            scrolled = cp - window_width/2;
            s_cls();
            reshow(line_soffset(cmd_buffer,
                                cmd_buffer + pmax,
                                scrolled - promptlen), TRUE, prompt);
            /* LCOV_EXCL_STOP */
            }
          else
            {
            scrolled = BIGNUMBER;
            reshow(0, TRUE, prompt);
            }
          }
        }
      break;

      case ka_csd:               /* Cursor down */
      if (stack_flag)
        {
        if (sp >= 0)
          {
          if (++sp >= cmd_stackptr) sp = 0;
          Ustrcpy(cmd_buffer, cmd_stack[sp]);
          p = pmax = Ustrlen(cmd_buffer);
          cp = line_charcount(cmd_buffer, p);
          if (cp > (int)(window_width - promptlen))
            {
            /* LCOV_EXCL_START - not currently tested */
            scrolled = cp - window_width/2;
            s_cls();
            reshow(line_soffset(cmd_buffer,
                                cmd_buffer + pmax,
                                scrolled - promptlen), TRUE, prompt);
            /* LCOV_EXCL_STOP */
            }
          else
            {
            scrolled = BIGNUMBER;
            reshow(0, TRUE, prompt);
            }
          }
        }
      break;

      case ka_csl:               /* Cursor left */
      if (p > 0)
        {
        cp--;
        BACKCHAROFFSET(p, cmd_buffer);
        reshow(p, FALSE, prompt);
        }
      else if (scrolled != 0)
        {
        /* LCOV_EXCL_START - needs long prompt in small window */
        scrolled = promptlen + 1;
        reshow(p, FALSE, prompt);
        /* LCOV_EXCL_STOP */
        }
      break;

      case ka_cswl:             /* Cursor word left */
      if (p > 0)
        {
        for (;;)
          {
          cp--;
          BACKCHAROFFSET(p, cmd_buffer);
          if (p <= 0 || (ch_tab[cmd_buffer[p]] & ch_word) != 0) break;
          }
        while (p > 0)
          {
          if ((ch_tab[cmd_buffer[p]] & ch_word) == 0) break;
          cp--;
          BACKCHAROFFSET(p, cmd_buffer);
          }
        if ((ch_tab[cmd_buffer[p]] & ch_word) == 0)
          {
          cp++;
          SKIPCHAROFFSET(p, cmd_buffer, pmax);
          }
        }
      else if (scrolled != 0) scrolled = promptlen + 1;  /* LCOV_EXCL_LINE */
      reshow(p, FALSE, prompt);
      break;

      case ka_cstl:             /* Cursor text left */
      if (scrolled < promptlen) p = cp = 0; else
        {
        cp = scrolled - promptlen;
        p = line_soffset(cmd_buffer, cmd_buffer + pmax, cp);
        }
      break;

      case ka_cstr:             /* Cursor text right */
      cp = scrolled + window_width - promptlen;
      p = line_soffset(cmd_buffer, cmd_buffer + pmax, cp);
      if (p > pmax)
        {
        /* LCOV_EXCL_START */
        p = pmax;
        cp = line_charcount(cmd_buffer, p);
        /* LCOV_EXCL_STOP */
        }
      break;

      case ka_csls:             /* cursor to true line start */
      p = cp = 0;
      scrolled = promptlen+1;
      reshow(p, FALSE, prompt);
      break;

      case ka_csle:             /* cursor to true line end */
      p = pmax;
      cp = line_charcount(cmd_buffer, p);
      reshow(p, FALSE, prompt);
      break;

      case ka_csr:              /* Cursor right */
      cp++;
      SKIPCHAROFFSET(p, cmd_buffer, pmax);
      reshow(p, FALSE, prompt);
      break;

      case ka_cswr:             /* Cursor word right */
      while (p < pmax && (ch_tab[cmd_buffer[p]] & ch_word) != 0)
        {
        p++;
        cp++;
        }
      while (p < pmax && (ch_tab[cmd_buffer[p]] & ch_word) == 0)
        {
        cp++;
        SKIPCHAROFFSET(p, cmd_buffer, pmax);
        }
      reshow(p, FALSE, prompt);
      break;

      /* Tab is now used for file name completion */
      case ka_cstab:
        {
        int oldp = p;
        p = sys_fcomplete(p, &pmax);
        cp = line_charcount(cmd_buffer, p);
        reshow(oldp, TRUE, prompt);
        }
      break;

      case ka_csptab:             /* previous tab */
      do { cp--; }  while (cp % 8 != 0);
      if (cp <= 0) { p = 0; scrolled = promptlen + 1; }
        else p = line_soffset(cmd_buffer, cmd_buffer + pmax, cp);
      reshow(p, FALSE, prompt);
      break;

      case ka_dp:               /* Delete previous */
      if (p > 0)
        {
        int n;
        int pp = p;
        cp--;
        BACKCHAROFFSET(p, cmd_buffer);
        n = pp - p;
        for (int i = pp; i < pmax; i++) cmd_buffer[i-n] = cmd_buffer[i];
        pmax -= n;
        if (cp + promptlen == scrolled && scrolled > 0) scrolled--;
        reshow(p, TRUE, prompt);
        }
      /* LCOV_EXCL_START - needs long prompt in small window */
      else if (scrolled != 0)
        {
        scrolled = promptlen + 1;
        reshow(p, FALSE, prompt);
        }
      /* LCOV_EXCL_STOP */
      break;

      case ka_dc:               /* Delete current */
      if (p < pmax)
        {
        int n;
        int pp = p;
        SKIPCHAROFFSET(pp, cmd_buffer, pmax);
        n = pp - p;
        for (int i = p; i < pmax - n; i++) cmd_buffer[i] = cmd_buffer[i+n];
        pmax -= n;
        reshow(p, TRUE, prompt);
        }
      break;

      case ka_dar:              /* Delete all right */
      pmax = p;
      s_eraseright();
      break;

      case ka_dtwl:             /* Delete to word left */
      if (p >= pmax)
        {
        p = pmax;
        cp = line_charcount(cmd_buffer, p);
        }
      if (p > 0)
        {
        int pp = p;
        for (;;)
          {
          cp--;
          BACKCHAROFFSET(p, cmd_buffer);
          if (p <= 0 || (ch_tab[cmd_buffer[p]] & ch_word) != 0) break;
          }
        while (p > 0)
          {
          if ((ch_tab[cmd_buffer[p]] & ch_word) == 0) break;
          cp--;
          BACKCHAROFFSET(p, cmd_buffer);
          }
        if ((ch_tab[cmd_buffer[p]] & ch_word) == 0)
          {
          cp++;
          SKIPCHAROFFSET(p, cmd_buffer, pmax);
          }
        if (pp > p)
          {
          int count = pmax - pp;
          for (int i = 0; i < count; i++) cmd_buffer[p+i] = cmd_buffer[pp+i];
          pmax -= (pp - p);
          if (scrolled > 0 && p + promptlen < scrolled)
            {
            /* LCOV_EXCL_START */
            if (p == 0) scrolled = promptlen + 1;
              else scrolled -= scrolled - (p + promptlen);
            /* LCOV_EXCL_STOP */
            }
          reshow(p, TRUE, prompt);
          }
        }
      break;

      case ka_dtwr:             /* Delete to word right */
        {
        int pp = p;
        while (pp < pmax && (ch_tab[cmd_buffer[pp]] & ch_word) != 0)
          {
          pp++;
          }
        while (pp < pmax && (ch_tab[cmd_buffer[pp]] & ch_word) == 0)
          {
          SKIPCHAROFFSET(pp, cmd_buffer, pmax);
          }
        if (pp > p)
          {
          int count = pmax - pp;
          for (int i = 0; i < count; i++) cmd_buffer[p+i] = cmd_buffer[pp+i];
          pmax -= (pp - p);
          reshow(p, TRUE, prompt);
          }
        }
      break;

      case ka_dal:              /* Delete all left */
        {
        int j = 0;
        s_cls();
        s_printf("%s", prompt);
        for (int i = p; i < pmax; i++) cmd_buffer[j++] = cmd_buffer[i];
        pmax = j;
        p = cp = scrolled = 0;
        reshow(p, TRUE, prompt);
        }
      break;

      case ka_dl:               /* Delete line */
      s_cls();
      s_printf("%s", prompt);
      p = cp = pmax = scrolled = 0;
      reshow(p, TRUE, prompt);
      break;

      default:        /* Deal with the use of function keystrings */
      if (1 <= key  && key <= max_keystring)
        {
        uschar *keydata = main_keystrings[key];
        if (keydata != NULL)
          {
          s_cls();
          s_printf("E> %s", keydata);
          Ustrcpy(cmd_buffer, keydata);
          interactend = TRUE;
          }
        }
      break;
      }
    }

  /* Always make cursor correct before looping */

  s_move(cp + promptlen - scrolled, 0);
  }

/* Cursor is on a printing line */

main_pendnl = TRUE;
}

/* End of escrnrdl.c */
