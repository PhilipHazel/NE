/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */


/* This file contains code for obeying commands: Part IV */


#include "ehdr.h"
#include "cmdhdr.h"
#include "keyhdr.h"




/*************************************************
*              The SAVE command                  *
*************************************************/

/* The main function is also used by the WRITE command. For SAVE, if
successful, the buffer's name is changed if a new name was given and the output
file is not completely released. For WRITE, the file is unrelated to the
buffer; the name is not changed, and the file is closed.

Arguments:
  cmd        the command structure
  saveflag   if TRUE (SAVE cmd), change buffer name if name given
  line       first line to write
  last       last line to write or NULL for eof

Returns:     a done_xxx value
*/

static int
savew(cmdstr *cmd, BOOL saveflag, linestr *line, linestr *last)
{
int type = -1;
int yield = done_continue;
FILE *fid;
BOOL changename = saveflag;
uschar *alias, *name, *savealias;

/* Now do the writing */

if ((cmd->flags & cmdf_arg1) == 0)        /* no string */
  {
  type = cmd_confirmoutput(main_filealias, FALSE, FALSE, -1, &alias);
  if (type == 0)  /* Yes; always given when not interactive */
    {
    name = main_filename;
    alias = main_filealias;
    changename = FALSE;
    }
  /* LCOV_EXCL_START */
  else if (type == 4) name = alias;                   /* New name */
    else { main_repaint = TRUE; return done_error; }  /* No, Stop, Discard */
  /* LCOV_EXCL_STOP */
  }

else alias = name = (cmd->arg1.string)->text;     /* string supplied */

/* Open the output appropriately */

if (name == NULL || name[0] == 0)
  {
  error_moan(59, currentbuffer->bufferno);
  return done_error;
  }
fid = sys_fopen(name, US"w");

if (main_screenmode) error_printf("Writing %s\n", alias);

if (fid == NULL)
  {
  error_moan(5, name, "writing", strerror(errno));
  return done_error;
  }

/* If new name, reset the buffer's files (cf NAME) */

if (changename)
  {
  store_free(main_filealias);
  store_free(main_filename);
  main_filealias = store_copystring(alias);
  main_filename = store_copystring(alias);
  currentbuffer->filename = main_filename;
  currentbuffer->filealias = main_filealias;
  main_drawgraticules |= dg_bottom;
  }

/* Now write the file */

savealias = main_filealias;
main_filealias = alias;       /* for messages from sys_outputline */

while ((line->flags & lf_eof) == 0)
  {
  int rc = file_writeline(line, fid);
  if (rc < 0)
    {
    /* LCOV_EXCL_START - I/O error */
    error_moan(37, alias, strerror(errno));
    return done_error;
    /* LCOV_EXCL_STOP */
    }
  else if (rc == 0) yield = done_error;   /* failed binary */
  if (line == last) break;
  line = line->next;
  }

main_filealias = savealias;   /* restore real name */
fclose(fid);

if (saveflag)
  {
  if (yield == done_continue)   /* If no binary errors */
    {
    main_filechanged = FALSE;
    currentbuffer->changed = FALSE;
    currentbuffer->saved = TRUE;
    }
  }

if (yield == done_continue) main_nowait = TRUE;
return yield;
}

int e_save(cmdstr *cmd) { return savew(cmd, TRUE, main_top, NULL); }



/*************************************************
*              The SET command                   *
*************************************************/

int
e_set(cmdstr *cmd)
{
switch (cmd->misc)
  {
  case set_autovscroll:
  main_vcursorscroll = cmd->arg1.value;
  break;

  case set_autovmousescroll:
  main_vmousescroll = cmd->arg1.value;
  break;

  case set_splitscrollrow:
  main_ilinevalue = cmd->arg1.value;
  break;

  case set_oldcommentstyle:
  main_oldcomment = TRUE;
  break;

  case set_newcommentstyle:
  main_oldcomment = FALSE;
  break;
  }
return done_continue;
}



/*************************************************
*            The SHOW command                    *
*************************************************/

