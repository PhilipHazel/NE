/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */


/* This file contains code for reading the arguments of commands or infering
data from the command name. */


#include "ehdr.h"
#include "cmdhdr.h"

static void c_iline(cmdstr *); /* Used before defined */
static void c_save(cmdstr *);  /* Used before defined */



/*************************************************
*            Command with no arguments           *
*************************************************/

static void
noargs(cmdstr *c)
{
(void)c;
}



/*************************************************
*          Command with an ON/OFF argument       *
*************************************************/

/* The argument is optional; its absence means "flip". */

static void
c_onoff(cmdstr *cmd)
{
cmd_readword();
if (cmd_word[0] != 0)
  {
  cmd->flags |= cmdf_arg1;
  if (Ustrcmp(cmd_word, "on") == 0) cmd->arg1.value = TRUE;
  else if (Ustrcmp(cmd_word, "off") == 0) cmd->arg1.value = FALSE;
  else error_moan_decode(13, "\"on\" or \"off\"");
  }
}



/*************************************************
*     Align, Closeup, Dleft, Dline & Dright      *
*************************************************/

static void c_align(cmdstr *c)     { c->misc = lb_align; }
static void c_alignp(cmdstr *c)    { c->misc = lb_alignp; }
static void c_closeback(cmdstr *c) { c->misc = lb_closeback; }
static void c_closeup(cmdstr *c)   { c->misc = lb_closeup; }
static void c_dleft(cmdstr *c)     { c->misc = lb_eraseleft; }
static void c_dline(cmdstr *c)     { c->misc = lb_delete; }
static void c_dright(cmdstr *c)    { c->misc = lb_eraseright; }



/*************************************************
*         The xA, xB and xE commands             *
*************************************************/

/* This local function is used for GA, GE, GB, A, B, E.

Arguments:
  cmd       the command structure
  gflag     TRUE for the G commands
  misc      value to set in the misc field

Returns:    nothing
*/

static void
c_abe(cmdstr *cmd, BOOL gflag, int misc)
{
cmd->misc = misc;

if (cmd_atend()) return;    /* no argument supplied */
match_L = FALSE;            /* for compiling regular expressions */

/* Check for prompted first arg, or read first arg */

if (cmd_readse(&cmd->arg1.se))
  {
  cmd->flags |= cmdf_arg1 + cmdf_arg1F;
  if (gflag && (cmd->arg1.se)->type == cb_qstype)
    {
    qsstr *q = cmd->arg1.qs;
    if (q->length == 0 && (q->flags & (qsef_B+qsef_E)) == 0)
      {
      error_moan_decode(27);
      return;
      }
    }
  }
else
  {
  cmd_faildecode = TRUE;
  return;
  }

/* Check for prompted second arg, or read second arg */

if (cmd_readqualstr(&cmd->arg2.qs, rqs_XRonly))
  cmd->flags |= cmdf_arg2 + cmdf_arg2F;
else { cmd_faildecode = TRUE; return; }
}

static void c_ga(cmdstr *cmd) { c_abe(cmd, TRUE, abe_a); }
static void c_gb(cmdstr *cmd) { c_abe(cmd, TRUE, abe_b); }
static void c_ge(cmdstr *cmd) { c_abe(cmd, TRUE, abe_e); }

static void c_a(cmdstr *cmd) { c_abe(cmd, FALSE, abe_a); }
static void c_b(cmdstr *cmd) { c_abe(cmd, FALSE, abe_b); }
static void c_e(cmdstr *cmd) { c_abe(cmd, FALSE, abe_e); }



/*************************************************
*            The yA, yB  commands                *
*************************************************/

/* This is used for PA, PB, SA, SB, DTA, DTB */

static void
c_ab(cmdstr *cmd, int misc)
{
cmd->misc = misc;
if (cmd_readse(&cmd->arg1.se)) cmd->flags |= cmdf_arg1 + cmdf_arg1F;
  else cmd_faildecode = TRUE;
}

static void c_pa(cmdstr *cmd) { c_ab(cmd, abe_a); }
static void c_pb(cmdstr *cmd) { c_ab(cmd, abe_b); }



/*************************************************
*            The BACKREGION command              *
*************************************************/

static void
c_backregion(cmdstr *cmd)
{
cmd->arg1.value = cmd_readnumber();
if (cmd->arg1.value >= 0) cmd->flags |= cmdf_arg1;
}



/*************************************************
*             The BACKUP command                 *
*************************************************/

/* Currently very restricted */

