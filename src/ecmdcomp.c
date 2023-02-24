/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */


/* This file contains code for the top-level handling of a command line, and
for compiling the line into an internal form. */


#include "ehdr.h"
#include "cmdhdr.h"


static BOOL SpecialCmd;

/* This list must be in alphabetical order, and must be kept in step with the
two tables at the end of ecmdarg, and also with the following table of
read-only permissions. This table is global so that it can be output by the
SHOW command. */

uschar *cmd_list[] = {
  US"a",
  US"abandon",
  US"align",
  US"alignp",
  US"attn",
  US"autoalign",
  US"b",
  US"back",
  US"backregion",
  US"backup",
  US"beginpar",
  US"bf",
  US"break",
  US"buffer",
  US"c",
  US"casematch",
  US"cbuffer",
  US"cdbuffer",
  US"center",
  US"centre",
  US"cl",
  US"closeback",
  US"closeup",
  US"comment",
  US"copy",
  US"cproc",
  US"csd",
  US"csu",
  US"cut",
  US"cutstyle",
  US"dbuffer",
  US"dcut",
  US"debug",
  US"detrail",
  US"df",
  US"dleft",
  US"dline",
  US"dmarked",
  US"drest",
  US"dright",
  US"dta",
  US"dtb",
  US"dtwl",
  US"dtwr",
  US"e",
  US"eightbit",
  US"endpar",
  US"f",
  US"fkeystring",
  US"fks",
  US"format",
  US"front",
  US"ga",
  US"gb",
  US"ge",
  US"help",
  US"i",
  US"icurrent",
  US"if",
  US"iline",
  US"ispace",
  US"key",
  US"lcl",
  US"load",
  US"loop",
  US"m",
  US"makebuffer",
  US"mark",
  US"mouse",
  US"n",
  US"name",
  US"ne",
  US"newbuffer",
  US"overstrike",
  US"p",
  US"pa",
  US"paste",
  US"pb",
  US"pbuffer",
  US"pll",
  US"plr",
  US"proc",
  US"prompt",
  US"quit",
  US"readonly",
  US"refresh",
  US"renumber",
  US"repeat",
  US"rmargin",
  US"sa",
  US"save",
  US"sb",
  US"set",
  US"show",
  US"stop",
  US"subchar",
  US"t",
  US"title",
  US"tl",
  US"topline",
  US"ucl",
  US"undelete",
  US"unformat",
  US"unless",
  US"until",
  US"uteof",
  US"verify",
  US"w",
  US"warn",
  US"while",
  US"widechars",
  US"word",
  US"write"
};

int cmd_listsize = sizeof(cmd_list)/sizeof(uschar *);

/* The ids for special commands carry on from the end of those for normal
commands. */

#define cmd_specialbase cmd_listsize

/* Indicators for commands that are permitted in readonly buffers */

uschar cmd_readonly[] = {
  0, /* a */
  1, /* abandon */
  0, /* align */
  0, /* alignp */
  1, /* attn */
  1, /* autoalign */
  0, /* b */
  0, /* back */
  0, /* backregion */
  1, /* backup */
  1, /* beginpar */
  1, /* bf */
  1, /* break */
  1, /* buffer */
  1, /* c */
  1, /* casematch */
  1, /* cbuffer */
  1, /* cdbuffer */
  0, /* center */
  0, /* centre */
  0, /* cl */
  0, /* closeback */
  0, /* closeup */
  1, /* comment */
  1, /* copy */
  1, /* cproc */
  1, /* csd */
  1, /* csu */
  0, /* cut */
  1, /* cutstyle */
  1, /* dbuffer */
  1, /* dcut */
  1, /* debug */
  0, /* detrail */
  0, /* df */
  0, /* dleft */
  0, /* dline */
  0, /* dmarked */
  0, /* drest */
  0, /* dright */
  0, /* dta */
  0, /* dtb */
  0, /* dtwl */
  0, /* dtwr */
  0, /* e */
  1, /* eightbit */
  1, /* endpar */
  1, /* f */
  1, /* fkeystring */
  1, /* fks */
  0, /* format */
  0, /* front */
  0, /* ga */
  0, /* gb */
  0, /* ge */
  1, /* help */
  0, /* i */
  0, /* icurrent */
  1, /* if */
  0, /* iline */
  0, /* ispace */
  1, /* key */
  0, /* lcl */
  1, /* load */
  1, /* loop */
  1, /* m */
  1, /* makebuffer */
  1, /* mark */
  1, /* mouse */
  1, /* n */
  1, /* name */
  1, /* ne */
  1, /* newbuffer */
  1, /* overstrike */
  1, /* p */
  1, /* pa */
  0, /* paste */
  1, /* pb */
  1, /* pbuffer */
  1, /* pll */
  1, /* plr */
  1, /* proc */
  1, /* prompt */
  1, /* quit */
  1, /* readonly */
  1, /* refresh */
  0, /* renumber */
  1, /* repeat */
  1, /* rmargin */
  0, /* sa */
  1, /* save */
  0, /* sb */
  1, /* set */
  1, /* show */
  1, /* stop */
  1, /* subchar */
  1, /* t */
  1, /* title */
  1, /* tl */
  1, /* topline */
  0, /* ucl */
  0, /* undelete */
  0, /* unformat */
  1, /* unless */
  1, /* until */
  1, /* uteof */
  1, /* verify */
  1, /* w */
  1, /* warn */
  1, /* while */
  1, /* wide */
  1, /* word */
  1, /* write */

/* The single-character special commands have ids that follow on from the
command words. Keep this in step with the string just below. */

  1, /* * */
  1, /* ? */
  1, /* > */
  1, /* < */
  0, /* # */
  0, /* $ */
  0, /* % */
  0, /* ~ */

/* Finally, bracketed sequences and procedures use values that follow. */

  1, /* brackets */
  1  /* procedure */
};