/* Subroutine to check for filling the screen. */

static void
check_screen_lines(usint needed, usint *count)
{
if (!main_screenOK) return;   /* Not in screen mode */
/* LCOV_EXCL_START */
if (screen_max_row - *count < needed)
  {
  int c;
  error_printf("Press RETURN to continue ");
  error_printflush();
  while ((c = fgetc(kbd_fid)) != '\n' && c != '\r');
  error_printf("\r                        \r");
  *count = 0;
  }
/* LCOV_EXCL_STOP */
}


/* Generate tab setting text */

static const char *
tabsetting(void)
{
if (!main_tabin && !main_tabout) return "-notabs";
if (main_tabin && main_tabout) return " -tabin -tabout";
if (main_tabin) return main_tabflag? "  -tabs" : " -tabin";
return "-tabout";
}


/* Key display subroutine. */

static void
showkeysub(int type, usint *acount)
{
uschar *keychars = US"abcdefghijklmnopqrstuvwxyz[\\]^_";
int offset = 0;
int end = 31;
int f = 0;
int spextra = 0;
int flip = FALSE;

check_screen_lines(4, acount);
*acount += 2;

switch(type)
  {
  case show_ckeys:
  error_printf("\nCTRL KEYS\n");
  break;

  case show_fkeys:
  error_printf("\nFUNCTION KEYS\n");
  end = max_fkey;
  offset = s_f_umax;
  f = 1;
  break;

  case show_keystrings:
  error_printf("\nFUNCTION KEYSTRINGS\n");
  end = max_keystring;
  f = 2;
  spextra = 3;
  break;

  case show_actions:
  error_printf("\nKEY ACTIONS\n");
  end = key_actnamecount;
  f = 4;
  break;

  default:
  error_printf("\nEXTRA KEYS\n");
  offset = s_f_ubase - 1;
  end = s_f_umax - s_f_ubase + 1;
  f = 3;
  }

for (int i = 1; i <= end; i++)
  {
  int ba = TRUE;
  int spused = spextra;
  int action = (f==4)? key_actnames[i-1].code :
               (f==2)? i : key_table[i+offset];

  if (action != 0)
    {
    uschar *s;
    if (1 <= action && action <= max_keystring)
      {
      s = main_keystrings[action];
      ba = FALSE;
      }
    else s = key_actionnames[action - max_keystring - 1];

    if (s != NULL)
      {
      int spaces;

      check_screen_lines(2, acount);
      switch (f)
        {
        case 0: error_printf("ctrl/%c ", keychars[i-1]); break;
        case 1: error_printf("fkey %d %s", i, i<10? " ":""); break;
        case 2: error_printf("keystring %d %s",action,action<10? " ":""); break;

        case 3:
          {
          uschar *sp = US"    ";
          int key = i + offset;

          if ((key_specialmap[(i-1)%4] & (1L << (i-1)/4)) == 0)
            {
            ba = TRUE;
            if (key == s_f_bsp) s = US"same as ctrl/h";
            else if (key == s_f_ret) s = US"same as ctrl/m";
            else if (key == s_f_tab) s = US"same as ctrl/i";
            else continue;
            }

          if ((key & s_f_shiftbit) != 0) { error_printf("s/"); sp += 2; }
          if ((key & s_f_ctrlbit)  != 0) { error_printf("c/"); sp += 2; }
          error_printf("%s%s", key_specialnames[(key - s_f_ubase) >> 2], sp);
          }
        break;

        case 4: error_printf("%-6s ", key_actnames[i-1].name); break;
        }

      if (ba) error_printf("%s", s); else
        {
        if (f==2 || (f==1 && i==action))
          {
          error_printf("\"%s\"", s);
          spused += 2;
          }
        else
          {
          error_printf("%s(%d)\"%s\"", action<10? " ":"", action, s);
          spused += 6;
          }
        }

      spaces = 28 - Ustrlen(s) - spused;
      if (flip || spaces <= 0)
        {
        error_printf("\n");
        *acount += 1;
        flip = FALSE;
        }
      else
        {
        int j;
        for (j = 1; j <= spaces; j++) error_printf(" ");
        flip = TRUE;
        }
      }
    }
  }

if (flip)
  {
  error_printf("\n");
  *acount += 1;
  }

/* After extra keys, show any that are system or terminal specific. */

if (f == 3) sys_specialnotes(acount, check_screen_lines);
}