static void
c_backup(cmdstr *cmd)
{
cmd_readword();
if (cmd_word[0] != 0 && Ustrcmp(cmd_word, "files") == 0)
  {
  cmd->misc = backup_files;
  c_onoff(cmd);
  }
else error_moan_decode(13, "\"files\"");
}



/*************************************************
*            The BEGINPAR command                *
*************************************************/

/* Also used for endpar */

static void
c_beginpar(cmdstr *cmd)
{
if (cmd_readse(&cmd->arg1.se)) cmd->flags |= cmdf_arg1 | cmdf_arg1F;
  else cmd_faildecode = TRUE;    /* message already given */
}



/*************************************************
*       The {C,CD,P}BUFFER commands              *
*************************************************/

static void
allbuffer(cmdstr *cmd, int misc)
{
cmd->misc = misc;
cmd->arg1.value = cmd_readnumber();
if (cmd->arg1.value >= 0) cmd->flags |= cmdf_arg1;
}

static void c_buffer(cmdstr *cmd)   { allbuffer(cmd, FALSE); }
static void c_pbuffer(cmdstr *cmd)  { allbuffer(cmd, TRUE); }
static void c_cbuffer(cmdstr *cmd)  { allbuffer(cmd, cbuffer_c); }
static void c_cdbuffer(cmdstr *cmd) { allbuffer(cmd, cbuffer_cd); }




/*************************************************
*              The BREAK command                 *
*************************************************/

static void
c_break(cmdstr *cmd)
{
cmd->arg1.value = cmd_readnumber();
if (cmd->arg1.value >= 0) cmd->flags |= cmdf_arg1;
}



/*************************************************
*             The COMMENT command                *
*************************************************/

static void
c_comment(cmdstr *cmd)
{
int rc = cmd_readstring(&cmd->arg1.string);
if (rc <= 0)
  {
  cmd_faildecode = TRUE; 
  if (rc == 0) error_moan(13, "string");
  }
else cmd->flags |= cmdf_arg1;
}



/*************************************************
*            The CL command                      *
*************************************************/

static void
c_cl(cmdstr *cmd)
{
if (!cmd_atend()) c_iline(cmd);
}



/*************************************************
*            The CPROC command                   *
*************************************************/

static void
c_cproc(cmdstr *cmd)
{
if (!cmd_readprocname(&(cmd->arg1.string))) cmd_faildecode = TRUE;
  else cmd->flags |= cmdf_arg1;
}



/*************************************************
*             The CUTSTYLE command               *
*************************************************/

static void
c_cutstyle(cmdstr *cmd)
{
cmd_readword();
if (cmd_word[0] != 0)
  {
  cmd->flags |= cmdf_arg1;
  if (Ustrcmp(cmd_word, "append") == 0) cmd->arg1.value = TRUE;
  else if (Ustrcmp(cmd_word, "replace") == 0) cmd->arg1.value = FALSE;
  else error_moan_decode(13, "\"append\" or \"replace\"");
  }
}



/*************************************************
*           The DEBUG command                    *
*************************************************/

/* This command is provided for the maintainer to force crashes in order to
test the crash-handling code. It won't be used in normal testing. */

/* LCOV_EXCL_START */

static void
c_debug(cmdstr *cmd)
{
cmd_readword();
if (Ustrcmp(cmd_word, "crash") == 0)
  {
  cmd->arg1.value = debug_crash;
  cmd->flags |= cmdf_arg1;
  }
else if (Ustrcmp(cmd_word, "exceedstore") == 0)
  {
  cmd->arg1.value = debug_exceedstore;
  cmd->flags |= cmdf_arg1;
  }
if (Ustrcmp(cmd_word, "nullline") == 0)
  {
  cmd->arg1.value = debug_nullline;
  cmd->flags |= cmdf_arg1;
  }
if (Ustrcmp(cmd_word, "baderror") == 0)
  {
  cmd->arg1.value = debug_baderror;
  cmd->flags |= cmdf_arg1;
  }
}

/* LCOV_EXCL_STOP */



/*************************************************
*               The DETRAIL command              *
*************************************************/

static void
c_detrail(cmdstr *cmd)
{
cmd_readword();
if (cmd_word[0] != 0)
  {
  if (Ustrcmp(cmd_word, "output") == 0) cmd->misc = detrail_output;
  else error_moan_decode(13, "\"output\"");
  }
else cmd->misc = detrail_buffer;
}



/*************************************************
*             The F & BF commands                *
*************************************************/

