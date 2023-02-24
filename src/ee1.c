/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */


/* This file contains code for obeying commands: Part I */


#include "ehdr.h"
#include "cmdhdr.h"



/*************************************************
*         Single-character commands              *
*************************************************/

/* This handles all the single-character commands except '*'. */

int
e_singlechar(cmdstr *cmd)
{
if ((main_current->flags & lf_eof) != 0 && cmd->misc != '?')
  {
  char name[4];
  if (main_eoftrap) return done_eof;
  name[0] = cmd->misc;
  name[1] = 0;
  error_moan(30, "end of file", name);
  return done_error;
  }

switch(cmd->misc)
  {
  uschar *p, *pe;

  case '>':
  cursor_col += cmd->count;
  break;

  case '<':
  if (cmd->count > cursor_col) cursor_col = 0;
    else cursor_col -= cmd->count;
  break;

  case '?':
  line_verify(main_current, TRUE, !main_screenOK);
  break;

  case '#':
  line_deletech(main_current, cursor_col, cmd->count, TRUE);
  main_current->flags |= lf_shn;
  break;

  case '$':
  case '%':
  case '~':

  p = main_current->text + line_offset(main_current, cursor_col);
  pe = main_current->text + main_current->len;

  for (usint i = 0; i < cmd->count; i++)
    {
    if (p >= pe) break;
    if (*p < 128) *p = (cmd->misc == '$')? tolower(*p) :
                       (cmd->misc == '%')? toupper(*p) :
                       isupper(*p)? tolower(*p) : toupper(*p);
    SKIPCHAR(p, pe);
    }

  cursor_col += cmd->count;
  main_current->flags |= lf_shn;
  cmd_recordchanged(main_current, cursor_col);
  break;
  }

return done_continue;
}



/*************************************************
*              Obey NE Procedure                 *
*************************************************/

int
e_obeyproc(cmdstr *cmd)
{
procstr *p;

if (cmd_findproc(cmd->arg1.string->text, &p))
  {
  int yield;
  BOOL wasactive = (p->flags & pr_active) != 0;
  p->flags |= pr_active;
  yield = cmd_obeyline(p->body);
  if (!wasactive) p->flags &= ~pr_active;
  return yield;
  }
else
  {
  error_moan(48, cmd->arg1.string->text);
  return done_error;
  }
}



/*************************************************
*    Align and other operations on line group    *
*************************************************/

int
e_actongroup(cmdstr *cmd)
{
(void)cmd;
int misc = cmd->misc;
int oneline = (mark_type != mark_lines);

linestr *line = oneline? main_current : mark_line;
linestr *endline = main_current;

static uschar *cname[] = {
  US"align", US"dline", US"dright", US"dleft", US"closeup" };

if (!oneline && line_checkabove(mark_line) < 0)
  {
  line = main_current;
  endline = mark_line;
  }

if (!oneline && (!mark_hold || misc == lb_delete))
  { mark_type = mark_unset; mark_line = NULL; }

/* If operating on one line only, and it is the eof line, moan or trap. */

if ((line->flags & lf_eof) != 0)
  {
  if (cmd_eoftrap) return done_eof;
  error_moan(30, "end of file", cname[misc]);
  return done_error;
  }

/* Deal with deletion separately */

if (misc == lb_delete)
  {
  for (;;)
    {
    if ((line->flags & lf_eof) != 0) break; else
      {
      int done = (line == endline);
      line = line_delete(line, TRUE);
      if (done) break;
      }
    }
  cmd_refresh = TRUE;
  }

/* Deal with all other actions */

else for (;;)
  {
  int action;
  usint cursor_byte = line_offset(line, cursor_col);

  if ((line->flags & lf_eof) == 0) switch (misc)
    {
    case lb_alignp:
      {
      linestr *prevline = line->prev;
      cursor_col = 0;
      if (prevline != NULL)
        {
        usint i;
        uschar *p = prevline->text;
        for (i = 0; i < prevline->len; i++)
          if (p[i] != ' ') { cursor_col = i; break; }
        }
      }
    /* Fall through */

    case lb_align:
    line_leftalign(line, cursor_col, &action);
    break;

    case lb_eraseright:
    if (cursor_byte < line->len)
      line_deletech(line, cursor_col, line->len - cursor_byte, TRUE);
    break;

    case lb_eraseleft:
    line_deletech(line, cursor_col, cursor_col, FALSE);
    break;

    case lb_closeup:
      {
      int count = 0;
      uschar *p = line->text;
      for (usint i = cursor_byte; i < line->len; i++)
        if (p[i] == ' ') count++; else break;
      line_deletech(line, cursor_col, count, TRUE);
      }
    break;

    case lb_closeback:
      {
      int count = 0;
      uschar *p = line->text;
      for (int i = cursor_byte - 1; i >= 0; i--)
        if (p[i] == ' ') count++; else break;
      line_deletech(line, cursor_col - count, count, TRUE);
      cursor_col -= count;
      }
    break;
    }

  line->flags |= lf_shn;
  if (line == endline) break;
  line = line->next;
  }

main_current = line;
if (misc == lb_eraseleft || misc == lb_delete) cursor_col = 0;
return done_continue;
}