/* SHOW command external function */

int
e_show(cmdstr *cmd)
{
BOOL allsettings = FALSE;
usint count = 0;

switch (cmd->misc)
  {
  case show_keystrings:
  case show_ckeys:
  case show_fkeys:
  case show_xkeys:
  case show_actions:
  showkeysub(cmd->misc, &count);
  break;

  case show_allkeys:
  showkeysub(show_ckeys, &count);
  showkeysub(show_xkeys, &count);
  showkeysub(show_fkeys, &count);
  break;

  case show_buffers:
    {
    bufferstr *b = main_bufferchain;
    currentbuffer->changed = main_filechanged;
    currentbuffer->linecount = main_linecount;

    /* The linecount field includes the EOF line, so substract one to get the 
    number of data lines. */

    while (b != NULL)
      {
      uschar *name = b->filealias;
      uschar *changed = b->changed?  US"(modified)" : US"          ";
      if (name == NULL || name[0] == 0) name = US"<unnamed>";
      error_printf("Buffer %d  %5d  lines %s  %s\n",
        b->bufferno, b->linecount - 1, changed, name);
      b = b->next;
      }

    if (cut_buffer != NULL)
      {
      linestr *p = cut_buffer;
      uschar *pastext = cut_pasted? US"(pasted)  " : US"          ";
      uschar *type = (cut_type == cuttype_text)? US"<text>" : US"<rectangle>";
      uschar *pad;
      int n = 0;

      while (p != NULL) { n++; p = p->next; }
      pad = (n < 10)? US"    " : (n < 100)? US"   " : (n < 1000)? US"  " :
        (n < 10000)? US" " : US"";
      error_printf("Cut buffer%s%d  lines %s  %s\n", pad, n, pastext, type);
      }
    }
  break;

  case show_wordchars:
    {
    error_printf("Wordchars:");
    for (int i = 0; i < 256; i++) if ((ch_tab[i] & ch_word) != 0)
      {
      int first = i;
      error_printf(" %c", i);
      while (i < 256 && (ch_tab[i+1] & ch_word) != 0) i++;
      if (first != i) error_printf("-%c", i);
      }
    error_printf("\n");
    }
  break;

  case show_wordcount:
    {
    long int lc = -1;                   /* allow for eof "line" */
    long int wc = 0, bc = 0, cc = 0;
    int w;
    uschar buff[32];

    for (linestr *line = main_top; line != NULL; line = line->next)
      {
      uschar *p, *pe;
      int len = line->len;

      lc++;
      if (main_interrupted(ci_scan)) return done_error;
      if (len == 0) continue;   /* Avoid issues if empty line has NULL ptr */

      p = line->text;
      pe = p + len;
      bc += len;

      while (p < pe)
        {
        int k;
        while (p < pe)
          {
          cc++;
          GETCHARINC(k, p, pe);
          if (k != ' ' && k != '\t') { wc++; break; }
          }
        while (p < pe)
          {
          cc++;
          GETCHARINC(k, p, pe);
          if (k == ' ' || k == '\t') break;
          }
        }
      }

    /* Largest must be either line count or byte count. */

    sprintf(CS buff, "%ld", (lc > bc)? lc : bc);
    w = Ustrlen(buff);
    error_printf("%*ld line%s\n"
                 "%*ld word%s (space/tab separated)\n",
                 w, lc, (lc==1? "":"s"),
                 w, wc, (wc==1? "":"s"));
    if (allow_wide) error_printf("%*ld character%s (excluding line endings)\n",
                 w, cc, (cc==1? "":"s"));
    error_printf("%*ld byte%s (excluding line endings)\n",
                 w, bc, (bc==1? "":"s"));
    }
  break;

  /* LCOV_EXCL_START */
  case show_version:
  error_printf("NE %s %s using PCRE %s\n", version_string, version_date,
    version_pcre);
  break;
  /* LCOV_EXCL_STOP */

  case show_commands:
    {
    error_printf("\nCOMMANDS\n");
    for (int i = 0; i < cmd_listsize; i++)
      {
      error_printf(" %-14s", cmd_list[i]);
      if (i%5 == 4 || i == cmd_listsize - 1) error_printf("\n");
      }
    }
  break;

  case show_allsettings:
  allsettings = TRUE;
  /* Fall through */

  case show_settings:
    {
    error_printf("append:           %s\n", main_appendswitch? " on" : "off");
    error_printf("attn:             %s\n", main_attn? " on" : "off");
    if (main_screenmode || allsettings)
      {
      error_printf("autoalign:        %s\n", main_AutoAlign? " on" : "off");
      error_printf("autovmousescroll: %3d\n", main_vmousescroll);
      error_printf("autovscroll:      %3d\n", main_vcursorscroll);
      }
    error_printf("casematch:        %s\n", cmd_casematch? " on" : "off");
    error_printf("commentstyle:     %s\n", main_oldcomment? "old" : "new");
    error_printf("detrail output:   %s\n", main_detrail_output? " on" : "off");
    error_printf("eightbit:         %s\n", main_eightbit? " on" : "off");
    if (main_screenmode || allsettings)
      {
      error_printf("overstrike:       %s\n", main_overstrike? " on" : "off");
      error_printf("mouse:            %s\n", mouse_enable? " on" : "off");
      }
    error_printf("prompt:           %s\n", currentbuffer->noprompt? "off" : " on");
    error_printf("readonly:         %s\n", main_readonly? " on" : "off");
    if (main_screenmode || allsettings)
      error_printf("splitscrollrow:   %3d\n", main_ilinevalue);
    error_printf("tab setting:  %s\n", tabsetting());
    if (!main_screenmode || allsettings)
      error_printf("verify:           %s\n", main_verify? " on" : "off");
    error_printf("warn:             %s\n", main_warnings? " on" : "off");
    error_printf("widechars:        %s\n", allow_wide? " on" : "off");
    }
  break;
  }

return done_wait;    /* indicate output produced */
}