static void
c_f_bf(cmdstr *cmd)
{
cmd->misc = match_L;         /* Save for match time */
if (!cmd_atend())            /* Argument provided */
  {
  if (cmd_readse(&cmd->arg1.se))
    {
    cmd->flags |= cmdf_arg1 + cmdf_arg1F;
    }
  else cmd_faildecode = TRUE;    /* message already given */
  }
}

/* We must set match_L for compiling regular expressions */

static void c_bf(cmdstr *cmd) { match_L = TRUE;  c_f_bf(cmd); }
static void c_f(cmdstr *cmd)  { match_L = FALSE; c_f_bf(cmd); }



/*************************************************
*            The FKEYSTRING command              *
*************************************************/

static void
c_fks(cmdstr *cmd)
{
int n = cmd_readnumber();
if (1 <= n  && n <= max_keystring)
  {
  int rc = cmd_readstring(&cmd->arg2.string);
  if (rc < 0) cmd_faildecode = TRUE; else
    {
    cmd->arg1.value = n;
    cmd->flags |= cmdf_arg1;
    if (rc) cmd->flags |= cmdf_arg2;
    }
  }
else if (n < 0) error_moan_decode(13, "Number");
else error_moan_decode(35, max_keystring);
}



/*************************************************
*             The IF command                     *
*************************************************/

/* A subroutine used by IF, UNLESS, WHILE, UNTIL.

Arguments:
  cmd       command structure
  misc      flags for the misc field
  proc      function to read command sequence arg(s)

Returns:    nothing
*/

static void
ifulwhut(cmdstr *cmd, int misc, void (*proc)(cmdstr *, int))
{
uschar *savecmdptr = cmd_ptr;
match_L = FALSE;    /* for compiling regular expression */

/* First read a condition, which is one of a number of special words, or a
search expression */

cmd_readword();

if (Ustrcmp(cmd_word, "mark") == 0) misc |= if_mark;
else if (Ustrcmp(cmd_word, "eol") == 0) misc |= if_eol;
else if (Ustrcmp(cmd_word, "sol") == 0) misc |= if_sol;
else if (Ustrcmp(cmd_word, "sof") == 0) misc |= if_sof;
else if (Ustrcmp(cmd_word, "prompt") == 0)
  {
  misc |= if_prompt;
  if (cmd_readstring(&cmd->arg1.string) <= 0)
    {
    cmd_faildecode = TRUE;
    return;
    }
  cmd->flags |= cmdf_arg1 | cmdf_arg1F;
  }
else if (Ustrcmp(cmd_word, "eof") != 0)
  {
  cmd_ptr = savecmdptr;
  if (!cmd_readse(&cmd->arg1.se))
    { cmd_faildecode = TRUE; return; }
  cmd->flags |= cmdf_arg1 | cmdf_arg1F;
  }

/* Insist on "THEN" or "DO" */

cmd_readword();
if (Ustrcmp(cmd_word, "then") != 0 && Ustrcmp(cmd_word, "do") != 0)
  {
  error_moan_decode(13, "\"then\" or \"do\"");
  return;
  }

/* Call given function to read one or two command sequence args. */

proc(cmd, misc);
}


/* Subroutine used by IF and UNLESS to read two alternative command groups.
They are packed up into a single argument. */

static void
ifularg2(cmdstr *cmd, int misc)
{
uschar *saveptr = NULL;
ifstr *arg = store_Xget(sizeof(ifstr));

cmd->arg2.ifelse = arg;
cmd->flags |= cmdf_arg2 | cmdf_arg2F;
cmd->misc = misc;

arg->type = cb_iftype;
arg->if_then = cmd_compile();
arg->if_else = NULL;
if (cmd_faildecode) return;

/* We must now cope with optional else clauses. People forget whether a
semicolon is or is not allowed, so we are forgiving about this, since there is
no ambguity. Inside brackets, or non-interactively, we allow continuation lines
for else, provided the first line does not end in a closing bracket. If we
don't find else, we must terminate the if and restore the pointer. On the other
hand, if we don't find else on a single line, moan. */

/* First skip over semicolons, setting the restart at the last one (since a
sequence of them is the same as one). */

mac_skipspaces(cmd_ptr);
while (*cmd_ptr == ';')
  {
  saveptr = cmd_ptr++;
  while (*cmd_ptr == ' ') cmd_ptr++;
  }

/* Deal with unbracketted, interactive case. */

if (cmd_bracount <= 0 && main_interactive && cmdin_fid == NULL)
  {
  if (cmd_atend() || *cmd_ptr == '\\') return;
  }
else

/* Deal with the bracketted case -- may have to join many lines until we find
one that does not consist solely of spaces or semicolons. */

for (;;)
  {
  if (*cmd_ptr == ')') return;
  if (cmd_atend() || *cmd_ptr == '\\')
    {
    if (!cmd_joinline(TRUE)) return;    /* return if eof */
    saveptr = cmd_ptr;                  /* the joining semicolon */
    while (*cmd_ptr == ';' || *cmd_ptr == ' ') cmd_ptr++;
    }
  else break;
  }

/* We now have something else to read, either on the original unbracketted
line, or on a concatenated line. If it is else, read commands; if not, reset
the pointer for reading a new command if a semicolon or end of line terminated
the THEN part. Otherwise complain. */

cmd_readword();
if (Ustrcmp(cmd_word, "else") == 0) arg->if_else = cmd_compile(); else
  {
  if (saveptr != NULL) { cmd_ptr = saveptr; return; }
  error_moan_decode(13, "else");
  }
}


