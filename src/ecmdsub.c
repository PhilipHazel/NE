/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2022 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: December 2022 */


/* This file contains code for command-processing functions */


#include "ehdr.h"


/*************************************************
*             Check for end of command           *
*************************************************/

BOOL
cmd_atend(void)
{
mac_skipspaces(cmd_ptr);
return (*cmd_ptr == 0 || *cmd_ptr == ';' || *cmd_ptr == ')' || *cmd_ptr == '\n');
}



/*************************************************
*              Read a word                       *
*************************************************/

/* The word is placed in cmd_word. */

void
cmd_readword(void)
{
int c;
int n = 0;
uschar *p = cmd_word;
mac_skipspaces(cmd_ptr);
c = *cmd_ptr;
while (isalpha(c))
  {
  if (n++ < max_wordlen) *p++ = tolower(c);
  if ((c = *(++cmd_ptr)) == 0) break;
  }
*p = 0;
}



/*************************************************
*            Read number                         *
*************************************************/

/* This function expects an unsigned decimal number. A negative value is
returned if one is not found. */

int cmd_readnumber(void)
{
int n = -1;
int c;
mac_skipspaces(cmd_ptr);

c = *cmd_ptr;
if (isdigit(c))
  {
  n = 0;
  while (isdigit(c))
    {
    n = n*10 + c - '0';
    c = *(++cmd_ptr);
    }
  }

return n;
}



/*************************************************
*        Read a Plain String                     *
*************************************************/

/* Local subroutine called in two different ways below.

Arguments:
  answer     where to put the string control block pointer
  delim      0 => first character is the delimiter
             else a delimiter character (typically ';')

Returns:   < 0 => error (invalid delimiter when delim == 0)
             0 => no string found
           > 0 => success
*/

static int
xreadstring(stringstr **answer, int delim)
{
stringstr *st;
int dch;
uschar *p;

mac_skipspaces(cmd_ptr);
if (cmd_atend()) return 0;

if (delim == 0)
  {
  dch = *cmd_ptr++;
  if ((ch_tab[dch] & ch_delim) == 0)
    {
    error_moan(13, "String");
    return -1;
    }
  }
else dch = delim;

p = cmd_ptr;
while (*p != 0 && *p != '\n' && *p != dch) p++;
if (*p != dch && dch != ';') cmd_ist = dch;  /* Implied delimiter */

st = store_Xget(sizeof(stringstr));
st->type = cb_sttype;
st->delim = dch;
st->hexed = FALSE;
st->text = store_copystring2(cmd_ptr, p);

cmd_ptr = (*p == 0 || *p == ';')? p : p+1;
mac_skipspaces(cmd_ptr);

*answer = st;  /* pass back control block */
return 1;      /* and appropriate result */
}

/* External entry points */

int cmd_readstring(stringstr **p)  { return xreadstring(p, 0); }
int cmd_readUstring(stringstr **p) { return xreadstring(p, ';'); }



/*************************************************
*        Get a command control block             *
*************************************************/

/* Argument is the command id */

cmdstr *
cmd_getcmdstr(int id)
{
cmdstr *yield = store_Xget(sizeof(cmdstr));
yield->flags = yield->misc = yield->ptype1 = yield->ptype2 = 0;
yield->next = NULL;
yield->type = cb_cmtype;
yield->id = id;
return yield;
}



/*************************************************
*            Copy a control block                *
*************************************************/