/*************************************************
*           The A, B & E commands                *
*************************************************/

 /* They all call the same function. */

int
e_abe(cmdstr *cmd)
{
int yield = done_continue;
int oldrmargin = main_rmargin;
int misc = cmd->misc;
int matchrc;
BOOL stringsearch, REreplace;

sestr *se = cmd->arg1.se;
qsstr *nt = cmd->arg2.qs;

/* Complain if at eof, unless in until eof loop */

if ((main_current->flags & lf_eof))
  {
  if (cmd_eoftrap) return done_eof;
  error_moan(30, "End of file", "a, b, or e");
  return done_error;
  }

/* Deal with saving arguments or re-using old ones */

if ((cmd->flags & cmdf_arg1) != 0)
  {
  if (last_abese != NULL) cmd_freeblock((cmdblock *)last_abese);
  if (last_abent != NULL) cmd_freeblock((cmdblock *)last_abent);
  last_abese = cmd_copyblock((cmdblock *)se);
  last_abent = cmd_copyblock((cmdblock *)nt);
  }
else if (last_abese == NULL)
  {
  error_moan(16, "a, b, or e command");
  return done_error;
  }
else
  {
  se = last_abese;
  nt = last_abent;
  }

/* Final preparations */

stringsearch = (se->type == cb_qstype) &&
  ((((qsstr *)se)->flags & qsef_N) == 0);

if (!stringsearch && cursor_col != 0)
  { error_moan(40); return done_error; }

REreplace = (nt->flags & qsef_R) != 0;

match_L = FALSE;
if (main_rmargin < MAX_RMARGIN) main_rmargin = MAX_RMARGIN;

/* Search for given context */

match_leftpos = line_offset(main_current, cursor_col);
match_rightpos = main_current->len;

/* Take action according as matched or not */

if ((matchrc = cmd_matchse(se, main_current)) == MATCH_OK)
  {
  uschar *p = nt->text + 1;
  usint len = nt->length;

  /* Regular Expression Change: if required wild strings are unavailable, give
  error and exit. NB cmd_ReChange() sets cursor_col to a *byte* offset. */

  if (REreplace)
    main_current = cmd_ReChange(main_current, p, len, (nt->flags & qsef_X) != 0,
      misc == abe_e, misc == abe_a);

  /* Normal (not regex) change */

  else
    {
    if ((nt->flags & qsef_X) != 0)
      {
      p = nt->hexed;
      len /= 2;
      }

    if (misc == abe_e)
      {
      line_deletebytes(main_current, match_start, match_end-match_start, TRUE);
      line_insertbytes(main_current, -1, match_start, p, len, 0);
      cursor_col = match_start + len;    /* Byte offset */
      }
    else
      {
      line_insertbytes(main_current, -1,
        ((misc == abe_a)? match_end : match_start), p, len, 0);
      cursor_col = match_end + len;      /* Byte offset */
      }
    }

  /* Correct cursor_col from byte to char offset, and note the line has
  changed. */

  cursor_col = line_charcount(main_current->text, cursor_col);
  main_current->flags |= lf_shn;
  }

/* No match has been found or there was an error (message given) */

else
  {
  if (matchrc == MATCH_FAILED) error_moanqse(17, se);   /* not found */
  yield = done_error;
  }

/* Restore rmargin before exit */

main_rmargin = oldrmargin;
return yield;
}



