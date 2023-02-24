/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */


/* This file contains code for obeying commands: Part III */


#include "ehdr.h"
#include "cmdhdr.h"
#include "keyhdr.h"
#include "shdr.h"



/*************************************************
*            The M command                       *
*************************************************/

int
e_m(cmdstr *cmd)
{
linestr *line = main_current;
int found = FALSE;
int n = cmd->arg1.value;

/* Zero means top of file */

if (n == 0)
  {
  line = main_top;
  found = TRUE;
  }

/* Negative means bottom of file */

else if (n < 0)
  {
  line = main_bottom;
  found = TRUE;
  }

/* Otherwise seek a numbered line. We first have to discover in which
direction we need to move. */

else
  {
  /* Advance to a non-inserted line or the end of the file. */

  while (line->key <= 0)
    {
    if (main_interrupted(ci_move)) return done_error;
    if ((line->flags & lf_eof) != 0) break;
    line = line->next;
    }

  /* Forwards search */

  if ((line->flags & lf_eof) == 0 && n > line->key) for (;;)
    {
    if (main_interrupted(ci_move)) return done_error;
    if (n == line->key) { found = TRUE; break; }
    if (n < line->key || (line->flags & lf_eof) != 0) break;
    line = line->next;
    }

  /* Backwards search */

  else for (;;)
    {
    if (main_interrupted(ci_move)) return done_error;
    if (n == line->key) { found = TRUE; break; }
    if ((line->key > 0 && n > line->key) || line->prev == NULL) break;
    line = line->prev;
    }
  }

if (found)
  {
  main_current = line;
  cursor_col = 0;
  return done_continue;
  }

else
  {
  error_moan(25, n);
  return done_error;
  }
}



/*************************************************
*            The MAKEBUFFER command              *
*************************************************/

int
e_makebuffer(cmdstr *cmd)
{
bufferstr *old = currentbuffer;

/* Ensure a buffer with this number does not exist */

if (cmd_findbuffer(cmd->arg2.value) != NULL)
  {
  error_moan(51, cmd->arg2.value);
  return done_error;
  }

/* Create a new buffer (with an arbitrary number), and then change the number
to the one required. The old number will have been main_nextbufferno-1, so
reset that. */

e_newbuffer(cmd);
currentbuffer->bufferno = cmd->arg2.value;
main_nextbufferno--;

/* The new buffer will have been made current; revert to the
previously current buffer. */

init_selectbuffer(old);
return done_continue;
}



/*************************************************
*           The MARK command                     *
*************************************************/

int
e_mark(cmdstr *cmd)
{
int type = mark_unset;
int yield = done_continue;

switch(cmd->misc)
  {
  case amark_unset:
  if (mark_type != mark_unset)
    {
    mark_line->flags |= lf_shn;
    mark_line = NULL;
    mark_type = mark_unset;
    }
  if (mark_line_global != NULL)
    {
    mark_line_global->flags |= lf_shn;
    mark_line_global = NULL;
    }
  break;

  case amark_limit:
  if (mark_line_global == NULL)
    {
    main_current->flags |= lf_shn;
    mark_line_global = main_current;
    mark_col_global = cursor_col;
    }
  else
    {
    error_moan(43, "global limit", "global limit");
    yield = done_error;
    }
  break;

  case amark_line:
  case amark_hold:
  type = mark_lines;
  break;

  case amark_rectangle:
  type = mark_rect;
  break;

  default:
  type = mark_text;
  break;
  }

if (type != mark_unset)   /* Not "unset" and not global */
  {
  if (mark_type == mark_unset)
    {
    main_current->flags |= lf_shn;
    mark_line = main_current;
    mark_col = cursor_col;
    mark_type = type;
    mark_hold = cmd->misc == amark_hold;
    }
  else
    {
    error_moan(43, mark_type_names[type], mark_type_names[mark_type]);
    yield = done_error;
    }
  }

return yield;
}



/*************************************************
*               The MOUSE command                *
*************************************************/

int
e_mouse(cmdstr *cmd)
{
mouse_enable = ((cmd->flags & cmdf_arg1) != 0)? cmd->arg1.value : !mouse_enable;
sys_mouse(mouse_enable);
return done_continue;
}



/*************************************************
*            The N command                       *
*************************************************/