void *
cmd_copyblock(cmdblock *cb)
{
void *yield;
if (cb == NULL) return NULL;
yield = store_copy(cb);

switch (cb->type)
  {
  case cb_sttype:
    {
    stringstr *y = (stringstr *)yield;
    stringstr *s = (stringstr *)cb;
    y->text = store_copy(s->text);
    }
  break;

  case cb_setype:
    {
    sestr *y = (sestr *)yield;
    sestr *s = (sestr *)cb;
    y->left.se = cmd_copyblock((cmdblock *)(s->left.se));
    y->right.se = cmd_copyblock((cmdblock *)(s->right.se));
    }
  break;

  case cb_qstype:
    {
    qsstr *y = (qsstr *)yield;
    qsstr *q = (qsstr *)cb;
    y->text  = store_copy(q->text);
    y->cre   = store_copy(q->cre);
    y->hexed = store_copy(q->hexed);
    }
  break;

  case cb_cmtype:
    {
    cmdstr *cmd = (cmdstr *)cb;
    cmdstr *cy = (cmdstr *)yield;
    if ((cmd->flags & cmdf_arg1F) != 0)
      cy->arg1.cmds = (cmdstr *)cmd_copyblock((cmdblock *)cmd->arg1.cmds);
    if ((cmd->flags & cmdf_arg2F) != 0)
      cy->arg2.cmds = (cmdstr *)cmd_copyblock((cmdblock *)cmd->arg2.cmds);
    cy->next = cmd_copyblock((cmdblock *)cmd->next);
    }
  break;

  case cb_iftype:
    {
    ifstr *in  = (ifstr *)cb;
    ifstr *out = (ifstr *)yield;
    out->if_then = cmd_copyblock((cmdblock *)in->if_then);
    out->if_else = cmd_copyblock((cmdblock *)in->if_else);
    }
  break;

  /* It seems that this case never actually occurs in practice, so for the
  moment, cut it out of coverage. */

  /* LCOV_EXCL_START */
  case cb_prtype:
    {
    procstr *in =  (procstr *)cb;
    procstr *out = (procstr *)yield;
    out->name = store_copystring(in->name);
    out->body = cmd_copyblock((cmdblock *)in->body);
    }
  break;
  /* LCOV_EXCL_STOP */
  }

return yield;
}


/*************************************************
*            Free a control block                *
*************************************************/

/* Any attached blocks are also freed */

void
cmd_freeblock(cmdblock *cb)
{
if (cb == NULL) return;

#ifdef showfree
printf("Freeing %d - ", cb);
#endif

switch (cb->type)
  {
  case cb_sttype:
    {
    stringstr *s = (stringstr *)cb;
    store_free(s->text);
    }
  break;

  case cb_setype:
    {
    sestr *s = (sestr *)cb;
    store_free(s->left.se);
    store_free(s->right.se);
    }
  break;

  case cb_qstype:
    {
    qsstr *q = (qsstr *)cb;
    store_free(q->text);
    store_free(q->cre);
    store_free(q->hexed);
    }
  break;

  case cb_cmtype:
    {
    cmdstr *cmd = (cmdstr *)cb;
    if ((cmd->flags & cmdf_arg1F) != 0) cmd_freeblock((cmdblock *)cmd->arg1.block);
    if ((cmd->flags & cmdf_arg2F) != 0) cmd_freeblock((cmdblock *)cmd->arg2.block);
    cmd_freeblock((cmdblock *)cmd->next);
    }
  break;

  case cb_iftype:
    {
    ifstr *ifblock = (ifstr *)cb;
    cmd_freeblock((cmdblock *)ifblock->if_then);
    cmd_freeblock((cmdblock *)ifblock->if_else);
    }
  break;

  case cb_prtype:
    {
    procstr *pblock = (procstr *)cb;
    store_free(pblock->name);
    cmd_freeblock((cmdblock *)pblock->body);
    }
  break;
  }

store_free(cb);
}



/*************************************************
*         Join on continuation line              *
*************************************************/

/* The name is now a misnomer. It replaces rather than joins a new line. We
create a new command line starting with a semicolon. This is for the benefit of
the if command, which reads a new line to see if there is an "else", and if
not, wants to leave the pointer at a semicolon terminating the "if".

Argument:  TRUE if end of file is not an error
Returns:   TRUE for OK, FALSE after an error
*/

BOOL
cmd_joinline(BOOL eofflag)
{
BOOL eof = FALSE;

/* Deal with command lines from buffer */

if (cmd_cbufferline != NULL)
  {
  if ((cmd_cbufferline->flags & lf_eof) != 0) eof = TRUE; else
    {
    if (cmd_cbufferline->len > CMD_BUFFER_SIZE - 2)
       {
       error_moan_decode(56);
       return FALSE;
       }
    if (cmd_cbufferline->len > 0)
      memcpy(cmd_buffer+1, cmd_cbufferline->text, cmd_cbufferline->len);
    cmd_buffer[cmd_cbufferline->len+1] = 0;
    cmd_cbufferline = cmd_cbufferline->next;
    cmd_clineno++;
    }
  }

/* Don't want to have yet another argument to scrn_rdline, so just move up
the line afterwards. */

else if (main_screenOK && cmdin_fid == NULL)
  {
  scrn_rdline(FALSE, US"NE+ ");
  memmove(cmd_buffer+1, cmd_buffer, CMD_BUFFER_SIZE-1);
  main_nowait = main_repaint = TRUE;
  }
else
  {
  if (main_interactive && cmdin_fid != NULL)
    {
    /* LCOV_EXCL_START - not in standard tests */
    error_printf("NE+ ");
    error_printflush();
    /* LCOV_EXCL_STOP */
    }
  eof = (cmdin_fid == NULL) ||
    (Ufgets(cmd_buffer+1, CMD_BUFFER_SIZE-1, cmdin_fid) == NULL);
  if (!eof) cmd_clineno++;
  }

/* Fill in the initial semicolon */

cmd_buffer[0] = ';';

/* Deal with reaching the end of a file */

if (eof)
  {
  if (!eofflag) error_moan_decode(32);    /* flag set => no error */
  return FALSE;
  }

/* Set up for a new command line */

else
  {
  int n = Ustrlen(cmd_buffer);
  if (n > 0 && cmd_buffer[n-1] == '\n') n--;
  if (cmd_ist > 0) cmd_buffer[n++] = cmd_ist;    /* insert assumed delimiter */
  cmd_buffer[n++] = ';';                         /* in case not present */
  cmd_buffer[n] = 0;
  cmd_ptr = cmd_buffer;
  return TRUE;
  }
}