/* The calling point for IF */

static void c_if(cmdstr *cmd)
{
ifulwhut(cmd, if_if, ifularg2);
}



/*************************************************
*              The ILINE command                 *
*************************************************/

static void
c_iline(cmdstr *cmd)
{
if (cmd_readqualstr(&cmd->arg1.qs, rqs_Xonly))
  cmd->flags |= cmdf_arg1 | cmdf_arg1F;
else cmd_faildecode = TRUE;
}



/*************************************************
*            The KEY command                     *
*************************************************/

static void
c_key(cmdstr *cmd)
{
int rc = cmd_readUstring(&cmd->arg1.string);
if (rc < 0) cmd_faildecode = TRUE;
else if (rc == 0)
  {
  error_moan_decode(13, "Key definition(s)");
  }
else
  {
  cmd->flags |= cmdf_arg1;
  cmd_faildecode = !key_set(cmd->arg1.string->text, FALSE);  /* syntax check */
  }
}



/*************************************************
*                The M command                   *
*************************************************/

/* This function is also used by the T and TL commands. */

static void
c_m(cmdstr *cmd)
{
cmd->arg1.value = cmd_readnumber();
if (cmd->arg1.value < 0)
  {
  if (*cmd_ptr == '*') cmd_ptr++; else
    {
    error_moan_decode(13, "Number or \"*\"");
    return;
    }
  }
cmd->flags |= cmdf_arg1;
}



/*************************************************
*           The MAKEBUFFER command               *
*************************************************/

static void
c_makebuffer(cmdstr *cmd)
{
cmd->arg2.value = cmd_readnumber();
if (cmd->arg2.value < 0)
  {
  error_moan_decode(13, "Number");
  return;
  }
cmd->flags |= cmdf_arg2;
c_save(cmd);
}



/*************************************************
*           The MARK command                     *
*************************************************/

static void
c_mark(cmdstr *cmd)
{
cmd_readword();
if (Ustrcmp(cmd_word, "limit") == 0) cmd->misc = amark_limit;
else if (Ustrcmp(cmd_word, "line") == 0 || Ustrcmp(cmd_word, "lines") == 0)
  {
  if (cmd_atend()) cmd->misc = amark_line; else
    {
    cmd_readword();
    if (Ustrcmp(cmd_word, "hold") == 0) cmd->misc = amark_hold;
      else error_moan_decode(13, "\"hold\"");
    }
  }
else if (Ustrcmp(cmd_word,"rectangle") == 0)
  cmd->misc = amark_rectangle;
else if (Ustrcmp(cmd_word,"text") == 0)
  cmd->misc = amark_text;
else if (Ustrcmp(cmd_word,"unset") == 0)
  cmd->misc = amark_unset;
else error_moan_decode(13, "\"limit\", \"line\", \"rectangle\", \"text\" or \"unset\"");
}



/*************************************************
*           The NAME command                     *
*************************************************/

/* Subroutine used by the NAME and TITLE commands. The argument for the former
is syntax checked. */

static void
readfilename(cmdstr *cmd, BOOL checkflag)
{
if (cmd_atend())
  {
  error_moan_decode(13, "File name");
  }
else
  {
  int rc = ((ch_tab[(usint)(*cmd_ptr)] & ch_filedelim) == 0)?
    cmd_readUstring(&cmd->arg1.string) : cmd_readstring(&cmd->arg1.string);

  if (rc < 0) cmd_faildecode = TRUE; else    /* syntax error */
    {
    uschar *rcm = checkflag? sys_checkfilename((cmd->arg1.string)->text) : NULL;
    cmd->flags |= cmdf_arg1 + cmdf_arg1F;
    if (rcm != NULL) error_moan_decode(12, (cmd->arg1.string)->text, rcm);
    }
  }
}