int
e_n(cmdstr *cmd)
{
(void)cmd;
if ((main_current->flags & lf_eof) != 0)
  {
  if (main_eoftrap) return done_eof;
  error_moan(30, "end of file", "n");
  return done_error;
  }
else
  {
  main_current = main_current->next;
  cursor_col = 0;
  return done_continue;
  }
}



/*************************************************
*             The NAME command                   *
*************************************************/

int
e_name(cmdstr *cmd)
{
uschar *s = (cmd->arg1.string)->text;
int len = Ustrlen(s) + 1;

store_free(main_filealias);
store_free(main_filename);

main_filealias = store_Xget(len);
main_filename = store_Xget(len);

Ustrcpy(main_filealias, s);
Ustrcpy(main_filename, s);

currentbuffer->filename = main_filename;
currentbuffer->filealias = main_filealias;

main_drawgraticules |= dg_bottom;
main_filechanged = TRUE;

return done_continue;
}



/*************************************************
*            The NE[WBUFFER] command             *
*************************************************/

int
e_newbuffer(cmdstr *cmd)
{
bufferstr *new;
FILE *fid = NULL;
uschar *name = ((cmd->flags & cmdf_arg1) != 0)? (cmd->arg1.string)->text : NULL;

if (name != NULL && name[0] != 0)
  {
  fid = sys_fopen(name, US"r");
  if (fid == NULL)
    {
    error_moan(5, name, "reading", strerror(errno));
    return done_error;
    }
  }

/* Get store for new buffer control data */

new = store_Xget(sizeof(bufferstr));

/* Ensure an unused buffer number */

while (cmd_findbuffer(main_nextbufferno++) != NULL);

/* Initialise the new buffer, chain it, and select it */

init_buffer(new, main_nextbufferno - 1, store_copystring(name),
  store_copystring(name), fid);
new->next = main_bufferchain;
main_bufferchain = new;
init_selectbuffer(new);
return done_continue;
}



/*************************************************
*           The OVERSTRIKE command               *
*************************************************/

int
e_overstrike(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg1) != 0) main_overstrike = cmd->arg1.value;
  else main_overstrike = !main_overstrike;
if (main_screenOK) main_drawgraticules |= dg_flags;
return done_continue;
}



/*************************************************
*            The P command                       *
*************************************************/

int
e_p(cmdstr *cmd)
{
(void)cmd;
if (main_current->prev == NULL)
  {
  error_moan(30, "start of file", "p");
  return done_error;
  }
else
  {
  main_current = main_current->prev;
  cursor_col = 0;
  return done_continue;
  }
}



/*************************************************
*          The PA & PB commands                  *
*************************************************/

int
e_pab(cmdstr *cmd)
{
int matchrc;

match_L = FALSE;
match_leftpos = cursor_col;
match_rightpos = main_current->len;

if ((matchrc = cmd_matchse(cmd->arg1.se, main_current)) == MATCH_OK)
  {
  cursor_col = line_charcount(main_current->text,
    (cmd->misc == abe_b)? match_start : match_end);
  return done_continue;
  }
else
  {
  if (matchrc == MATCH_FAILED) error_moanqse(17, cmd->arg1.se);
  return done_error;
  }
}



/*************************************************
*          The PASTE command                     *
*************************************************/

int
e_paste(cmdstr *cmd)
{
bufferstr *old = currentbuffer;

/* An argument specifies which window to paste in */

if ((cmd->flags & cmdf_arg1) != 0)
  {
  bufferstr *new = cmd_findbuffer(cmd->arg1.value);
  if (new == NULL)
    {
    error_moan(26, cmd->arg1.value);
    return done_error;
    }
  if (new != old) init_selectbuffer(new);
  }

if (cut_buffer == NULL || cut_buffer->len == 0) error_moan(55);
  else if (cut_type == cuttype_text) cut_pastetext();
  else cut_pasterect();

/* Restore previous buffer */

if (currentbuffer != old) init_selectbuffer(old);

return done_continue;
}



/*************************************************
*            The PLL and PLR commands            *
*************************************************/

int
e_plllr(cmdstr *cmd)
{
cursor_col = (cmd->misc == abe_b)? 0 :
  line_charcount(main_current->text, main_current->len);
return done_continue;
}