/*************************************************
*             Confirm Output Wanted              *
*************************************************/

/* This is called by the SAVE and W commands.

Arguments:
  name         candidate file name or NULL
  stopflag     allow "stop" response
  discardflag  allow "discard" response
  buffno       if >= 0, show as buffer number in prompt
  aname        where to return a new name

Returns:       0 => yes
               1 => no
               2 => stop
               3 => discard
               4 => new file name
*/

int
cmd_confirmoutput(uschar *name, BOOL stopflag, BOOL discardflag, int buffno,
  uschar **aname)
{
uschar buff[256];
uschar *pname = name;
uschar *dots = US"";
int yield = 0;
BOOL yesok = (name != NULL);

*aname = NULL;   /* initialize to no new name */

/* Assume "yes" if not interactive or if prompting or warning is disabled and
there is a file name. */

if (!main_interactive ||
  ((currentbuffer->noprompt || !main_warnings) && yesok))
    return 0;

/* LCOV_EXCL_START - the standard tests are not interactive */

/* Cope with overlong names */

if (yesok && (int)Ustrlen(pname) > 100)
  {
  pname += Ustrlen(pname) - 100;
  dots = US"...";
  }

if (buffno >= 0)
  {
  if (yesok)
    sprintf(CS buff, "Write buffer %d to %s%s? (Y/N",
      buffno, dots, pname);
  else
    sprintf(CS buff, "Write buffer %d? (N", buffno);
  }
else
  {
  if (yesok)
    sprintf(CS buff, "Write to %s%s? (Y/N", dots, pname);
  else
    sprintf(CS buff, "Write? (N");
  }

sprintf(CS buff + Ustrlen(buff), "/TO filename%s%s) ",
  discardflag? "/Discard" : "", stopflag? "/STOP" : "");

/* Deal with overlong prompt line; can only happen if a file name
has been included. */

if (main_screenOK && Ustrlen(buff) > window_width)
  {
  int shortenby = (int)Ustrlen(buff) - window_width;
  uschar *at = Ustrchr(buff, 'o');
  if (at != NULL)    /* just in case */
    {
    (void)memmove(at+2, at+2+shortenby,
      (int)Ustrlen(buff) - ((at - buff) + 2 + shortenby) + 1);
    (void)memcpy(at+2, "...", 3);
    }
  }

/* Loop during bad responses */

error_werr = TRUE;

for (;;)
  {
  BOOL done = FALSE;

  while (!done)
    {
    usint cmdbufflen;
    uschar *p = cmd_buffer;
    if (main_screenOK) scrn_rdline(FALSE, buff); else
      {
      error_printf("%s", buff);
      error_printflush();
      if (Ufgets(cmd_buffer, CMD_BUFFER_SIZE, kbd_fid) == NULL) return 0;
      }
    cmdbufflen = Ustrlen(cmd_buffer);
    if (cmdbufflen > 0 && cmd_buffer[cmdbufflen-1] == '\n')
      cmd_buffer[cmdbufflen-1] = 0;
    while (*p != 0) if (*p++ != ' ') { done = TRUE; break; }
    }

  cmd_ptr = cmd_buffer;
  mac_skipspaces(cmd_ptr);
  cmd_readword();
  mac_skipspaces(cmd_ptr);

  if (*cmd_ptr == 0 || *cmd_ptr == '\n')
    {
    if ((Ustrcmp(cmd_word, "y") == 0 || Ustrcmp(cmd_word, "yes") == 0) && yesok) break;
    if (Ustrcmp(cmd_word, "n") == 0 || Ustrcmp(cmd_word, "no") == 0)
      { yield = 1; break; }
    if (Ustrcmp(cmd_word, "stop") == 0 && stopflag) { yield = 2; break; }
    if ((Ustrcmp(cmd_word, "d") == 0 || Ustrcmp(cmd_word, "discard") == 0) && discardflag)
      { yield = 3; break; }
    error_moan(11);
    }

  else if (Ustrcmp(cmd_word, "to") != 0) error_moan(11); else
    {
    uschar *msg = sys_checkfilename(cmd_ptr);
    if (msg == NULL)
      {
      *aname = cmd_ptr;
      yield = 4;
      break;
      }
    else error_moan(12, cmd_ptr, msg);
    }
  }

error_werr = FALSE;
return yield;

/* LCOV_EXCL_STOP */
}