static void c_name(cmdstr *cmd)  { readfilename(cmd, TRUE); }    /* with sys check */
static void c_namex(cmdstr *cmd) { readfilename(cmd, FALSE); }   /* without ditto */



/*************************************************
*            The PLL & PLR commands              *
*************************************************/

static void c_pll(cmdstr *cmd) { cmd->misc = abe_b; }
static void c_plr(cmdstr *cmd) { cmd->misc = abe_a; }



/*************************************************
*            The PROC command                    *
*************************************************/

static void
c_proc(cmdstr *cmd)
{
if (!cmd_readprocname(&(cmd->arg1.string)))
  {
  cmd_faildecode = TRUE;
  return;
  }
cmd->flags |= cmdf_arg1;
cmd_readword();
if (Ustrcmp(cmd_word, "is") == 0)
  {
  cmdstr *body = cmd_compile();
  if (cmd_faildecode) return;
  cmd->arg2.cmds = body;
  if (body != NULL) cmd->flags |= cmdf_arg2;
  }
else error_moan_decode(13, "\"is\"");
}



/*************************************************
*            The REPEAT command                  *
*************************************************/

static void
c_repeat(cmdstr *cmd)
{
cmd->arg1.cmds = cmd_compile();
cmd->flags |= cmdf_arg1 | cmdf_arg1F;
}



/*************************************************
*           The RMARGIN command                  *
*************************************************/

static void
c_rmargin(cmdstr *cmd)
{
cmd_readword();
if (cmd_word[0] != 0)
  {
  cmd->flags |= cmdf_arg2;
  if (Ustrcmp(cmd_word, "on") == 0) cmd->arg2.value = TRUE;
  else if (Ustrcmp(cmd_word, "off") == 0) cmd->arg2.value = FALSE;
  else error_moan_decode(13, "\"on\" or \"off\" or a number");
  }
else
  {
  cmd->arg1.value = cmd_readnumber();
  if (cmd->arg1.value >= 0) cmd->flags |= cmdf_arg1;
  }
}



/*************************************************
*             The SAVE command                   *
*************************************************/

static void
c_save(cmdstr *cmd)
{
if (!cmd_atend()) c_name(cmd);
}



/*************************************************
*              The SET command                   *
*************************************************/

static void
c_set(cmdstr *cmd)
{
cmd_readword();
if (Ustrcmp(cmd_word, "autovscroll") == 0 ||
    Ustrcmp(cmd_word, "autovmousescroll") == 0)
  {
  int n = cmd_readnumber();
  cmd->misc = (cmd_word[5] == 'm')? set_autovmousescroll : set_autovscroll;

  /* If window_depth is zero, it means that NE was called with -line or -with,
  which starts it in line-by-line mode, with no possibility of entering screen
  mode. We allow any value to be set, so that the use of these settings in
  .nerc files doesn't cause an issue when NE is run in non-screen mode. */

  if (window_depth == 0 || (n >= 1 && n <= (int)window_depth))
    {
    cmd->arg1.value = n;
    cmd->flags |= cmdf_arg1;
    }
  else error_moan_decode(34, cmd_word, "not in range 1 to display depth - 1");
  }

else if (Ustrcmp(cmd_word, "splitscrollrow") == 0)
  {
  int n = cmd_readnumber();
  if (n > 0)
    {
    cmd->misc = set_splitscrollrow;
    cmd->arg1.value = n - 1;
    cmd->flags |= cmdf_arg1;
    }
  else error_moan_decode(13, "Positive number");
  }

else if (Ustrcmp(cmd_word, "oldcommentstyle") == 0)
  {
  cmd->misc = set_oldcommentstyle;
  }

else if (Ustrcmp(cmd_word, "newcommentstyle") == 0)
  {
  cmd->misc = set_newcommentstyle;
  }

else   /* unknown SET option */
  {
  error_moan_decode(13, "\"autovscroll\", \"autovmousescroll\", "
    "\"splitscrollrow\", \"oldcommentstyle\", or \"newcommentstyle\"");
  }
}



/*************************************************
*              The SHOW command                  *
*************************************************/

