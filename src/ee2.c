/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */


/* This file contains code for obeying commands: Part II */


#include "ehdr.h"
#include "cmdhdr.h"
#include "shdr.h"


/*************************************************
*          The F & BF commands                   *
*************************************************/

/* The same function handles both; the "backwards" flag is in the command
structure. */

int
e_f(cmdstr *cmd)
{
linestr *line = main_current;
sestr *se = cmd->arg1.se;
BOOL stringsearch;
int matched = MATCH_FAILED;
usint cursor_byte = line_offset(line, cursor_col);

match_L = cmd->misc;                 /* Global indicating lefwards matching */

/* Deal with saving and repeated search */

if ((cmd->flags & cmdf_arg1) != 0)
  {
  if (last_se != NULL) cmd_freeblock((cmdblock *)last_se);
  last_se = cmd_copyblock((cmdblock *)se);
  }
else if (last_se == NULL)
  {
  error_moan(16, "search command");
  return done_error;
  }
else se = last_se;

stringsearch = (se->type == cb_qstype) && ((se->flags & qsef_N) == 0);

/* If the cursor is not at the end (start) of the current line, try to match on
it, unless this is a line search, in which case match only if the cursor is at
the start (end). */

if (match_L)    /* Leftwards search */
  {
  if (cursor_byte > 0 && (stringsearch || cursor_byte >= line->len) &&
      (line->flags & lf_eof) == 0)
    {
    match_leftpos = 0;
    match_rightpos = cursor_byte;
    matched = cmd_matchse(se, line);
    }
  }
else            /* Rightwards search */
  {
  if (cursor_byte < line->len && (stringsearch || cursor_byte == 0))
    {
    match_leftpos = cursor_byte;
    match_rightpos = line->len;
    matched = cmd_matchse(se, line);
    }
  }

/* Search for line */

if ((line->flags & lf_eof) == 0 || match_L)
  {
  match_leftpos = 0;
  while (matched == MATCH_FAILED)
    {
    if (main_interrupted(ci_move)) return done_error;
    line = match_L? line->prev : line->next;
    if (line == NULL) break;
    if ((line->flags & lf_eof) != 0)break;
    match_rightpos = line->len;
    matched = cmd_matchse(se, line);
    }
  }

if (matched == MATCH_OK)
  {
  main_current = line;
  cursor_col = line_charcount(line->text, match_L? match_start : match_end);
  return done_continue;
  }

/* Not matched => reached end (start) of file */

if (cmd_eoftrap && !match_L) return done_eof;
if (matched == MATCH_FAILED) error_moanqse(17, se);
return done_error;
}



/*************************************************
*           The FKEYSTRING command               *
*************************************************/

int
e_fks(cmdstr *cmd)
{
key_setfkey(cmd->arg1.value, ((cmd->flags & cmdf_arg2) == 0)? NULL :
  cmd->arg2.string->text);
return done_continue;
}



/*************************************************
*            The FORMAT command                  *
*************************************************/

int
e_format(cmdstr *cmd)
{
(void)cmd;
if ((main_current->flags & lf_eof) == 0)
  {
  line_formatpara(FALSE);
  cmd_refresh = TRUE;
  }
return done_continue;
}



/*************************************************
*             The FRONT command                  *
*************************************************/