/*************************************************
*             Mark file changed                  *
*************************************************/

/* We remember that the file has changed, and update the backup list
appropriately.

Arguments:
  line       the line that has changed
  col        the relevant column

Returns:     nothing
*/

void
cmd_recordchanged(linestr *line, int col)
{
main_filechanged = TRUE;

/* If the top back setting is NULL, this is the first change, so we can just
set it. Otherwise, if the top setting is for this line, just update it.
Otherwise, ensure that any previous settings for this line and any nearby ones
are removed before setting a new top entry. */

if (main_backlist[main_backtop].line != NULL &&
    main_backlist[main_backtop].line != line)
  {
  int i;
  linestr *tline = line;
  linestr *bline = line;

  /* Try to find a region of size lines, preferably centred on this line, for
  which we are going to remove all entries. */

  for (i = (int)main_backregionsize/2; i > 0; i--)
    {
    if (tline->prev == NULL) break;
    tline = tline->prev;
    }

  for (i += (int)main_backregionsize/2; i > 0; i--)
    {
    if (bline->next == NULL) break;
    bline = bline->next;
    }

  for (; i > 0; i--)
    {
    if (tline->prev == NULL) break;
    tline = tline->prev;
    }

  /* Scan the existing list and remove any appropriate lines. We scan down from
  the top of this list - this means that when the upper entries are slid down,
  we do not need to rescan the current entry as it is guaranteed to be OK. */

  for (i = (int)main_backtop; i >= 0; i--)
    {
    linestr *sline = tline;
    for(;;)
      {
      if (main_backlist[i].line == sline)
        {
        memmove(main_backlist + i, main_backlist + i + 1,
          (main_backtop - i) * sizeof(backstr));
        if (main_backtop > 0) main_backtop--;
          else main_backlist[0].line = NULL;
        break;
        }
      if (sline == bline) break;
      sline = sline->next;
      }
    }

  /* Check for a full list; if it's full, discard the bottom element. Otherwise
  advance to a new slot unless we are at the NULL empty-list slot. Currently
  none of the standard tests hits the limit. */

  if (main_backtop == back_size - 1)
    memmove(main_backlist, main_backlist + 1,  /* LCOV_EXCL_LINE */
      (back_size - 1) * sizeof(backstr));
  else
    if (main_backlist[main_backtop].line != NULL) main_backtop++;
  }

main_backlist[main_backtop].line = line;
main_backlist[main_backtop].col = col;
main_backnext = main_backtop;
}



/*************************************************
*          Find a numbered buffer                *
*************************************************/

bufferstr *
cmd_findbuffer(int n)
{
bufferstr *yield = main_bufferchain;
while (yield != NULL)
  {
  if (yield->bufferno == n) return yield;
  yield = yield->next;
  }
return NULL;
}



/*************************************************
*             Empty a buffer                     *
*************************************************/

/* Subroutine to empty a buffer, close any files that are associated with it,
and free any associated memory. Called by DBUFFER and LOAD. If the buffer has
changed since last saved and prompting and warning are enabled, the user is
asked to confirm. Note that we do not want to select the buffer, as that would
cause an unnecessary screen refresh. The buffer block must be re-initialized
before re-use.

Arguments:
  buffer     the buffer to be emptied
  cmdname    the command name, for messages

Returns:     TRUE for success
             FALSE if prompting got a negative response
*/