static void
c_show(cmdstr *cmd)
{
cmd_readword();
if (Ustrcmp(cmd_word, "ckeys") == 0)            cmd->misc = show_ckeys;
else if (Ustrcmp(cmd_word, "fkeys") == 0)       cmd->misc = show_fkeys;
else if (Ustrcmp(cmd_word, "xkeys") == 0)       cmd->misc = show_xkeys;
else if (Ustrcmp(cmd_word, "keys") == 0)        cmd->misc = show_allkeys;
else if (Ustrcmp(cmd_word, "keystrings") == 0)  cmd->misc = show_keystrings;
else if (Ustrcmp(cmd_word, "buffers") == 0)     cmd->misc = show_buffers;
else if (Ustrcmp(cmd_word, "wordcount") == 0)   cmd->misc = show_wordcount;
else if (Ustrcmp(cmd_word, "version") == 0)     cmd->misc = show_version;
else if (Ustrcmp(cmd_word, "keyactions") == 0)  cmd->misc = show_actions;
else if (Ustrcmp(cmd_word, "commands") == 0)    cmd->misc = show_commands;
else if (Ustrcmp(cmd_word, "wordchars") == 0)   cmd->misc = show_wordchars;
else if (Ustrcmp(cmd_word, "settings") == 0)    cmd->misc = show_settings;
else if (Ustrcmp(cmd_word, "allsettings") == 0) cmd->misc = show_allsettings;
else
  {
  error_moan_decode(13, "keys, ckeys, fkeys, xkeys, keystrings, keyactions, "
    "buffers, commands,\n   wordchars, wordcount, [all]settings, or version");
  }
}



/*************************************************
*              The SUBCHAR command               *
*************************************************/

static void
c_subchar(cmdstr *cmd)
{
int c;
uschar *eptr = cmd_ptr + Ustrlen(cmd_ptr);
mac_skipspaces(cmd_ptr);
GETCHARINC(c, cmd_ptr, eptr);
if ((ch_displayable[c/8] & (1<<(c%8))) != 0) error_moan_decode(54, c);
else cmd->arg1.value = c;
}



/*************************************************
*             The TL command                     *
*************************************************/

static void
c_tl(cmdstr *cmd)
{
cmd->misc = TRUE;
c_m(cmd);
}



/*************************************************
*             The UNLESS command                 *
*************************************************/

static void c_unless(cmdstr *cmd)
{
ifulwhut(cmd, if_unless, ifularg2);
}



/*************************************************
*         The UNTIL & UTEOF commands             *
*************************************************/

/* Subroutine used by UNTIL && WHILE to read second argument */

static void
utwharg2(cmdstr *cmd, int misc)
{
cmd->misc = misc;
cmd->arg2.cmds = cmd_compile();
cmd->flags |= cmdf_arg2 | cmdf_arg2F;

if (cmd->arg2.cmds == NULL)    /* lock out null second arguments */
  error_moan_decode(33, (misc == if_unless)? "until" : "while");
}

static void c_until(cmdstr *cmd) { ifulwhut(cmd, if_unless, utwharg2); }
static void c_uteof(cmdstr *cmd) { utwharg2(cmd, if_unless); }



/*************************************************
*               The WHILE command                *
*************************************************/

static void
c_while(cmdstr *cmd)
{
ifulwhut(cmd, if_if, utwharg2);
}



/*************************************************
*              The WORD command                  *
*************************************************/

/* Only ASCII characters are allowed to be specified. */

static void
c_word(cmdstr *cmd)
{
uschar *p, *pe;
uschar *e = NULL;
int rc = cmd_readstring(&cmd->arg1.string);

if (rc <= 0)
  {
  cmd_faildecode = TRUE; 
  if (rc == 0) error_moan(13, "String");
  return;
  }

cmd->flags |= cmdf_arg1 | cmdf_arg1F;
p = cmd->arg1.string->text;
pe = p + Ustrlen(p);

while (*p != 0)
  {
  int a;
  if (*p == '\"')
    {
    if (*(++p) == 0)
      {
      e = US"unexpected end";
      break;
      }
    }

  GETCHARINC(a, p, pe);
  if (a > 127)
    {
    e = US"Only ASCII characters may be specified";
    break;
    }

  if (*p == '-')
    {
    int b, ta, tb;
    if (*(++p) == 0)
      {
      e = US"unexpected end";
      break;
      }
    GETCHARINC(b, p, pe);
    if (b > 127)
      {
      e = US"Only ASCII characters may be specified";
      break;
      }
    ta = ch_tab[a] & (ch_letter+ch_digit);
    tb = ch_tab[b] & (ch_letter+ch_digit);
    if (ta == 0 || ta != tb)
      {
      e = US"\n   only digits or letters of the same case are allowed in a range";
      break;
      }
    if (b < a)
      {
      e = US"characters out of order in a range";
      break;
      }
    }
  }

if (e != NULL) error_moan_decode(44, line_charcount(cmd->arg1.string->text,
  p - cmd->arg1.string->text), e);
}