/*************************************************
*            The STOP command                    *
*************************************************/

/* If there are modified, unsaved buffers, proceed only if the user allows it,
or if non-interactive. */

/* LCOV_EXCL_START - not used in standard tests */

int
e_stop(cmdstr *cmd)
{
(void)cmd;

if (main_interactive)
  {
  bufferstr *b = main_bufferchain;
  bufferstr *last = NULL;
  int count = 0;

  currentbuffer->changed = main_filechanged;  /* make up-to-date */

  while (b != NULL)
    {
    if (b != currentbuffer && b->changed)
      {
      count++;
      last = b;
      }
    b = b->next;
    }

  if (count > 0 && main_warnings)
    {
    uschar buff[100];
    if (count > 1)
      sprintf(CS buff, "Some buffers have been modified but not saved.\n");
    else if (last->filealias == NULL || (last->filealias)[0] == 0)
      sprintf(CS buff, "Buffer %d has been modified but not saved.\n", last->bufferno);
    else
      sprintf(CS buff, "Buffer %d (%s) has been modified but not saved.\n",
        last->bufferno, last->filealias);
    error_printf("%s", buff);
    if (!cmd_yesno("Continue with STOP (QUIT) command (Y/N)? ")) return done_error;
    }
  }

main_rc = 8;
return done_finish;
}
/* LCOV_EXCL_STOP */



/*************************************************
*            The SUBCHAR command                 *
*************************************************/

int
e_subchar(cmdstr *cmd)
{
screen_subchar = cmd->arg1.value;
return done_continue;
}



/*************************************************
*              The T and TL commands             *
*************************************************/