/*************************************************
*              The PROC command                  *
*************************************************/

int
e_proc(cmdstr *cmd)
{
uschar *name = cmd->arg1.string->text;
if (cmd_findproc(name, NULL))
  {
  error_moan(45, name);
  return done_error;
  }
else
  {
  procstr *p = store_Xget(sizeof(procstr));
  p->type = cb_prtype;
  p->flags = 0;
  p->name = store_copystring(name);
  p->body = cmd_copyblock((cmdblock *)(cmd->arg2.cmds));
  p->next = main_proclist;
  main_proclist = p;
  return done_continue;
  }
}



/*************************************************
*             The PROMPT command                 *
*************************************************/

int
e_prompt(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg1) != 0) currentbuffer->noprompt = !cmd->arg1.value;
  else currentbuffer->noprompt = !currentbuffer->noprompt;
return done_continue;
}



/*************************************************
*           The READONLY command                 *
*************************************************/

int
e_readonly(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg1) != 0) main_readonly = cmd->arg1.value;
  else main_readonly = !main_readonly;
main_drawgraticules |= dg_flags;
return done_continue;
}



/*************************************************
*             The REFRESH command                *
*************************************************/

int
e_refresh(cmdstr *cmd)
{
(void)cmd;
if (main_screenOK)
  {
  screen_forcecls = TRUE;
  scrn_display();
  s_selwindow(message_window, -1, -1);
  s_cls();
  s_flush();
  main_pendnl = FALSE;
  }
return done_continue;
}



/*************************************************
*              The RENUMBER command              *
*************************************************/

int
e_renumber(cmdstr *cmd)
{
(void)cmd;
int number = 1;
linestr *line = main_top;

for (;;)
  {
  line->key = number++;
  if ((line->flags & lf_eof) != 0) break;
  line = line->next;
  }
return done_continue;
}



/*************************************************
*             The REPEAT command                 *
*************************************************/

int
e_repeat(cmdstr *cmd)
{
int yield = done_loop;
while (yield == done_loop)
  {
  yield = done_continue;
  while (yield == done_continue)
    {
    if (main_interrupted(ci_loop)) return done_error;
    yield = cmd_obeyline(cmd->arg1.cmds);
    }
  if (yield == done_loop || yield == done_break)
    {
    if (--cmd_breakloopcount > 0) break;
    if (yield == done_break) yield = done_continue;
    }
  }
return yield;
}



/*************************************************
*             The RMARGIN command                *
*************************************************/

int
e_rmargin(cmdstr *cmd)
{
if ((cmd->flags & cmdf_arg2) != 0)
  {
  if (cmd->arg2.value)
    {
    if (main_rmargin > MAX_RMARGIN) main_rmargin -= MAX_RMARGIN;
    }
  else if (main_rmargin < MAX_RMARGIN) main_rmargin += MAX_RMARGIN;
  main_drawgraticules |= dg_margin;
  }

else if ((cmd->flags & cmdf_arg1) != 0)
  {
  int r = cmd->arg1.value;
  if (r > 0)
    {
    main_rmargin = r;
    main_drawgraticules |= dg_both;
    }
  else
    {
    error_moan(15, "0", "as an argument for RMARGIN");
    return done_error;
    }
  }

else
  {
  if (main_rmargin > MAX_RMARGIN) main_rmargin -= MAX_RMARGIN; else
    main_rmargin += MAX_RMARGIN;
  main_drawgraticules |= dg_margin;
  }

return done_continue;
}



/*************************************************
*             The SA & SB commands               *
*************************************************/

int
e_sab(cmdstr *cmd)
{
linestr *prevline = main_current;
int yield = e_pab(cmd);

if (yield != done_continue) return yield;
main_current->flags |= lf_shn;
main_current = line_split(main_current, cursor_col);
cursor_col = 0;

if (main_AutoAlign)
  {
  int dummy;
  usint i;
  uschar *p = prevline->text;
  for (i = 0; i < prevline->len; i++)
    if (p[i] != ' ') { cursor_col = i; break; }
  if (cursor_col > 0)
    line_leftalign(main_current, cursor_col, &dummy);
  }

cmd_refresh = TRUE;
return done_continue;
}

/* End of ee3.c */