/*************************************************
*            Table of arg functions              *
*************************************************/

cmd_Cproc cmd_Cproclist[] = {
  c_a,          /* a */
  noargs,       /* abandon */
  c_align,      /* align */
  c_alignp,     /* alignp */
  c_onoff,      /* attn */
  c_onoff,      /* autoalign */
  c_b,          /* b */
  noargs,       /* back */
  c_backregion, /* backregion */
  c_backup,     /* backup */
  c_beginpar,   /* beginpar */
  c_bf,         /* bf */
  c_break,      /* break */
  c_buffer,     /* buffer */
  c_name,       /* c */
  c_onoff,      /* casematch */
  c_cbuffer,    /* cbuffer */
  c_cdbuffer,   /* cdbuffer */
  noargs,       /* center */
  noargs,       /* centre */
  c_cl,         /* cl */
  c_closeback,  /* closeback */
  c_closeup,    /* closeup */
  c_comment,    /* comment */
  noargs,       /* copy */
  c_cproc,      /* cproc */
  noargs,       /* csd */
  noargs,       /* csu */
  noargs,       /* cut */
  c_cutstyle,   /* cutstyle */
  c_buffer,     /* dbuffer */
  noargs,       /* dcut */
  c_debug,      /* debug */
  c_detrail,    /* detrail */
  c_f,          /* df */
  c_dleft,      /* dleft */
  c_dline,      /* dline */
  noargs,       /* dmarked */
  noargs,       /* drest */
  c_dright,     /* dright */
  c_pa,         /* dta */
  c_pb,         /* dtb */
  noargs,       /* dtwl */
  noargs,       /* dtwr */
  c_e,          /* e */
  c_onoff,      /* eightbit */
  c_beginpar,   /* endpar */
  c_f,          /* f */
  c_fks,        /* fkeystring */
  c_fks,        /* fks */
  noargs,       /* format */
  noargs,       /* front */
  c_ga,         /* ga */
  c_gb,         /* gb */
  c_ge,         /* ge */
  c_show,       /* help is a synonym for show */
  c_save,       /* i */
  noargs,       /* icurrent */
  c_if,         /* if */
  c_iline,      /* iline */
  noargs,       /* ispace */
  c_key,        /* key  */
  noargs,       /* lcl */
  c_name,       /* load */
  c_break,      /* loop */
  c_m,          /* m */
  c_makebuffer, /* makebuffer */
  c_mark,       /* mark */
  c_onoff,      /* mouse */
  noargs,       /* n */
  c_name,       /* name */
  c_save,       /* ne */
  c_save,       /* newbuffer */
  c_onoff,      /* overstrike */
  noargs,       /* p */
  c_pa,         /* pa */
  c_buffer,     /* paste */
  c_pb,         /* pb */
  c_pbuffer,    /* pbuffer */
  c_pll,        /* pll */
  c_plr,        /* plr */
  c_proc,       /* proc */
  c_onoff,      /* prompt */
  noargs,       /* quit */
  c_onoff,      /* readonly */
  noargs,       /* refresh */
  noargs,       /* renumber */
  c_repeat,     /* repeat */
  c_rmargin,    /* rmargin */
  c_pa,         /* sa */
  c_save,       /* save */
  c_pb,         /* sb */
  c_set,        /* set */
  c_show,       /* show */
  noargs,       /* stop */
  c_subchar,    /* subchar */
  c_m,          /* t */
  c_namex,      /* title */
  c_tl,         /* tl */
  noargs,       /* topline */
  noargs,       /* ucl */
  noargs,       /* undelete */
  noargs,       /* unformat */
  c_unless,     /* unless */
  c_until,      /* until */
  c_uteof,      /* ufeof */
  c_onoff,      /* verify */
  noargs,       /* w    */
  c_onoff,      /* warn */
  c_while,      /* while */
  c_onoff,      /* wide */
  c_word,       /* word */
  c_name        /* write */
};



/*************************************************
*            Table of execution functions        *
*************************************************/

/* We put this here for convenience of editing, so that the two tables can be
changed together. Note that the table of command names is in ecmdcomp, and must
be kept in step too. */