/*************************************************
*           The ABANDON command                  *
*************************************************/

/* LCOV_EXCL_START - not in standard tests */

int
e_abandon(cmdstr *cmd)
{
(void)cmd;
main_rc = 8;
return done_finish;
}

/* LCOV_EXCL_STOP */



/*************************************************
*              The ATTN command                  *
*************************************************/

int
e_attn(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg1) != 0) main_attn = cmd->arg1.value;
  else main_attn = !main_attn;
if (main_attn && main_oneattn)
  {
  /* LCOV_EXCL_START - can never occur in standard tests */
  main_oneattn = FALSE;
  error_moan(23);
  return done_error;
  /* LCOV_EXCL_STOP */
  }
else return done_continue;
}



/*************************************************
*          The AUTOALIGN command                 *
*************************************************/

int
e_autoalign(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg1) != 0) main_AutoAlign = cmd->arg1.value;
  else main_AutoAlign = !main_AutoAlign;
main_drawgraticules |= dg_flags;
return done_continue;
}



/*************************************************
*             The BACK command                   *
*************************************************/

/* If the next back item is NULL, nothing has been changed; do nothing. If
the next back item is not the current line, use it. Otherwise move back to the
previous item, if there is one. */

int
e_back(cmdstr *cmd)
{
(void)cmd;
if (main_backlist[main_backnext].line != NULL)
  {
  linestr *line = main_top;

  if (main_backlist[main_backnext].line == main_current)
    main_backnext = (main_backnext == 0)? main_backtop : main_backnext - 1;

  /* Double-check that the line exists. */

  while (line != main_backlist[main_backnext].line)
    {
    if (line == NULL)
      {
      /* LCOV_EXCL_START */
      error_moan(62);      /* Internal error if line not found */
      return done_error;
      /* LCOV_EXCL_STOP */
      }
    line = line->next;
    }

  main_current = main_backlist[main_backnext].line;
  cursor_col = main_backlist[main_backnext].col;
  }
return done_continue;
}



/*************************************************
*             The BACKREGION command             *
*************************************************/

int
e_backregion(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg1) != 0)
  {
  main_backregionsize = cmd->arg1.value;
  if (main_backregionsize < 1) main_backregionsize = 1;
  }
return done_continue;
}



/*************************************************
*           The BACKUP command                   *
*************************************************/

int
e_backup(cmdstr *cmd)
{
switch(cmd->misc)
  {
  case backup_files:
  if ((cmd->flags & cmdf_arg1) != 0) main_backupfiles = cmd->arg1.value;
    else main_backupfiles = !main_backupfiles;
  break;
  }
return done_continue;
}



/*************************************************
*            The BEGINPAR command                *
*************************************************/

int
e_beginpar(cmdstr *cmd)
{
cmd_freeblock((cmdblock *)par_begin);
par_begin = cmd_copyblock((cmdblock *)cmd->arg1.se);
return done_continue;
}



/*************************************************
*             The BREAK command                  *
*************************************************/

int
e_break(cmdstr *cmd)
{
cmd_breakloopcount = ((cmd->flags & cmdf_arg1) != 0)? cmd->arg1.value : 1;
return done_break;
}



/*************************************************
*              The BUFFER command                *
*************************************************/