/* Single-character special commands; we have star at the front of the string
to allocate it an id, though it is never matched via this string. If ever this
is changed, keep the readonly table above in step. */

static uschar *xcmdlist = US"*?><#$%~";

/* The ids for bracketed sequences of commands and command procedures follow
on the end, after those for named commands and single-character commands. */

#define cmd_specialend (cmd_specialbase + (int)Ustrlen(xcmdlist))
#define cmd_sequence cmd_specialend
#define cmd_obeyproc (cmd_specialend+1)

/* Allow for mutual recursion between command sequence and command compile */

static cmdstr *CompileSequence(int *endscolon);



/*************************************************
*         Compile "system" command               *
*************************************************/

/* The "system" command is a command line that begins with an asterisk. */

static cmdstr *
CompileSysLine(void)
{
stringstr *string = store_Xget(sizeof(stringstr));
cmdstr *yield = cmd_getcmdstr(cmd_specialbase);   /* "*" is the first special */

yield->flags |= cmdf_arg1 + cmdf_arg1F;

string->type = cb_sttype;
string->delim = 0;
string->hexed = FALSE;
string->text = store_copystring(cmd_ptr + 1);

while (*cmd_ptr) cmd_ptr++;
yield->arg1.string = string;
return yield;
}



/*************************************************
*             Compile one command                *
*************************************************/

cmdstr *
cmd_compile(void)
{
int found = -1;
int count = cmd_readnumber();
if (count < 0) count = 1;

SpecialCmd = FALSE;     /* indicates special one-character cmd */
cmd_ist = 0;            /* implicit string terminator */

/* Attempt to read a word; yields "" if non-letter found */

cmd_readword();

/* Cases where there is no word */

if (cmd_word[0] == 0)
  {
  if (cmd_atend() || (*cmd_ptr == '\\' && (main_oldcomment || cmd_ptr[1] == '\\')))
    return NULL;   /* End of line */

  /* Deal with bracketed groups */

  if (*cmd_ptr == '(')
    {
    int dummy;
    cmdstr *yield = cmd_getcmdstr(cmd_sequence);
    yield->count = count;
    cmd_ptr++;

    cmd_bracount++;
    yield->arg1.cmds = CompileSequence(&dummy);
    yield->flags |= cmdf_arg1 | cmdf_arg1F;
    cmd_bracount--;

    /* On successful return from CompileSequence(), cmd_ptr will be pointing at
    the closing parenthesis. Otherwise, a message will have been issued. */

    if (!cmd_faildecode) cmd_ptr++;
    return yield;
    }

  /* Deal with procedure call */

  else if (*cmd_ptr == '.')
    {
    stringstr *name;
    if (cmd_readprocname(&name))
      {
      cmdstr *yield = cmd_getcmdstr(cmd_obeyproc);
      yield->count = count;
      yield->arg1.string = name;
      yield->flags |= cmdf_arg1 | cmdf_arg1F;
      return yield;
      }
    else
      {
      cmd_faildecode = TRUE;
      return NULL;
      }
    }

  /* Deal with special character commands */

  else
    {
    uschar *p;
    int c = *cmd_ptr++;
    SpecialCmd = TRUE;
    mac_skipspaces(cmd_ptr);

    /* Pack up multiples by using count */

    while (*cmd_ptr == c)
      {
      count++;
      cmd_ptr++;
      mac_skipspaces(cmd_ptr);
      }

    /* Now search for known command */

    if ((p = Ustrchr(xcmdlist, c)) == NULL)
      {
      uschar s[2];
      s[0] = c;
      s[1] = 0;
      error_moan_decode(10, s);
      return NULL;
      }
    else
      {
      cmdstr *yield = cmd_getcmdstr(cmd_specialbase + p - xcmdlist);
      yield->count = count;
      yield->misc = c;
      if (*cmd_ptr == '^') cmd_ptr++;
      return yield;
      }
    }
  }

/* Else use binary chop to search command word list */

else
  {
  uschar **first = cmd_list;
  uschar **last  = first + cmd_listsize;
  while (last > first)
    {
    int c;
    uschar **cmd = first + (last-first)/2;
    c = Ustrcmp(cmd_word, *cmd);
    if (c == 0)
      {
      found = (cmd - cmd_list);
      break;
      }
    if (c > 0) first = cmd + 1; else last = cmd;
    }
  }

/* If a valid command word is found, get a control block and call the
command-specific compiling routine to read the arguments. Before doing this, we
skip a single '^' character in the input, if present. This character can
therefore be used as a delimiter in -opt and -init strings, avoiding the need
for quoting for spaces. */

if (found >= 0)
  {
  cmdstr *yield = cmd_getcmdstr(found);
  yield->count = count;
  if (*cmd_ptr == '^') cmd_ptr++;
  (cmd_Cproclist[found])(yield);
  return yield;
  }

else
  {
  if (Ustrcmp(cmd_word, "else") == 0) error_moan_decode(9); /* misplaced else */
    else error_moan_decode(10, cmd_word);                   /* unknown command */
  return NULL;
  }
}