cmd_Eproc cmd_Eproclist[] = {
  e_abe,        /* a */
  e_abandon,    /* abandon */
  e_actongroup, /* align */
  e_actongroup, /* alignp */
  e_attn,       /* attn */
  e_autoalign,  /* autoalign */
  e_abe,        /* b */
  e_back,       /* back */
  e_backregion, /* backregion */
  e_backup,     /* backup */
  e_beginpar,   /* beginpar */
  e_f,          /* bf (same function as f) */
  e_break,      /* break */
  e_buffer,     /* buffer */
  e_c,          /* c */
  e_casematch,  /* casematch */
  e_cdbuffer,   /* cbuffer (sic) */
  e_cdbuffer,   /* cdbuffer */
  e_centre,     /* center */
  e_centre,     /* centre */
  e_cl,         /* cl */
  e_actongroup, /* closeback */
  e_actongroup, /* closeup */
  e_comment,    /* comment */
  e_copy,       /* copy */
  e_cproc,      /* cproc */
  e_csd,        /* csd */
  e_csu,        /* csu */
  e_cut,        /* cut */
  e_cutstyle,   /* cutstyle */
  e_dbuffer,    /* dbuffer */
  e_dcut,       /* dcut */
  e_debug,      /* debug */
  e_detrail,    /* detrail */
  e_df,         /* df */
  e_actongroup, /* dleft */
  e_actongroup, /* dline */
  e_dmarked,    /* dmarked */
  e_drest,      /* drest */
  e_actongroup, /* dright */
  e_dtab,       /* dta */
  e_dtab,       /* dtb */
  e_dtwl,       /* dtwl */
  e_dtwr,       /* dtwr */
  e_abe,        /* e */
  e_eightbit,   /* eightbit */
  e_endpar,     /* endpar */
  e_f,          /* f */
  e_fks,        /* fkeystring */
  e_fks,        /* fks */
  e_format,     /* format */
  e_front,      /* front */
  e_g,          /* ga */
  e_g,          /* gb */
  e_g,          /* ge */
  e_show,       /* help is a synonym for show*/
  e_i,          /* i */
  e_icurrent,   /* icurrent */
  e_if,         /* if */
  e_iline,      /* iline */
  e_ispace,     /* ispace */
  e_key,        /* key  */
  e_lcl,        /* lcl */
  e_load,       /* load */
  e_loop,       /* loop */
  e_m,          /* m */
  e_makebuffer, /* makebuffer */
  e_mark,       /* mark */
  e_mouse,      /* mouse */
  e_n,          /* n */
  e_name,       /* name */
  e_newbuffer,  /* ne */
  e_newbuffer,  /* newbuffer */
  e_overstrike, /* overstrike */
  e_p,          /* p */
  e_pab,        /* pa */
  e_paste,      /* paste */
  e_pab,        /* pb */
  e_buffer,     /* pbuffer */
  e_plllr,      /* pll */
  e_plllr,      /* plr */
  e_proc,       /* proc */
  e_prompt,     /* prompt */
  e_stop,       /* quit */
  e_readonly,   /* readonly */
  e_refresh,    /* refresh */
  e_renumber,   /* renumber */
  e_repeat,     /* repeat */
  e_rmargin,    /* rmargin */
  e_sab,        /* sa */
  e_save,       /* save */
  e_sab,        /* sb */
  e_set,        /* set */
  e_show,       /* show */
  e_stop,       /* stop */
  e_subchar,    /* subchar */
  e_ttl,        /* t */
  e_title,      /* title */
  e_ttl,        /* tl */
  e_topline,    /* topline */
  e_ucl,        /* ucl */
  e_undelete,   /* undelete */
  e_unformat,   /* unformat */
  e_if,         /* unless */
  e_while,      /* until */
  e_while,      /* uteof */
  e_verify,     /* verify */
  e_w,          /* w    */
  e_warn,       /* warn */
  e_while,      /* while */
  e_wide,       /* wide */
  e_word,       /* word */
  e_write,      /* write */

  /* Special commands follow on directly. They are all handled by a single
  function (the command character being in cmd->misc). */

  e_star,       /* *command */
  e_singlechar, /* ? all */
  e_singlechar, /* > are */
  e_singlechar, /* < handled */
  e_singlechar, /* # by */
  e_singlechar, /* $ the */
  e_singlechar, /* % same */
  e_singlechar, /* ~ function */

  /* Bracketed group and procedure commands follow at the end */

  e_sequence,   /* sequence in brackets */
  e_obeyproc    /* procedure call */
};

/* End of ecmdarg.c */