int
e_buffer(cmdstr *cmd)
{
bufferstr *new = main_bufferchain;

if ((cmd->flags & cmdf_arg1) != 0)
  {
  new = cmd_findbuffer(cmd->arg1.value);
  if (new == NULL)
    {
    error_moan(26, cmd->arg1.value);
    return done_error;
    }
  }

/* cycle backwards */

else if (cmd->misc)
  {
  while (new->next != currentbuffer)
    {
    if (new->next == NULL) break;
    new = new->next;
    }
  }

/* cycle forwards */

else if (currentbuffer->next != NULL) new = currentbuffer->next;

/* Cannot switch to a buffer from which commands are being read. */

if (new->commanding > 0)
  {
  error_moan(50, new->bufferno, "selected");
  return done_error;
  }

if (new != currentbuffer) init_selectbuffer(new);
return done_continue;
}



/*************************************************
*              The C command                     *
*************************************************/

int
e_c(cmdstr *cmd)
{
FILE *oldcfile = cmdin_fid;
linestr *oldcbufferline = cmd_cbufferline;
BOOL wasinteractive = main_interactive;
int oldclineno = cmd_clineno;
int oldblevel = cmd_bracount;
int oldonecommand = cmd_onecommand;
int yield = done_continue;
uschar *name = (cmd->arg1.string)->text;

cmdin_fid = sys_fopen(name, US"r");

if (cmdin_fid == NULL)
  {
  cmdin_fid = oldcfile;
  error_moan(5, name, "reading", strerror(errno));
  return done_error;
  }

/* Now obey the lines in the file. At EOF we return the most recent yield from
obeying a command line. */

cmd_cbufferline = NULL;
cmd_onecommand = main_interactive = FALSE;
cmd_clineno = 0;

for (;;)
  {
  int n;
  if (Ufgets(cmd_buffer, CMD_BUFFER_SIZE, cmdin_fid) == NULL) break;
  cmd_clineno++;
  n = Ustrlen(cmd_buffer);
  if (n > 0 && cmd_buffer[n-1] == '\n') cmd_buffer[n-1] = 0;
  yield = cmd_obey(cmd_buffer);

  if (yield == done_error)
    {
    error_printf("c command abandoned after obeying line %d of %s\n",
      cmd_clineno, name);
    break;
    }

  /* LCOV_EXCL_START */
  if (yield == done_wait)
    {
    if (main_screenOK)
      {
      scrn_rdline(FALSE, US"Press RETURN to continue ");
      error_printf("\n");
      }
    yield = done_continue;
    }
  /* LCOV_EXCL_STOP */

  /* Break for done_finish and done_eof, though the latter shouldn't happen
  here. */

  if (yield != done_continue && yield != done_break && yield != done_loop)
    break;
  }

fclose(cmdin_fid);
cmd_clineno = oldclineno;
cmd_cbufferline = oldcbufferline;
main_interactive = wasinteractive;
cmdin_fid = oldcfile;
cmd_bracount = oldblevel;
cmd_onecommand = oldonecommand;
return yield;
}



/*************************************************
*           The CASEMATCH command                *
*************************************************/

int
e_casematch(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg1) != 0) cmd_casematch = cmd->arg1.value;
  else cmd_casematch = !cmd_casematch;
main_drawgraticules |= dg_flags;
return done_continue;
}



/*************************************************
*           The C(D)BUFFER command               *
*************************************************/