BOOL
cmd_emptybuffer(bufferstr *buffer, uschar *cmdname)
{
linestr *line;
int linecount = buffer->linecount;
uschar *filealias = buffer->filealias;
uschar *filename = buffer->filename;

/* Ensure relevant cached values are filed back */

if (buffer == currentbuffer)
  {
  buffer->changed = main_filechanged;
  buffer->top = main_top;
  buffer->bottom = main_bottom;
  }

if (buffer->changed && !buffer->noprompt && main_warnings)
  {
  error_moan(24, buffer->bufferno);
  if (!cmd_yesno("Continue with %s (Y/N)? ", cmdname)) return FALSE;
  }

line = buffer->top;
while (line != NULL)
  {
  linestr *next = line->next;

  if (main_interrupted(ci_delete))
    {
    /* LCOV_EXCL_START */
    line->prev = NULL;
    buffer->top = buffer->current = line;
    buffer->linecount = linecount;
    buffer->col = 0;

    if (buffer == currentbuffer)
      {
      main_linecount = linecount;
      main_current = main_top = line;
      cursor_col = 0;
      }
    error_moan(57);

    return FALSE;
    /* LCOV_EXCL_STOP */
    }

  if (line->text != NULL) store_free(line->text);
  store_free(line);
  linecount--;
  line = next;
  }

store_free(filealias);
store_free(filename);
store_free(buffer->backlist);
return TRUE;
}



/*************************************************
*            Simple yes/no prompt                *
*************************************************/

/* A yield of TRUE means 'yes', and this is always given if running
non-interactively. */

BOOL
cmd_yesno(const char *s, ...)
{
BOOL yield = TRUE;
va_list ap;
uschar prompt[256];

va_start(ap, s);

vsprintf(CS prompt, s, ap);

/* LCOV_EXCL_START */
if (main_interactive) for (;;)
  {
  if (main_screenOK)
    {
    scrn_rdline(FALSE, prompt);
    printf("\n");
    }
  else
    {
    error_printf("%s", prompt);
    error_printflush();
    if (Ufgets(cmd_buffer, CMD_BUFFER_SIZE, kbd_fid) == NULL) break;
    }

  cmd_ptr = cmd_buffer;
  cmd_readword();

  if (cmd_atend())
    {
    if (Ustrcmp(cmd_word, "y") == 0 || Ustrcmp(cmd_word, "yes") == 0) break;
      else if (Ustrcmp(cmd_word, "n") == 0 || Ustrcmp(cmd_word, "no") == 0)
        { yield = FALSE; break; }
    }
  }
/* LCOV_EXCL_STOP */

if (yield) main_pendnl = main_nowait = TRUE;
return yield;
}



/*************************************************
*           Read Procedure Name                  *
*************************************************/

/* The name of an NE procedure must start with a dot followed by any number of
alphanumerics. It can only be followed by a space (when being defined) or by
eol, space, semicolon, or closing parens (when being executed).

Argument:   where to return the string structure containing the name
Returns:    TRUE if OK, FALSE on error
*/

BOOL
cmd_readprocname(stringstr **aname)
{
uschar *p, *q;
int n;
stringstr *st;

mac_skipspaces(cmd_ptr);
p = cmd_ptr;

if (*cmd_ptr++ != '.')
  {
  error_moan(46);
  return FALSE;
  }

while (isalnum((usint)(*cmd_ptr))) cmd_ptr++;
if (*cmd_ptr != 0 && *cmd_ptr != ' ' && *cmd_ptr != ';' && *cmd_ptr != ')')
  {
  error_moan(46);
  return FALSE;
  }

n = cmd_ptr - p;
st = store_Xget(sizeof(stringstr));
st->type = cb_sttype;
st->delim = ' ';
st->hexed = FALSE;
st->text = q = store_Xget(n + 1);

while (n--) *q++ = tolower(*p++);
*q = 0;

mac_skipspaces(cmd_ptr);
*aname = st;               /* pass back control block */
return TRUE;
}



/*************************************************
*              Find procedure                    *
*************************************************/

/* If found, move to top of list.

Arguments:
  name      required name
  ap        where to return the found procedure or NULL if not needed

Returns:    TRUE on success, FALSE if not found
*/

BOOL
cmd_findproc(uschar *name, procstr **ap)
{
procstr *p = main_proclist;
procstr *pp = NULL;
while (p != NULL)
  {
  if (Ustrcmp(name, p->name) == 0)
    {
    if (ap != NULL) { *ap = p; }
    if (pp != NULL)
      {
      pp->next = p->next;
      p->next = main_proclist;
      main_proclist = p;
      }
    return TRUE;
    }
  pp = p;
  p = p->next;
  }
return FALSE;
}

/* End of ecmdsub.c */