/*************************************************
*       Compile Sequence until ')' or end        *
*************************************************/

/*
Argument:    variable to set TRUE if line ended with a semicolon
Returns:     pointer to the first cmdstr in a chain or NULL if first is bad
*/

static cmdstr *
CompileSequence(BOOL *endscolon)
{
cmdstr *yield = NULL;
cmdstr *last = NULL;
BOOL firsttime = TRUE;

SpecialCmd = TRUE;       /* no advance cmdptr first time */

/* Loop compiling commands - repeat condition is at the bottom */

do
  {
  *endscolon = FALSE;

  if (*cmd_ptr == ';' || SpecialCmd)
    {
    cmdstr *next;
    if (!SpecialCmd) cmd_ptr++;
    next = cmd_compile();
    mac_skipspaces(cmd_ptr);
    if (last == NULL) yield = next; else last->next = next;
    if (next == NULL)
      {
      if (!firsttime) *endscolon = TRUE;
      }
    else last = next;
    }
  else
    {
    error_moan_decode(8);
    break;
    }

  /* Deal with the case of a logical command line extending over more than one
  physical command line. */

  while (!cmd_faildecode && cmd_bracount > 0 && (*cmd_ptr == 0 || *cmd_ptr == '\n' ||
           (*cmd_ptr == '\\' && (main_oldcomment || cmd_ptr[1] == '\\'))))
    {
    if (main_interrupted(ci_read)) { cmd_faildecode = TRUE; break; }
    cmd_joinline(FALSE);      /* join on next line, error if eof */
    }                         /* cmd_ptr will be at an initial semicolon */

  firsttime = FALSE;
  }
while (!cmd_faildecode && *cmd_ptr != 0 && *cmd_ptr != '\n' &&
  (*cmd_ptr != '\\' || (!main_oldcomment && cmd_ptr[1] != '\\')) && *cmd_ptr != ')');

return yield;
}



/*************************************************
*        Compile Command Line                    *
*************************************************/

/* The global variable faildecode must be set to indicate success/failure.

Arguments:
   cmdline    the line being compiled; may be changed by CompileSequence if
              the line is continued onto other physical input lines
   endscolon  must be set TRUE if the line ended in a semicolon

Returns:      pointer to the first cmdstr in a chain
              or NULL for an empty line or if the first command is bad
*/

static cmdstr *
CompileCmdLine(uschar *cmdline, BOOL *endscolon)
{
cmdstr *yield;
cmd_faildecode = FALSE;
cmd_cmdline = cmd_ptr = cmdline;
cmd_bracount = 0;

mac_skipspaces(cmd_ptr);
*endscolon = FALSE;

if (*cmd_ptr == '*') yield = CompileSysLine(); else
  {
  yield = CompileSequence(endscolon);
  if (*cmd_ptr != 0 && *cmd_ptr != '\n' &&
       (*cmd_ptr != '\\' || (!main_oldcomment && cmd_ptr[1] != '\\')) &&
       !cmd_faildecode)
    {
    error_moan_decode(7);             /* unmatched ')' */
    }
  }

return yield;
}



/*************************************************
*                Obey command line               *
*************************************************/

/* Command line equals "sequence of commands in brackets". This function is
global because it's called from a number of places.

Argument:  pointer to the first compiled command
Returns:   a done_xxx value
*/