int
e_ttl(cmdstr *cmd)
{
int n = cmd->arg1.value;
BOOL flag = cmd->misc;
uschar *hexlist = US"0123456789ABCDEF";
linestr *line = main_current;

if (n < 0) n = BIGNUMBER;    /* -ve comes from *, meaning "to eof" */

for (int i = 0; i < n; i++)
  {
  int len = line->len;
  uschar *p = line->text;
  BOOL nonprinters = FALSE;

  if (main_interrupted(ci_type)) return done_error;
  if ((line->flags & lf_eof) != 0) break;

  if (flag)
    {
    if (line->key > 0) error_printf("%4d  ", line->key);
      else error_printf("****  ");
    }

  for (int j = 0; j < len; j++)
    {
    int c = p[j];
    if (!isprint(c))
      {
      error_printf("%c", hexlist[(c & 0xf0) >> 4]);
      nonprinters = TRUE;
      }
    else error_printf("%c", c);
    }
  error_printf("\n");

  if (nonprinters)
    {
    if (flag) error_printf("      ");
    for (int j = 0; j < len; j++)
      {
      int c = p[j];
      if (!isprint(c)) error_printf("%c", hexlist[c & 0x0f]);
        else error_printf(" ");
      }
    error_printf("\n");
    }

  line = line->next;
  }

return done_wait;
}



/*************************************************
*              The TITLE command                 *
*************************************************/

int
e_title(cmdstr *cmd)
{
uschar *s = (cmd->arg1.string)->text;
if (main_filealias != main_filename) store_free(main_filealias);
currentbuffer->filealias = main_filealias = store_copystring(s);
main_drawgraticules |= dg_bottom;
return done_continue;
}



/*************************************************
*            The TOPLINE command                 *
*************************************************/

int
e_topline(cmdstr *cmd)
{
(void)cmd;
scrn_hint(sh_topline, 0, main_current);
return done_continue;
}



/*************************************************
*           The UNDELETE command                 *
*************************************************/

int
e_undelete(cmdstr *cmd)
{
(void)cmd;
if (main_undelete != NULL)
  {
  /* Handle single character undelete. Each character is preceded by a T/F byte
  where TRUE means it was a "forward" delete. */

  if ((main_undelete->flags & lf_udch) != 0)
    {
    uschar *p = main_undelete->text + main_undelete->len;
    uschar *pe = p;

    BACKCHAR(p, main_undelete->text);
    line_insertbytes(main_current, cursor_col, -1, p, pe - p, 0);
    main_current->flags |= lf_shn;

    if (!*(--p)) cursor_col++;      /* Advance cursor for a backwards delete */
    main_undelete->len -= pe - p;

    if (main_undelete->len <= 0)    /* Used up all deleted characters */
      {
      linestr *next = main_undelete->next;
      store_free(main_undelete->text);
      store_free(main_undelete);
      main_undeletecount--;
      main_undelete = next;
      if (next != NULL) next->prev = NULL;
        else main_lastundelete = NULL;
      }
    }

  /* Handle line undelete */
  else
    {
    linestr *new = main_undelete;
    linestr *prev = main_current->prev;

    main_undelete = new->next;
    if (main_undelete == NULL) main_lastundelete = NULL;
      else main_undelete->prev = NULL;
    main_undeletecount--;

    if (prev == NULL) main_top = new; else prev->next = new;
    new->prev = prev;
    new->next = main_current;
    main_current->prev = new;
    main_current = new;          /* Put cursor back on inserted line */
    cursor_col = 0;
    main_linecount++;
    if (main_screenOK) scrn_hint(sh_insert, 1, NULL);
    cmd_refresh = TRUE;
    }

  cmd_recordchanged(main_current, cursor_col);
  }

return done_continue;
}



/*************************************************
*            The UNFORMAT command                *
*************************************************/

int
e_unformat(cmdstr *cmd)
{
(void)cmd;
if ((main_current->flags & lf_eof) == 0)
  {
  line_formatpara(TRUE);
  cmd_refresh = TRUE;
  }
return done_continue;
}