int
e_cdbuffer(cmdstr *cmd)
{
linestr *oldcbufferline = cmd_cbufferline;
FILE *oldcfile = cmdin_fid;
BOOL wasinteractive = main_interactive;
int oldclineno = cmd_clineno;
int oldblevel = cmd_bracount;
int oldonecommand = cmd_onecommand;
int yield = done_continue;
linestr *line;
bufferstr *buffer = currentbuffer;

if (main_binary)
  {
  error_moan(61);
  return done_error;
  }

if ((cmd->flags & cmdf_arg1) != 0)
  {
  buffer = cmd_findbuffer(cmd->arg1.value);
  if (buffer == NULL)
    {
    error_moan(26, cmd->arg1.value);
    return done_error;
    }
  }

if (buffer == currentbuffer)
  {
  error_moan(69, buffer->bufferno);
  return done_error;
  }

/* Obey the commands */

cmdin_fid = NULL;
buffer->commanding++;
cmd_onecommand = main_interactive = FALSE;
cmd_clineno = 0;
line = buffer->top;

for (;;)
  {
  if (line == NULL || (line->flags & lf_eof) != 0) break;

  if (line->len > CMD_BUFFER_SIZE - 1)
    {
    error_moan(56);
    yield = done_error;
    }
  else
    {
    if (line->len > 0) memcpy(cmd_buffer, line->text, line->len);
    cmd_buffer[line->len] = 0;
    cmd_clineno++;
    cmd_cbufferline = line->next;
    yield = cmd_obey(cmd_buffer);
    }

  if (yield == done_error)
    {
    error_printf("** c%sbuffer command abandoned after obeying line %d of buffer %d\n",
      (cmd->misc == cbuffer_cd)? "d":"", cmd_clineno, buffer->bufferno);
    break;
    }

  /* LCOV_EXCL_START */
  if (yield == done_wait)
    {
    if (main_screenOK)
      {
      scrn_rdline(FALSE, US"Press RETURN to continue ");
      error_printf("\n");
      }
    yield = done_continue;
    }
  /* LCOV_EXCL_STOP */

  /* Break for done_finish and done_eof, though the latter shouldn't happen
  here. */

  if (yield != done_continue && yield != done_break && yield != done_loop)
    break;   /* LCOV_EXCL_LINE - not in standard tests */

  line = cmd_cbufferline;
  }

main_interactive = wasinteractive;
cmdin_fid = oldcfile;
cmd_clineno = oldclineno;
cmd_cbufferline = oldcbufferline;
cmd_bracount = oldblevel;
cmd_onecommand = oldonecommand;

/* This is no longer a buffer from which commands are being read. Mark it as 
unchanged, as its work is done and the presumption is that it can be discarded 
on exit if not explicitly saved. */

buffer->commanding--;
buffer->changed = FALSE;

/* If requested, delete the buffer by invoking a dbuffer command. */

if (cmd->misc == cbuffer_cd && yield != done_error) 
  return e_dbuffer(cmd);
   
return yield;
}



/*************************************************
*              The CENTRE command                *
*************************************************/

int
e_centre(cmdstr *cmd)
{
(void)cmd;
if ((main_current->flags & lf_eof) == 0)
  {
  uschar *p = main_current->text;
  int clen = line_charcount(p, main_current->len);
  int width = main_rmargin;
  int i = 0;
  int leading, count;

  /* Find start of data in line */

  while (i < clen && p[i] == ' ') i += 1;

  /* If margin disabled, use the remembered value */

  if (width > MAX_RMARGIN) width -= MAX_RMARGIN;

  /* Compute how many leading spaces we need */

  leading = (width - (clen - i))/2;

  /* Add or delete some */

  line_leftalign(main_current, leading, &count);
  if (count != 0) { main_current->flags |= lf_shn; main_filechanged = TRUE; }
  }

return done_continue;
}



/*************************************************
*                 The CL command                 *
*************************************************/