int
e_front(cmdstr *cmd)
{
(void)cmd;
main_backnext = main_backtop;
if (main_backlist[main_backnext].line != NULL)
  {
  linestr *line = main_top;

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
*          The GA, GB and GE commands            *
*************************************************/

/* They all call the same function, with cmd->misc differentiating. */

int
e_g(cmdstr *cmd)
{
linestr *limitline = mark_line_global;
int resetgraticules = dg_none;
int lastr = 0;
int misc = cmd->misc;
int yield = done_continue;
int matchcount = 0;
int changecount = 0;
int rcount = 0;
int matched = MATCH_FAILED;
usint oldrmargin = main_rmargin;
usint oldcursor = cursor_col;
uschar *wordptr = US"";
BOOL all = !main_interactive;
BOOL change = all;
BOOL stringsearch, REreplace;
BOOL Gcontinue = TRUE;
BOOL quit = FALSE;
BOOL skip_end = FALSE;
BOOL interrupted = FALSE;
linestr *line = main_current;
linestr *oldcurrent = line;
sestr *se = cmd->arg1.se;
qsstr *nt = cmd->arg2.qs;

/* Deal with saving arguments or re-using old ones */

if ((cmd->flags & cmdf_arg1) != 0)
  {
  if (last_gse != NULL) cmd_freeblock((cmdblock *)last_gse);
  if (last_gnt != NULL) cmd_freeblock((cmdblock *)last_gnt);
  last_gse = cmd_copyblock((cmdblock *)se);
  last_gnt = cmd_copyblock((cmdblock *)nt);
  }
else if (last_gse == NULL)
  {
  error_moan(16, "global command");
  return done_error;
  }
else
  {
  se = last_gse;
  nt = last_gnt;
  }

/* If the search is for the null string, treat all as GB. If null string search
is at end of line and the S qualifier is present, remember this in order always
to skip to the end of line after a match. */

if (se->type == cb_qstype && ((qsstr *)se)->length == 0)
  {
  misc = abe_b;
  skip_end = (((qsstr *)se)->flags & (qsef_E + qsef_S)) == qsef_E + qsef_S;
  }

/* Final preparations */

stringsearch = (se->type == cb_qstype) &&
  ((((qsstr *)se)->flags & qsef_N) == 0);
REreplace = (nt->flags & qsef_R) != 0;

match_L = FALSE;
if (main_rmargin < MAX_RMARGIN) main_rmargin += MAX_RMARGIN;

/* Main loop starts here */

while (Gcontinue)
  {
  usint cursor_byte = line_offset(line, cursor_col);
  matched = MATCH_FAILED;

  /* Look at current line if relevant */

  if (cursor_byte < line->len && (stringsearch || cursor_byte == 0) &&
      (matchcount == 0 || (se->flags & qsef_B) == 0))  /* for null replacements */
    {
    if (main_interrupted(ci_move))
      {
      /* LCOV_EXCL_START */
      yield = done_error;
      quit = interrupted = TRUE;
      break;
      /* LCOV_EXCL_STOP */
      }
    match_leftpos = cursor_byte;
    match_rightpos = line->len;
    if (line == limitline && mark_col_global >= cursor_col)
      match_rightpos = line_offset(line, mark_col_global);
    matched = cmd_matchse(se, line);
    }

  /* Now look at subsequent lines until EOF */

  match_leftpos = 0;
  if ((line->flags & lf_eof) == 0) while (matched == MATCH_FAILED)
    {
    if (main_interrupted(ci_move))
      {
      /* LCOV_EXCL_START */
      yield = done_error;
      quit = interrupted = TRUE;
      break;
      /* LCOV_EXCL_STOP */
      }
    if (line == limitline) break;
    line = line->next;
    if (line == NULL || (line->flags & lf_eof) != 0) break;

    match_rightpos = line->len;
    if (line == limitline) match_rightpos = line_offset(line, mark_col_global);
    matched = cmd_matchse(se, line);
    }

  /* Take action according as matched or not */

  if (matched == MATCH_OK)
    {
    usint boldcol = line_charcount(line->text, match_start);
    usint boldcount = line_charcount(line->text + match_start,
                                      match_end - match_start);
    if (boldcount == 0) boldcount = 1;
    matchcount++;
    main_current = line;
    cursor_col = line_charcount(line->text,
      (misc == abe_a)? match_end - 1 : match_start);
    if (cursor_col > oldrmargin) resetgraticules = dg_both;

    /* If interactive, prompt user, allowing multiple responses */

    if (main_interactive && !all)
      {
      /* LCOV_EXCL_START */
      uschar *prompt =
        US"Change, Skip, Once, Last, All, Finish, Quit or Error? ";

      /* If nothing left from last prompt, read some more */

      if (*wordptr == 0 && rcount <= 0)
        {
        BOOL done = FALSE;
        while (!done)
          {
          if (main_screenOK)
            {
            scrn_hint(sh_above, 1, NULL);
            if (boldcol + boldcount >= cursor_max)
              {
              cursor_rh_adjust = cursor_max - boldcol + 1;
              if (cursor_rh_adjust > 20) cursor_rh_adjust = 20;
              }
            if (cursor_rh_adjust < 3) cursor_rh_adjust = 3;
            scrn_display();
            cursor_rh_adjust = 0;
            if (boldcol + boldcount > cursor_max)
              boldcount = cursor_max - boldcol;
            scrn_invertchars(main_current, cursor_row, boldcol, boldcount,
              TRUE);
            scrn_rdline(FALSE, prompt);
            scrn_display();
            scrn_invertchars(main_current, cursor_row, boldcol, boldcount,
              FALSE);
            main_pendnl = TRUE;
            main_nowait = FALSE;
            s_selwindow(message_window, 0, 0);
            s_flush();
            s_cls();
            }
          else
            {
            line_verify(main_current, TRUE, TRUE);
            error_printf("%s", prompt);
            error_printflush();
            /* Note fudge to avoid compiler warning */
            if(Ufgets(cmd_buffer, CMD_BUFFER_SIZE, kbd_fid)){};
            cmd_buffer[Ustrlen(cmd_buffer)-1] = 0;
            }

          wordptr = cmd_buffer;
          while (*wordptr != 0)
            {
            int c = tolower(*wordptr);
            if (c != ' ')
              {
              if (isdigit(c) || c=='c' || c=='o' || c=='a' || c=='f' ||
                  c=='e' || c=='q' || c=='s' || c=='l')
                { done = TRUE; *wordptr++ = c; }
              else
                {
                prompt = US"Initial letters only: Change, Skip, "
                  "Once, Last, All, Finish, Quit or Error? ";
                done = FALSE;
                break;
                }
              }
            }
          }
        wordptr = cmd_buffer;
        rcount = 0;
        }

      /* Extract the next response */

      if (rcount > 0) rcount--; else
        {
        lastr = *wordptr++;
        while (isdigit(lastr))
          {
          rcount = rcount*10 + lastr - '0';
          lastr = *wordptr++;
          }
        rcount--;
        }

      /* Set flags according to response */

      change = FALSE;
      if (lastr == 'c') change = TRUE;
      else if (lastr == 'o') { change = TRUE; Gcontinue = FALSE; }
      else if (lastr == 'l')
        { change = TRUE; Gcontinue = FALSE; quit = TRUE; }
      else if (lastr == 'a') { change = TRUE; all = TRUE; }
      else if (lastr == 'f') Gcontinue = FALSE;
      else if (lastr == 'q') { Gcontinue = FALSE; quit = TRUE; }
      else if (lastr == 'e') { Gcontinue = FALSE; yield = done_error; }
      /* For 's' (skip), no action */
      /* LCOV_EXCL_STOP */
      }

    /* Make the change if wanted */

    if (change)
      {
      uschar *p = nt->text + 1;
      usint len = nt->length;
      changecount++;

      /* Regular Expression Change. Note that cmd_ReChange() sets cursor_col to
      a byte offset. */

      if (REreplace)
        line = cmd_ReChange(line, p, len, (nt->flags & qsef_X) != 0,
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
          line_deletebytes(line, match_start, match_end - match_start, TRUE);
          line_insertbytes(line, -1, match_start, p, len, 0);
          cursor_col = match_start + len;
          }
        else
          {
          line_insertbytes(line, -1,
            ((misc == abe_a)? match_end : match_start), p, len, 0);
          cursor_col = match_end + len;
          }

        /* If the match included the qualifiers E and S, we might not actually
        be at the end of the line (because of trailing spaces) but we don't
        want to carry on operating on this line. */

        if (skip_end) cursor_col = line->len;
        }

      /* Note line has changed */

      line->flags |= lf_shn;

      /* Why was this code put in here? It causes much screen flapping during
      "all". Ah, perhaps wanted to see changes during other responses. Try
      cutting it out for "all". */

      if (main_screenOK && !all)
        {
        /* LCOV_EXCL_START */
        scrn_display();
        main_pendnl = TRUE;
        main_nowait = FALSE;
        s_selwindow(message_window, 0, 0);
        s_cls();
        /* LCOV_EXCL_STOP */
        }
      }

    /* Change to be skipped - can only happen if interactive */

    else
      {
      cursor_col = match_end;  /* LCOV_EXCL_LINE */
      }

    /* Correct cursor column from byte to character offset */

    cursor_col = line_charcount(line->text, cursor_col);
    }

  /* No match has been found */

  else
    {
    Gcontinue = FALSE;
    if (main_interactive && cmdin_fid == NULL && matchcount <= 0)
      {
      /* LCOV_EXCL_START */
      if (matched == MATCH_FAILED) error_moanqse(17, se);  /* not found */
      yield = done_error;
      /* LCOV_EXCL_STOP */
      }
    }
  }

/* Verify number of matches and changes if interactive */

if (!interrupted && main_interactive && matchcount > 0)
  {
  /* LCOV_EXCL_START */
  uschar buff[256];
  sprintf(CS buff, "%s%d match%s, %d change%s",
    (matched == MATCH_FAILED && yield != done_error)?
      ((line == mark_line_global)? "Global limit reached: " : "No more: ") : "",
      matchcount, (matchcount == 1)? "" : "es",
        changecount, (changecount == 1)? "" : "s");

  if (main_screenOK)
    {
    s_flush();  /* Possible graticule update for final change */
    if (mark_type == mark_unset && mark_line_global == NULL)
      {
      s_selwindow(message_window, 0, 0);
      s_cls();
      main_leave_message = TRUE;
      s_printf("%s", buff);
      main_pendnl = TRUE;
      }
    else
      {
      error_printf("%s\n", buff);
      }
    }
  else sys_mprintf(msgs_fid, "%s\n", buff);
  /* LCOV_EXCL_STOP */
  }

/* Restore rmargin and original position unless "quit" */

main_rmargin = oldrmargin;
if (!quit)
  {
  cursor_col = oldcursor;
  main_current = oldcurrent;
  }

main_drawgraticules |= resetgraticules;
return yield;
}



/*************************************************
*            The I command                       *
*************************************************/


int
e_i(cmdstr *cmd)
{
cmd_refresh = TRUE;

/* If there was an argument, it is a file that is to be inserted. */

if ((cmd->flags & cmdf_arg1) != 0)
  {
  size_t binoffset = 0;
  linestr *botline;
  uschar *name = cmd->arg1.string->text;
  FILE *f = sys_fopen(name, US"r");

  if (f == NULL)
    {
    error_moan(5, name, "reading", strerror(errno));
    return done_error;
    }

  /* We first read all the lines into store, chaining them together. Then we
  splice the chain into the existing chain of lines above the current line. */

  botline = file_nextline(f, &binoffset);

  if ((botline->flags & lf_eof) == 0)
    {
    int count = 1;
    linestr *prev = main_current->prev;
    linestr *topline = botline;
    linestr *line = botline;

    botline = file_nextline(f, &binoffset);
    while ((botline->flags & lf_eof) == 0)
      {
      line->next = botline;
      botline->prev = line;
      line = botline;
      botline = file_nextline(f, &binoffset);
      count++;
      }

    line->next = main_current;
    topline->prev = prev;
    if (prev == NULL) main_top = topline; else prev->next = topline;
    main_current->prev = line;
    main_linecount += count;

    cmd_recordchanged(main_current, cursor_col);
    cmd_recordchanged(topline, 0);

    if (main_screenOK) scrn_hint(sh_insert, count, NULL);
    }

  store_free(botline->text);
  store_free(botline);
  fclose(f);
  return done_continue;
  }

/* If there was no argument, inline lines are to be inserted. */

else
  {
  int count = 0;
  int yield = done_continue;
  linestr *prev = main_current->prev;
  linestr *topline = NULL;

  if (main_screenOK)
    {
    if (main_pendnl)
      {
      /* LCOV_EXCL_START */
      sys_mprintf(msgs_fid, "\r\n");
      main_pendnl = main_nowait = FALSE;
      /* LCOV_EXCL_STOP */
      }
    screen_forcecls = TRUE;
    }

  /* Loop reading lines until teminator */

  for (;;)
    {
    BOOL eof = FALSE;
    linestr *line;

    if (cmd_cbufferline != NULL)
      {
      line = store_copyline(cmd_cbufferline);
      cmd_cbufferline = cmd_cbufferline->next;
      cmd_clineno++;
      }
    else if (cmdin_fid != NULL)
      {
      line = file_nextline(cmdin_fid, NULL);
      cmd_clineno++;
      }
    else  /* Interactive */
      {
      /* LCOV_EXCL_START */
      int len;
      if (main_screenOK)
        {
        scrn_rdline(FALSE, US"NE< ");
        len = Ustrlen(cmd_buffer);
        if (len > 0 && cmd_buffer[len-1] == '\n') cmd_buffer[--len] = 0;
        line = store_getlbuff(len);
        memcpy(line->text, cmd_buffer, len);
        }
      else line = file_nextline(kbd_fid, NULL);
      /* LCOV_EXCL_STOP */
      }

    if ((line->flags & lf_eof) != 0)
      {
      error_moan(29, "End of file", "I");
      eof = TRUE;
      yield = done_error;
      }

    if (eof || main_interrupted(ci_read) ||
      (line->len == 1 && tolower(line->text[0]) == 'z'))
        {
        store_free(line->text);
        store_free(line);
        break;
        }

    if (topline == NULL) topline = line;
    if (prev == NULL) main_top = line; else prev->next = line;
    line->next = main_current;
    line->prev = prev;
    main_current->prev = prev = line;
    count++;
    }

  main_linecount += count;
  if (count > 0)
    {
    cmd_recordchanged(main_current, cursor_col);
    if (topline != NULL) cmd_recordchanged(topline, 0);
    }

  if (main_screenOK) scrn_hint(sh_insert, count, NULL);
  return yield;
  }
}



/*************************************************
*            The ICURRENT command                *
*************************************************/

int
e_icurrent(cmdstr *cmd)
{
(void)cmd;
linestr *newline;
linestr *prev = main_current->prev;

if ((main_current->flags & lf_eof) != 0)
  {
  error_moan(29, "End of file", "ICURRENT");
  return done_error;
  }

newline = line_copy(main_current);
newline->key = 0;   /* Kill line number */
if (prev == NULL) main_top = newline; else prev->next = newline;
newline->prev = prev;
newline->next = main_current;
main_current->prev = newline;
main_linecount++;
cmd_recordchanged(main_current, cursor_col);
if (main_screenOK) scrn_hint(sh_insert, 1, NULL);
cmd_refresh = TRUE;
return done_continue;
}



/*************************************************
*            The IF command                      *
*************************************************/

/* This function is also used for UNLESS -- the switch in the command block
distinguishes. An absent first argument signifies that the test is for eof or
for the marked line. */

int
e_if(cmdstr *cmd)
{
int match;
int misc = cmd->misc;
sestr *se = ((cmd->flags & cmdf_arg1) == 0)? NULL : cmd->arg1.se;
ifstr *ifblock = cmd->arg2.ifelse;

if ((misc & if_prompt) != 0)
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

if (match == MATCH_ERROR) return done_error;
if (misc >= if_unless) match = (match == MATCH_OK)? MATCH_FAILED : MATCH_OK;
return cmd_obeyline((match == MATCH_OK)? ifblock->if_then : ifblock->if_else);
}



/*************************************************
*                The ILINE command               *
*************************************************/

int
e_iline(cmdstr *cmd)
{
int len;
qsstr *qs = cmd->arg1.qs;
linestr *prev = main_current->prev;
linestr *line;
uschar *p, *pqs;

if ((qs->flags & qsef_X) == 0)
  {
  pqs = qs->text + 1;
  len = qs->length;
  }
else
  {
  pqs = qs->hexed;
  len = qs->length / 2;
  }

line = store_getlbuff(len);
p = line->text;

/* If len == 0, the text pointer will be NULL which causes ASAN to complain, so
test for that case. */

if (len > 0)
  {
  memcpy(p, pqs, len);
  line->len = len;
  }
line->next = main_current;
line->prev = prev;
main_current->prev = line;
if (prev == NULL) main_top = line; else prev->next = line;

main_linecount++;
cmd_recordchanged(main_current, cursor_col);
if (main_screenOK) scrn_hint(sh_insert, 1, NULL);
cmd_refresh = TRUE;
return done_continue;
}



/*************************************************
*             The ISPACE command                 *
*************************************************/

int
e_ispace(cmdstr *cmd)
{
(void)cmd;
if (mark_type != mark_rect)
  {
  error_moan(41, "ispace");
  return done_error;
  }
else
  {
  linestr *line, *endline;
  int left, right, rectwidth;

  if (cursor_col < mark_col)
    { left = cursor_col; right = mark_col; }
  else
    { left = mark_col; right = cursor_col; }

  if (line_checkabove(mark_line) >= 0)
    { line = mark_line; endline = main_current; }
  else
    { line = main_current; endline = mark_line; }

  rectwidth = right - left;
  mark_type = mark_unset;
  mark_line = NULL;

  /* Loop through the relevant lines */

  for (;;)
    {
    if ((line->flags & lf_eof) == 0)
      {
      line_insertbytes(line, left, -1, NULL, 0, rectwidth);
      line->flags |= lf_shn;
      }
    if (line == endline) break;
    line = line->next;
    }
  return done_continue;
  }
}



/*************************************************
*                The KEY command                 *
*************************************************/

int
e_key(cmdstr *cmd)
{
return key_set(cmd->arg1.string->text, TRUE)? done_continue : done_error;
}



/*************************************************
*            The LCL & UCL commands              *
*************************************************/

static
int lettercase(int (*func)(int))
{
uschar *p = main_current->text + line_offset(main_current, cursor_col);
uschar *pe = main_current->text + main_current->len;
while (p < pe)
  {
  if (*p < 128) *p = func(*p);
  SKIPCHAR(p, pe);
  cursor_col++;
  }
main_current->flags |= lf_shn;
main_filechanged = TRUE;
return done_continue;
}

int e_lcl(cmdstr *cmd) { (void)cmd; return lettercase(tolower); }
int e_ucl(cmdstr *cmd) { (void)cmd; return lettercase(toupper); }



/*************************************************
*             The LOAD command                   *
*************************************************/

int
e_load (cmdstr *cmd)
{
uschar *s = (cmd->arg1.string)->text;
bufferstr *buffer = currentbuffer;
bufferstr *next = currentbuffer->next;
int n = currentbuffer->bufferno;
int yield = done_continue;
BOOL noprompt = currentbuffer->noprompt || !main_warnings;
FILE *fid;

/* Emptybuffer does not free the windowtitle field */

if (!cmd_emptybuffer(currentbuffer, US"LOAD")) return done_error;

if ((fid = sys_fopen(s, US"r")) == NULL)
  {
  error_moan(5, s, "reading", strerror(errno));
  yield = done_error;
  s = US"";
  }

/* Re-initialize buffer (destroys the next and noprompt fields; also the
windowtitle and windowhandle fields; gets a new back list). */

init_buffer(currentbuffer, n, store_copystring(s), store_copystring(s), fid);

currentbuffer->next = next;             /* restore */
currentbuffer->noprompt = noprompt;

currentbuffer = NULL;                   /* de-select to inhibit save */
init_selectbuffer(buffer);

return yield;
}



/*************************************************
*               The LOOP command                 *
*************************************************/

int
e_loop(cmdstr *cmd)
{
cmd_breakloopcount = ((cmd->flags & cmdf_arg1) != 0)? cmd->arg1.value : 1;
return done_loop;
}

/* End of ee2.c */