/*************************************************
*             The VERIFY command                 *
*************************************************/

int
e_verify(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg1) != 0) main_verify = cmd->arg1.value;
  else main_verify = !main_verify;
if (main_verify && !main_shownlogo)
  {
  error_printf("NE %s %s using PCRE %s\n", version_string, version_date,
    version_pcre);
  main_shownlogo = TRUE;
  }
return done_continue;
}



/*************************************************
*            The W (windup) command              *
*************************************************/

int
e_w(cmdstr *cmd)
{
int count = 0;
bufferstr *thisbuffer = currentbuffer;

/* Warn if data in the cut buffer has not been pasted; prompt for permission to
continue (if non-interactive, answer will always be 'yes'). */

if (!cut_pasted && cut_buffer != NULL &&
     (cut_buffer->len != 0 || cut_buffer->next != NULL) && main_warnings)
  {
  if (!cut_overwrite(US"Continue with W command (Y/N)? ")) return done_error;
  }

/* Cycle through all the buffers. Since some may get deleted on the way, first
find out how many there are. */

do
  {
  count++;
  thisbuffer = (thisbuffer->next == NULL)?
    main_bufferchain : thisbuffer->next;
  }
while (currentbuffer != thisbuffer);

while (count-- > 0)
  {
  BOOL writeneeded = FALSE;
  bufferstr *nextbuffer = (currentbuffer->next == NULL)?
    main_bufferchain : currentbuffer->next;
  int n = currentbuffer->bufferno;
  uschar *newname = NULL;

  if (main_filechanged)
    {
    int x = cmd_confirmoutput(main_filealias, TRUE, TRUE,
      ((currentbuffer == nextbuffer && n == 0)? -1 : n), &newname);

    switch (x)
      {
      case 0:                /* Yes */
      writeneeded = TRUE;
      break;

      /* LCOV_EXCL_START - can never occur in non-interactive tests */
      case 1:
      if (main_screenOK) screen_forcecls = TRUE;
      return done_error;     /* No */

      case 2:
      return e_stop(cmd);    /* Stop */

      case 3:                /* Discard */
      break;

      case 4:                /* New file name */
      writeneeded = TRUE;
      main_drawgraticules |= dg_bottom;
      break;
      /* LCOV_EXCL_STOP */
      }
    }

  else if (currentbuffer == thisbuffer)
    {
    if (main_filealias == NULL)
      error_printf("No changes made to unnamed buffer %d", n);
        else error_printf("No changes made to %s", main_filealias);

    if (currentbuffer->saved)
      error_printf(" since last SAVE\n");
        else error_printf("\n");
    }

  if (writeneeded)
    {
    if (newname == NULL) newname = main_filename;
    if (main_screenOK) sys_mprintf(msgs_fid, "\r");

    /* If writing fails in an interactive run, return immediately with an
    error. Otherwise carry on, in the hope of writing the other buffers. An
    error message will have been given, so the return code should get set. */

    if (file_save(newname))
      {
      main_filechanged = FALSE;
      currentbuffer->saved = TRUE;
      }
    /* LCOV_EXCL_START - don't expect test failure */
    else if (main_interactive) return done_error;
    /* LCOV_EXCL_STOP */
    }

  init_selectbuffer(nextbuffer);
  }

return done_finish;
}



/*************************************************
*            The WARN command                    *
*************************************************/

int
e_warn(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg1) != 0) main_warnings = cmd->arg1.value;
  else main_warnings = !main_warnings;
return done_continue;
}



/*************************************************
*                The WHILE command               *
*************************************************/

/* This function is also called by UNTIL -- a switch in the command block
distinguishes. The absence of a search expression means "test for eof". */