int
e_cl(cmdstr *cmd)
{
qsstr *qs = cmd->arg1.qs;
uschar *s = US"";
int slen = 0;
usint len;

if ((main_current->flags & lf_eof) != 0 ||
    (main_current->next->flags & lf_eof) != 0)
  {
  if (cmd_eoftrap) return done_eof;
  error_moan(30, "End of file", "cl");
  return done_error;
  }

/* Set up a glue string if (optionally) provided by the command. */

if ((cmd->flags & cmdf_arg1) != 0)
  {
  if ((qs->flags & qsef_X) == 0)
    {
    s = qs->text + 1;
    slen = qs->length;
    }
  else
    {
    s = qs->hexed;
    slen = qs->length / 2;
    }
  }

/* Inserting zero bytes at the cursor column, when it is past the actual
length of the line, causes the line to be filled up with spaces. */

len = main_current->len;
if (line_offset(main_current, cursor_col) > len)
  {
  line_insertbytes(main_current, cursor_col, -1, NULL, 0, 0);
  len = main_current->len;
  }

/* Join the lines, inserting slen spaces between them; that is room into which
we can copy the joining string. Then set the cursor column to the end of the
first line plus the joining string */

main_current = line_concat(main_current->next, slen);
memcpy(main_current->text + len, s, slen);
cursor_col = line_charcount(main_current->text, len + slen);

main_current->flags |= lf_shn;
cmd_refresh = TRUE;
return done_continue;
}



/*************************************************
*             The COMMENT command                *
*************************************************/

int
e_comment(cmdstr *cmd)
{
error_printf("%s\n", cmd->arg1.string->text);
return done_wait;
}



/*************************************************
*              The CPROC command                 *
*************************************************/

int
e_cproc(cmdstr *cmd)
{
procstr *p;
if (cmd_findproc(cmd->arg1.string->text, &p))
  {
  if ((p->flags & pr_active) != 0)
    {
    error_moan(47, cmd->arg1.string->text);
    return done_error;
    }
  else  /* A found procedure is always moved to the top of the list */
    {
    main_proclist = p->next;
    cmd_freeblock((cmdblock *)p);
    return done_continue;
    }
  }
else
  {
  error_moan(48, cmd->arg1.string->text);
  return done_error;
  }
}



/*************************************************
*             CUT, COPY & DMARKED                *
*************************************************/

static int
ccd(cmdstr *cmd, uschar *s)
{
(void)cmd;
linestr *line;
int type;

if (mark_type != mark_text && mark_type != mark_rect)
  {
  error_moan(41, s);
  return done_error;
  }

/* We unset the mark before actually doing anything. */

line = mark_line;
type = mark_type;
mark_type = mark_unset;
mark_line = NULL;

line->flags |= lf_shn;
if (cut_cut(line, mark_col, type, s[1] == 'o', s[0] == 'd'))
  {
  if (s[1] != 'o') cmd_refresh = TRUE;
  return done_continue;
  }
else return done_error;  /* LCOV_EXCL_LINE - only when interactive */
}

int e_cut(cmdstr *cmd)     { return ccd(cmd, US"cut"); }
int e_copy(cmdstr *cmd)    { return ccd(cmd, US"copy"); }
int e_dmarked(cmdstr *cmd) { return ccd(cmd, US"dmarked"); }



/*************************************************
*          The CUTSTYLE command                  *
*************************************************/

int
e_cutstyle(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg1) != 0) main_appendswitch = cmd->arg1.value;
  else main_appendswitch = !main_appendswitch;
main_drawgraticules |= dg_flags;
return done_continue;
}



/*************************************************
*            Cursor down/up commands             *
*************************************************/

int
e_csd(cmdstr *cmd)
{
(void)cmd;
linestr *next = main_current->next;
if (next == NULL)
  {
  error_moan(30, "end of file", "csd");
  return done_error;
  }
else
  {
  main_current = next;
  return done_continue;
  }
}

int
e_csu(cmdstr *cmd)
{
(void)cmd;
linestr *prev = main_current->prev;
if (prev == NULL)
  {
  error_moan(30, "start of file", "csu");
  return done_error;
  }
else
  {
  main_current = prev;
  return done_continue;
  }
}



/*************************************************
*           The DBUFFER command                  *
*************************************************/