int
cmd_obeyline(cmdstr *cmd)
{
int yield = done_continue;

if (cmd == NULL) return done_continue;

if (++cmd_bracount > 300)
  {
  error_moan(31);      /* nesting level too deep */
  return done_error;
  }

/* For each command, obey it <count> times, provided there are no errors.
Single-char commands take care of the count themselves, so are called only
once. They are identified by high-valued ids. */

while (cmd != NULL && yield == done_continue)
  {
  int count;

  /* Check for disallowed commands in readonly mode. */

  if (main_readonly && !cmd_readonly[(usint)(cmd->id)])
    {
    uschar *cmdname;
    uschar temp[4];

    if (cmd->id < cmd_specialbase)
      cmdname = cmd_list[(usint)(cmd->id)];
    else if (cmd->id < cmd_sequence)
      {
      temp[0] = xcmdlist[cmd->id - cmd_specialbase];
      temp[1] = 0;
      cmdname = temp;
      }

    /* These ones should never occur, but don't skimp on the code. We
    can't use a switch because cmd_sequence and cmd_obeyproc are not
    constants. */

    /* LCOV_EXCL_START */
    else if (cmd->id == cmd_sequence)
      cmdname = US"bracketed sequence";
    else if (cmd->id == cmd_obeyproc)
      cmdname = US"command procedure";
    else cmdname = US"unknown command";
    /* LCOV_EXCL_STOP */

    error_moan(52, cmdname);
    yield = done_error;
    break;
    }

  /* OK the command is permitted. */

  count =
    ((int)(cmd->id) >= cmd_specialbase && (int)(cmd->id) < cmd_specialend)?
      1 : cmd->count;

  while (count-- > 0)
    {
    if (main_interrupted(ci_cmd)) return done_error;

    /* Now obey the command, maintaining the BACK flag (?). The
    main_leave_message flag is set if the command leaves a message in the
    message window in screen mode. */

    main_leave_message = FALSE;
    yield = (cmd_Eproclist[(usint)(cmd->id)])(cmd);

    /* Commands that generate output (e_g. SHOW) return done_wait; in screen
    mode, if there are more commands to follow on the line, we take a pause
    here. Otherwise we pass back done_wait and the mainline will read more
    commands instead of reverting to screen. */

    if (yield == done_wait)
      {
      if (main_screenOK)
        {
        if (cmd_bracount == 1 && cmd->next == NULL && count == 0) break; else
          {
          yield = done_continue;
          scrn_rdline(FALSE, US"Press RETURN to continue ");
          error_printf("\n");
          }
        }
      else if (!main_screenmode) yield = done_continue;
      }
    else if (yield != done_continue) break;
    }

/***** This wasn't present in the old E, and isn't right here.
We need to invent code to support break & loop in bracketed
groups that are repeated by repeat counts. Putting this here
unqualified applies them to individual commands. Have to
test for bracket groups??

  if (yield == done_loop || yield == done_break)
    {
    if (--cmd_breakloopcount > 0) break;
    if (yield == done_break) yield = done_continue;
    }
*****/

  /* Carry on with next command */

  cmd = cmd->next;
  }

cmd_bracount--;
return yield;
}



/*************************************************
*     Obey bracketed sequence of commands        *
*************************************************/

int
e_sequence(cmdstr *cmd)
{
return cmd_obeyline(cmd->arg1.cmds);
}



/*************************************************
*               Handle command line              *
*************************************************/

/* The command line may be in fixed store, and so is not freed.

Argument:   pointer to command line
Returns:    one of the done_xxx values
*/

int
cmd_obey(uschar *cmdline)
{
int yield = done_error;
BOOL endscolon;
cmdstr *compiled;

main_cicount = 0;
compiled = CompileCmdLine(cmdline, &endscolon);

/* Save the command line, whether or not it compiled correctly, unless it is
null or identical to the previous line. */

if (cmdline[0] != 0 &&
      (cmd_stackptr == 0 || Ustrcmp(cmd_stack[cmd_stackptr-1], cmdline) != 0))
  {
  cmd_stack[cmd_stackptr++] = store_copystring(cmdline);
  if (cmd_stackptr > cmd_stacktop)
    {
    int i;
    store_free(cmd_stack[0]);
    for (i = 0; i < cmd_stacktop; i++) cmd_stack[i] = cmd_stack[i+1];
    cmd_stackptr--;
    }
  }

/* If successfully decoded, obey the commands and then free the store */

if (!cmd_faildecode)
  {
  cmd_onecommand = (compiled == NULL)? TRUE :
    (compiled->next == NULL && (compiled->flags & cmdf_group) == 0);
  cmd_bracount = 0;
  cmd_eoftrap = FALSE;
  cmd_refresh = FALSE;
  if ((yield = cmd_obeyline(compiled)) == done_finish) main_done = TRUE;
  cmd_freeblock((cmdblock *)compiled);
  }

return yield;
}

/* End of ecmdcomp.c */