int
e_while(cmdstr *cmd)
{
int yield = done_loop;
int misc = cmd->misc;
BOOL oldeoftrap = cmd_eoftrap;
BOOL prompt = (misc & if_prompt) != 0;
sestr *se = ((cmd->flags & cmdf_arg1) == 0)? NULL : cmd->arg1.se;

cmd_eoftrap = (se == NULL) &&
              (misc & (if_mark | if_eol | if_sol | if_sof)) == 0;

while (yield == done_loop)
  {
  yield = done_continue;
  while (yield == done_continue)
    {
    int match;
    if (main_interrupted(ci_loop)) return done_error;

    if (prompt)
      match = cmd_yesno("%s", cmd->arg1.string->text)? MATCH_OK : MATCH_FAILED;
    else if (se == NULL)
      {
      BOOL boolmatch;
      if ((misc & if_mark) != 0)
        boolmatch = (mark_type == mark_lines) && (mark_line == main_current);
      else if ((misc & if_eol) != 0)
        boolmatch = (line_offset(main_current, cursor_col) >= main_current->len);
      else if ((misc & if_sol) != 0)
        boolmatch = (cursor_col == 0);
      else if ((misc & if_sof) != 0)
        boolmatch = (cursor_col == 0 && main_current->prev == NULL);
      else boolmatch = (main_current->flags & lf_eof) != 0;
      match = boolmatch? MATCH_OK : MATCH_FAILED;
      }
    else
      {
      match_L = FALSE;
      match_leftpos = line_offset(main_current, cursor_col);
      match_rightpos = main_current->len;
      match = cmd_matchse(se, main_current);
      }
    if (misc >= if_unless) match = (match == MATCH_OK)? MATCH_FAILED : MATCH_OK;

    if (match == MATCH_OK) yield = cmd_obeyline(cmd->arg2.cmds); else break;
    }

  if (yield == done_loop || yield == done_break)
    {
    if (--cmd_breakloopcount > 0) break;
    if (yield == done_break) yield = done_continue;
    }
  }

if (yield == done_eof && !oldeoftrap) yield = done_continue;
cmd_eoftrap = oldeoftrap;
return yield;
}



/*************************************************
*               The WIDECHARS command            *
*************************************************/

int
e_wide(cmdstr *cmd)
{
allow_wide = ((cmd->flags & cmdf_arg1) != 0)? cmd->arg1.value : !allow_wide;
if (main_screenOK) screen_forcecls = TRUE;
return done_continue;
}



/*************************************************
*            The WORD command                    *
*************************************************/

/* Only ASCII characters are allowed in words, but this is checked when the
command is read, so we do not need to do it here. */

int
e_word(cmdstr *cmd)
{
uschar *p = cmd->arg1.string->text;
for (int i = 0; i < 256; i++) ch_tab[i] &= ~ch_word;
while (*p != 0)
  {
  int a = *p++;
  if (a == '\"') a = *p++;
  ch_tab[a] |= ch_word;
  if (*p == '-')
    {
    int b = *(++p);
    p++;
    for (int i = a+1; i <= b; i++) ch_tab[i] |= ch_word;
    }
  }
return done_continue;
}



/*************************************************
*            The WRITE command                   *
*************************************************/

/* Most of the code is shared with SAVE */

int
e_write(cmdstr *cmd)
{
linestr *first = main_top, *last = NULL;

if (mark_type == mark_lines)
  {
  if (line_checkabove(mark_line) > 0)
    { first = mark_line; last = main_current; }
  else
    { first = main_current; last = mark_line; }

  if (!mark_hold)
    {
    if (mark_line != NULL) mark_line->flags |= lf_shn;
    mark_type = mark_unset;
    mark_line = NULL;
    }
  }

return savew(cmd, FALSE, first, last);
}



/*************************************************
*           System Call Command                  *
*************************************************/

int
e_star(cmdstr *cmd)
{
uschar *s = NULL;

if ((cmd->flags & cmdf_arg1) != 0) s = (cmd->arg1.string)->text;

/* LCOV_EXCL_START */
if (main_screenOK && screen_suspend)
  {
  printf("\r\n");
  scrn_suspend();
  }
/* LCOV_EXCL_STOP */

if (s != NULL) if (system(CS s)){};  /* Fudge to avoid compiler warning */

return done_wait;
}

/* End of ee4.c */