int
e_dbuffer(cmdstr *cmd)
{
bufferstr *buffer, *deletebuffer, **link;

if ((cmd->flags & cmdf_arg1) != 0)
  {
  deletebuffer = cmd_findbuffer(cmd->arg1.value);
  if (deletebuffer == NULL)
    {
    error_moan(26, cmd->arg1.value);
    return done_error;
    }
  }
else deletebuffer = currentbuffer;

/* If in use as a command buffer, can't delete */

if (deletebuffer->commanding > 0)
  {
  error_moan(50, deletebuffer->bufferno, "deleted");
  return done_error;
  }

/* Actually do the deletion of a given buffer */

buffer = main_bufferchain;
link = &main_bufferchain;

while (buffer != deletebuffer)
  {
  link = &buffer->next;
  buffer = buffer->next;
  if (buffer == NULL)
    {
    /* LCOV_EXCL_START - internal error */
    error_moan(70, deletebuffer->bufferno);
    return done_error;
    /* LCOV_EXCL_STOP */
    }
  }

/* Empty the buffer */

if (!cmd_emptybuffer(buffer, US"DBUFFER")) return done_error;

/* If buffer is the only buffer, set it up as empty; otherwise,
select another buffer if it is current, and then wipe it out. */

if (buffer == main_bufferchain && buffer->next == NULL)
  {
  init_buffer(buffer, 0, store_copystring(US""), store_copystring(US""), NULL);
  currentbuffer = NULL;
  init_selectbuffer(buffer);
  }

else
  {
  bufferstr *next = buffer->next;
  if (next == NULL) next = main_bufferchain;
  if (buffer == currentbuffer) init_selectbuffer(next);
  *link = buffer->next;          /* disconnect from chain */
  store_free(buffer);
  if (main_bufferchain->next == NULL) main_drawgraticules |= dg_both;
  }

return done_continue;
}



/*************************************************
*                The DCUT command                *
*************************************************/

int
e_dcut(cmdstr *cmd)
{
(void)cmd;
while (cut_buffer != NULL)
  {
  linestr *next = cut_buffer->next;
  store_free(cut_buffer->text);
  store_free(cut_buffer);
  cut_buffer = next;
  }
cut_last = NULL;
cut_pasted = TRUE;
return done_continue;
}



/*************************************************
*              The DEBUG command                 *
*************************************************/

/* LCOV_EXCL_START */
int
e_debug(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg1) != 0) switch(cmd->arg1.value)
  {
  case debug_crash:
  *((int *)(-1)) = 0;
  break;

  case debug_exceedstore:
  (void) store_Xget(0xffffffffu);
  break;

  case debug_nullline:
  main_current = NULL;
  break;

  case debug_baderror:
  error_moan(4, "Cause disastrous error", "debug command", 0, 0, 0, 0, 0);
  break;
  }
else error_printf("Warning! Careless use of the debug command can damage your data\n");

return done_wait;
}
/* LCOV_EXCL_STOP */



/*************************************************
*              The DETRAIL command               *
*************************************************/

/* With no argument, removes trailing spaces from the current buffer. With
"output" as argument, sets flag for detrailing on output. */

int
e_detrail(cmdstr *cmd)
{
if (cmd->misc == detrail_buffer)
  {
  linestr *line = main_top;
  while ((line->flags & lf_eof) == 0)
    {
    uschar *s = line->text;
    uschar *t = s + line->len;
    while (t > s && t[-1] == ' ') t--;
    if (t - s < line->len)
      {
      line->len = t - s;
      main_filechanged = TRUE;
      }
    line = line->next;
    }
  }
else main_detrail_output = TRUE;
return done_continue;
}



/*************************************************
*              The DF command                    *
*************************************************/

int
e_df(cmdstr *cmd)
{
linestr *start = main_current;
int yield = e_f(cmd);
if (yield != done_continue) return yield;
while (start != main_current) start = line_delete(start, TRUE);
cmd_recordchanged(main_current, cursor_col);
cmd_refresh = TRUE;
return done_continue;
}



/*************************************************
*                The DREST command               *
*************************************************/

int
e_drest(cmdstr *cmd)
{
(void)cmd;

/* It should not be possible for the last line in a buffer not to be an EOF
line, but just in case... */

/* LCOV_EXCL_START */
if ((main_bottom->flags & lf_eof) == 0)
  {
  store_free(main_bottom->text);
  main_bottom->text = NULL;
  main_bottom->len = 0;
  main_bottom->flags |= lf_eof;
  }
/* LCOV_EXCL_STOP */

main_bottom->flags |= lf_shn;
scrn_hint(sh_topline, 0, NULL);

while ((main_current->flags & lf_eof) == 0)
  main_current = line_delete(main_current, FALSE);

cmd_recordchanged(main_current, cursor_col);
cmd_refresh = TRUE;

return done_continue;
}



/*************************************************
*          The DTA and DTB commands              *
*************************************************/

int
e_dtab(cmdstr *cmd)
{
int oldcol = cursor_col;
int yield = e_pab(cmd);
if (yield == done_continue)
  {
  int count = cursor_col - oldcol;
  line_deletech(main_current, cursor_col, count, FALSE);
  main_current->flags |= lf_shn;
  cursor_col -= count;
  }
return yield;
}



/*************************************************
*          DTWL & DTWR (delete to word)          *
*************************************************/

/* All word characters are ASCII (one-byte) characters. */

int
e_dtwl(cmdstr *cmd)
{
(void)cmd;
uschar *p = main_current->text;
int len = main_current->len;
int oldcursor = cursor_col;
int count, cursor_byte;

if (cursor_col == 0) return done_continue;

if ((main_current->flags & lf_eof) != 0)
  {
  error_moan(30, "End of file", "dtwl");
  return done_error;
  }

cursor_byte = line_offset(main_current, cursor_col);
if (cursor_byte >= len) cursor_byte = len;
while (--cursor_byte > 0 &&
  (ch_tab[(p[cursor_byte])] & ch_word) == 0);
while (cursor_byte > 0 &&
  (ch_tab[(p[cursor_byte])] & ch_word) != 0) cursor_byte--;
if ((ch_tab[(p[cursor_byte])] & ch_word) == 0) cursor_byte++;

cursor_col = line_charcount(main_current->text, cursor_byte);
count = oldcursor - cursor_col;
if (count > 0)
  {
  line_deletech(main_current, cursor_col, count, TRUE);
  main_current->flags |= lf_shn;
  }
return done_continue;
}


int
e_dtwr(cmdstr *cmd)
{
(void)cmd;
uschar *p = main_current->text;
int len = main_current->len;
int count, cursor_byte;

if ((main_current->flags & lf_eof) != 0)
  {
  error_moan(30, "End of file", "dtwr");
  return done_error;
  }

cursor_byte = line_offset(main_current, cursor_col);
if (cursor_byte >= len) cursor_byte = len;
while (cursor_byte < len && (ch_tab[(p[cursor_byte])] & ch_word) != 0) cursor_byte++;
while (cursor_byte < len && (ch_tab[(p[cursor_byte])] & ch_word) == 0) cursor_byte++;

count = line_charcount(main_current->text, cursor_byte) - cursor_col;
if (count > 0)
  {
  line_deletech(main_current, cursor_col, count, TRUE);
  main_current->flags |= lf_shn;
  }
return done_continue;
}



/*************************************************
*           The EIGHTBIT command                 *
*************************************************/

int
e_eightbit(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg1) != 0) main_eightbit = cmd->arg1.value;
  else main_eightbit = !main_eightbit;
screen_forcecls = TRUE;
return done_continue;
}



/*************************************************
*         The ENDPAR command                     *
*************************************************/

int
e_endpar(cmdstr *cmd)
{
cmd_freeblock((cmdblock *)par_end);
par_end = cmd_copyblock((cmdblock *)cmd->arg1.se);
return done_continue;
}

/* End of c.ee1 */
