/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */


/* This file contains initializing code, including the main program, which is
the entry point to NE. It also contains termination code. */

#include "ehdr.h"
#include "cmdhdr.h"



/*************************************************
*            Interrupt handler                   *
*************************************************/

/* We just set a flag. Resetting the trap happens later when the signal is
noticed. This allows two interrupts to kill a looping NE. */

/* LCOV_EXCL_START */
static void
sigint_handler(int sig)
{
(void)sig;
main_escape_pressed = TRUE;
}
/* LCOV_EXCL_STOP */



/*************************************************
*        Interrupt handler during fgets          *
*************************************************/

/* This applies when reading a ling of commands in line-by=line mode. */

static jmp_buf rdline_env;

/* LCOV_EXCL_START */
static void
fgets_sigint_handler(int sig)
{
(void)sig;
main_escape_pressed = TRUE;
longjmp(rdline_env, 1);
}
/* LCOV_EXCL_STOP */



/*************************************************
*            Test for interruption               *
*************************************************/

/* The call to sys_checkinterrupt() allows for checking in addition to the
normal signal handler. Used in Unix to check for ctrl/c explicitly in raw input
mode. To help avoid obeying expensive system calls too often, a count of calls
since the last command line was read is maintained, and the type of activity in
progress at the time of the call is also passed on.

Argument:  one of the ci_xxx enumerations
Returns:   TRUE if interrupted
*/

BOOL
main_interrupted(int type)
{
main_cicount++;
sys_checkinterrupt(type);
if (main_escape_pressed)
  {
  /* LCOV_EXCL_START */
  main_escape_pressed = FALSE;
  signal(SIGINT, sigint_handler);
  if (main_attn || main_oneattn)
    {
    main_oneattn = FALSE;
    error_moan(23);
    return TRUE;
    }
  else
    {
    main_oneattn = TRUE;
    return FALSE;
    }
  /* LCOV_EXCL_STOP */
  }
else return FALSE;
}



/*************************************************
*               Flush interruption               *
*************************************************/

/* Ignore any interrupt and reset the interrupt handler. */

void
main_flush_interrupt(void)
{
if (main_escape_pressed)
  {
  /* LCOV_EXCL_START */
  main_escape_pressed = FALSE;
  signal(SIGINT, sigint_handler);
  /* LCOV_EXCL_STOP */
  }
}



/*************************************************
*            Initialize a new buffer             *
*************************************************/

/* Called at start-up, and also from DBUFFER (to set up an empty buffer if the
only buffer is deleted), NEWBUFFER, and LOAD commands.

Arguments:
  buffer    buffer structure to be initialized
  n         buffer number
  name      file name associated with this buffer
  alias     name to be displayed
  f         file to read to fill the buffer, or NULL for an empty buffer

Returns:     nothing
*/

void
init_buffer(bufferstr *buffer, int n, uschar *name, uschar *alias, FILE *f)
{
/* Anything not explicitly set below is zeroed. */

(void)memset(buffer, 0, sizeof(bufferstr));

buffer->backlist = store_Xget(back_size * sizeof(backstr));
buffer->backlist[0].line = NULL;
buffer->backnext = 0;
buffer->backtop = 0;

buffer->binoffset = 0;
buffer->bufferno = n;
buffer->changed = buffer->saved = FALSE;
buffer->imax = 1;
buffer->imin = 0;
buffer->rmargin = default_rmargin;
buffer->filename = name;
buffer->filealias = alias;
buffer->readonly = main_readonly;

/* If no file is involved, set up a single EOF line. */

if (f == NULL)
  {
  buffer->bottom = buffer->top = store_getlbuff(0);
  buffer->top->flags |= lf_eof;
  buffer->top->key = buffer->linecount = 1;
  }

/* Otherwise, read the file into the buffer and close the file. */

else
  {
  buffer->bottom = buffer->top = file_nextline(f, &buffer->binoffset);
  buffer->top->key = buffer->linecount = 1;

  while ((buffer->bottom->flags & lf_eof) == 0)
    {
    linestr *last = buffer->bottom;
    buffer->bottom = file_nextline(f, &buffer->binoffset);
    buffer->bottom->key = buffer->imax += 1;
    last->next = buffer->bottom;
    buffer->bottom->prev = last;
    buffer->linecount += 1;
    }
  fclose(f);
  }

/* The current line is the top. */

buffer->current = buffer->top;
}



/************************************************
*          Select editing buffer                *
************************************************/

/* Many buffer-related parameters are cached in individual variables while a
buffer is current. On change of buffer, these are put back into the buffer's
struct, and new values set for the new buffer.

Arguments:  buffer to select
Returns:    nothing
*/

void
init_selectbuffer(bufferstr *buffer)
{
if (currentbuffer != NULL)   /* NULL => selecting the very first buffer */
  {
  currentbuffer->backlist = main_backlist;
  currentbuffer->backnext = main_backnext;
  currentbuffer->backtop = main_backtop;
  currentbuffer->bottom = main_bottom;
  currentbuffer->changed = main_filechanged;
  currentbuffer->col = cursor_col;
  currentbuffer->current = main_current;
  currentbuffer->filealias = main_filealias;
  currentbuffer->filename = main_filename;
  currentbuffer->imax = main_imax;
  currentbuffer->imin = main_imin;
  currentbuffer->linecount = main_linecount;
  currentbuffer->markcol = mark_col;
  currentbuffer->markcol = mark_col_global;
  currentbuffer->markline = mark_line;
  currentbuffer->markline_global = mark_line_global;
  currentbuffer->marktype = mark_type;
  currentbuffer->offset = cursor_offset;
  currentbuffer->readonly = main_readonly;
  currentbuffer->rmargin = main_rmargin;
  currentbuffer->row = cursor_row;
  currentbuffer->top = main_top;

  if (main_screenOK) currentbuffer->scrntop = window_vector[0];
  }

/* Now set parameters from saved block */

cursor_row = buffer->row;
cursor_col = buffer->col;
cursor_offset = buffer->offset;

main_backlist = buffer->backlist;
main_backnext = buffer->backnext;
main_backtop = buffer->backtop;
main_bottom = buffer->bottom;
main_current = buffer->current;
main_filealias = buffer->filealias;
main_filechanged = buffer->changed;
main_filename = buffer->filename;
main_imax = buffer->imax;
main_imin = buffer->imin;
main_linecount = buffer->linecount;
main_readonly = buffer->readonly;
main_rmargin = buffer->rmargin;
main_top = buffer->top;

mark_col = buffer->markcol;
mark_col_global = buffer->markcol_global;
mark_line = buffer->markline;
mark_line_global = buffer->markline_global;
mark_type = buffer->marktype;

cursor_max = cursor_offset + window_width;
currentbuffer = buffer;

if (main_screenOK)
  {
  screen_forcecls = TRUE;
  scrn_hint(sh_topline, 0, buffer->scrntop);
  }
}



/*************************************************
*               Start up a file                  *
*************************************************/

/* This function is called when starting up the first window. In screen mode
the input file has already been opened, to test for its existence. An empty
file name is equivalent to no name.

Arguments:
  fid       open input file, or NULL in line-by-line mode
  fromname  name of input file
  toname    name of output file

Returns:    TRUE if all went well
*/

BOOL
init_init(FILE *fid, uschar *fromname, uschar *toname)
{
bufferstr *firstbuffer;
cmdstr cmd;
stringstr str;

if (fid == NULL && fromname != NULL && fromname[0] != 0)
  {
  if (Ustrcmp(fromname, "-") == 0)
    {
    fid = stdin;
    main_interactive = main_verify = FALSE;
    if (cmdin_fid == stdin) cmdin_fid = NULL;
    if (msgs_fid == stdout) msgs_fid = stderr;
    }
  else
    {
    fid = sys_fopen(fromname, US"r");
    if (fid == NULL)  /* Hard error because main_initialized will be FALSE */
      error_moan(5, fromname, "reading", strerror(errno));
    }
  }

/* Now initialize the first buffer and select it as the first buffer. */

firstbuffer = main_bufferchain = store_Xget(sizeof(bufferstr));
init_buffer(main_bufferchain, 0, store_copystring(toname),
  store_copystring(toname), fid);
main_nextbufferno = 1;
init_selectbuffer(main_bufferchain);

main_filechanged = TRUE;
if ((fromname == NULL && toname == NULL) ||
    (fromname != NULL && toname != NULL && Ustrcmp(fromname, toname) == 0 &&
      Ustrcmp(fromname, "-") != 0))
        main_filechanged = FALSE;

cmd_stackptr = 0;
last_se = NULL;
last_gse = last_abese = NULL;
last_gnt = last_abent = NULL;
main_proclist = NULL;
cut_buffer = NULL;
cmd_cbufferline = NULL;
main_undelete = main_lastundelete = NULL;
main_undeletecount = 0;
par_begin = par_end = NULL;
files_written = NULL;

/* There may be additional file names, to be loaded into other buffers. */

for (int i = 0; i < MAX_FROM; i++)
  {
  if (main_fromlist[i] == NULL) break;
  str.text = main_fromlist[i];
  cmd.arg1.string = &str;
  cmd.flags |= cmdf_arg1;
  if (e_newbuffer(&cmd) != done_continue) break;
  }

/* If other files were loaded, the selected buffer will have changed. */

if (main_bufferchain->next != NULL) init_selectbuffer(firstbuffer);
return TRUE;
}



/*************************************************
*          Given help on command syntax          *
*************************************************/

static
void givehelp(void)
{
printf("NE %s %s using PCRE %s\n%s\n\n", version_string, version_date,
  version_pcre, version_copyright);
printf("-b[inary]        run in binary mode\n");
printf("-from <files>    input files, default null, - means stdin, key can be omitted\n");
printf("-[-]h[elp]       output this help\n");
printf("-id              show current version\n");
printf("-line            run in line-by-line mode\n");
printf("-noinit or -norc don\'t obey .nerc file\n");
printf("-notabs          no special tab treatment\n");
printf("-notraps         don't catch signals (debugging option)\n");
printf("-opt <string>    initial line of commands\n");
printf("-r[eadonly]      start in readonly state\n");
printf("-tabin           expand input tabs; no tabs on output\n");
printf("-tabout          use tabs in all output lines\n");
printf("-tabs            expand input tabs; retab those lines on output\n");
printf("-[t]o <file>     output file for 1st input, default = from\n");
printf("-ver <file>      verification file, default is screen\n");
printf("-[-]v[ersion]    show current version\n");
printf("-w[idechars]     recognize UTF-8 characters in files\n");
printf("-with <file>     command file, default is terminal\n");
printf("-withkeys <file> fake keystrokes - testing feature\n");
printf("-wks <n>         pause value (seconds) for -withkeys\n");

printf("\nThe tabbing default is -tabs unless overridden by the NETABS "
       "environment\nvariable.\n\n");

printf("          EXAMPLES\n");
printf("ne myfile -notabs\n");
printf("ne myfile -with commands -to outfile\n");
printf("ne -line file1 file2 -notabs\n");
}



/*************************************************
*             Decode command line                *
*************************************************/

/* This function uses the "rdargs" command line decoding function.

Arguments:
  argc             value from main()
  artv             value from main()
  arg_from_buffer  where to put first "from" name, if present
  arg_to_buffer    where to put "to" name, if present

Returns:           nothing
*/

static void
decode_command(int argc, char **argv, uschar *arg_from_buffer,
  uschar *arg_to_buffer)
{
arg_result results[100];

/* Indices for the various keys in the results vector. Keep in step with the
key string below. */

enum { arg_from,     arg_to=MAX_FROM, arg_id,        arg_help,   arg_line,
       arg_with,     arg_ver,         arg_opt,       arg_noinit, arg_tabs,
       arg_tabin,    arg_tabout,      arg_notabs,    arg_binary, arg_notraps,
       arg_readonly, arg_widechars,   arg_withkeys,  arg_wks,    arg_end };

/* Macro magic to get the MAX_FROM value inserted as part of the key list
string. */

#define STR(s) #s
#define XSTR(s) STR(s)
uschar *argstring = US
  "from/"
  XSTR(MAX_FROM)
  ",to=o/k,id=-version=version=v/s,help=-help=h/s,line/s,with/k,ver/k,"
  "opt/k,noinit=norc/s,tabs/s,tabin/s,tabout/s,notabs/s,binary=b/s,"
  "notraps/s,readonly=r/s,widechars=w/s,withkeys/k,wks/k/n";
#undef STR
#undef XSTR

/* Decode - the error is hard because main_initialized is still FALSE. */

if (rdargs(argc, argv, argstring, results) != 0)
  {
  main_screenmode = main_screenOK = FALSE;
  error_moan(0, results[0].data.text, results[1].data.text);
  }

/* Deal with "id" and "help" */

if (results[arg_id].data.number != 0)
  {
  printf("NE %s %s using PCRE %s\n", version_string, version_date,
    version_pcre);
  exit(EXIT_SUCCESS);
  }

if (results[arg_help].data.number != 0)
  {
  givehelp();
  exit(EXIT_SUCCESS);
  }

/* Flag to skip obeying initialization string if requested */

if (results[arg_noinit].data.number != 0) main_noinit = TRUE;

/* Handle tabbing options; the defaults are set from a system variable, but if
any options keywords are given, they define the entire state relative to
everything being FALSE. */

if (results[arg_tabs].data.number != 0)
  {
  main_tabin = main_tabflag = TRUE;
  main_tabout = FALSE;
  }

if (results[arg_tabin].data.number != 0)
  {
  main_tabin = TRUE;
  main_tabflag = FALSE;
  main_tabout = (results[arg_tabout].data.number != 0);
  }

else if (results[arg_tabout].data.number != 0)
  {
  main_tabin = FALSE;
  main_tabout = TRUE;
  }

if (results[arg_notabs].data.number != 0)
  main_tabin = main_tabout = FALSE;

/* Force line-by-line mode if requested */

if (results[arg_line].data.number != 0)
  main_screenmode = main_screenOK = FALSE;

/* Binary options */

if (results[arg_binary].data.number != 0)
  main_binary = main_overstrike = TRUE;

/* Readonly option */

if (results[arg_readonly].data.number != 0) main_readonly = TRUE;

/* Widechars option */

if (results[arg_widechars].data.number != 0) allow_wide = TRUE;

/* Notraps option */

if (results[arg_notraps].data.number != 0) no_signal_traps = TRUE;

/* Deal with an initial opt command line */

main_opt = results[arg_opt].data.text;

/* Set up a command file - this implies line-by-line mode */

if (results[arg_with].data.text != NULL)
  {
  main_screenmode = main_screenOK = main_interactive = FALSE;
  arg_with_name = store_copystring(results[arg_with].data.text);
  }

/* Set up a verification output file - this implies line-by-line mode */

if (results[arg_ver].data.text != NULL)
  {
  arg_ver_name = store_copystring(results[arg_ver].data.text);
  main_screenmode = main_screenOK = main_interactive = FALSE;
  if (Ustrcmp(arg_ver_name, "-") != 0)
    {
    msgs_fid = sys_fopen(arg_ver_name, US"w");
    if (msgs_fid == NULL)
      {
      msgs_fid = stderr;
      error_moan(5, arg_ver_name, "writing", strerror(errno));  /* Hard because not initialized */
      }
    }
  }

/* Deal with "from" & "to" */

if (results[arg_from].data.text != NULL)
  {
  arg_from_name = arg_from_buffer;
  Ustrcpy(arg_from_name, results[arg_from].data.text);
  if (Ustrcmp(arg_from_name, "-") == 0 &&
    (arg_with_name != NULL && Ustrcmp(arg_with_name, "-") == 0))
      {
      main_screenmode = FALSE;
      error_moan(60, "-from or -with", "input");    /* Hard */
      }
  }

/* Now deal with additional arguments to "from". There may be up to
MAX_FROM in total. */

for (int i = 1; i < MAX_FROM; i++) main_fromlist[i-1] = NULL;
for (int i = 1; i < MAX_FROM; i++)
  {
  if (results[arg_from + i].data.text == NULL) break;
  main_fromlist[i-1] = results[arg_from+i].data.text;
  }

/* Now "to" */

if (results[arg_to].data.text != NULL)
  {
  arg_to_name = arg_to_buffer;
  Ustrcpy(arg_to_name, results[arg_to].data.text);
  if (Ustrcmp(arg_to_name, "-") == 0)
    {
    if (arg_ver_name == NULL) msgs_fid = stderr;
    else if (Ustrcmp(arg_ver_name, "-") == 0)
      {
      main_screenmode = FALSE;
      error_moan(60, "-to or -ver", "output");    /* Hard */
      }
    }
  }

/* Set up a file that contains data for creating simulated keystrokes for
automatic testing of screen mode. */

if (results[arg_withkeys].data.text != NULL)
  {
  if (!main_screenmode) error_moan(72);  /* Hard */
  withkey_fid = sys_fopen(results[arg_withkeys].data.text, US"r");
  if (withkey_fid == NULL)
    {
    /* LCOV_EXCL_START */
    main_screenmode = FALSE;  /* Hard error */
    error_moan(5, results[arg_withkeys].data.text, "reading", strerror(errno));
    /* LCOV_EXCL_STOP */
    }
  kbd_fid = withkey_fid;
  if (results[arg_wks].presence != arg_present_not)
    withkey_sleep = results[arg_wks].data.number;    /* LCOV_EXCL_LINE */
  }
}



/*************************************************
*             Initialize keystrings              *
*************************************************/

static void
keystrings_init(void)
{
for (int i = 0; i <= max_keystring; i++) main_keystrings[i] = NULL;
key_setfkey(1, US"buffer");
key_setfkey(3, US"w");
key_setfkey(4, US"undelete");
key_setfkey(6, US"pll");
key_setfkey(7, US"f");
key_setfkey(8, US"m*");
key_setfkey(9, US"show keys");
key_setfkey(10, US"rmargin");
key_setfkey(11, US"pbuffer");
key_setfkey(16, US"plr");
key_setfkey(17, US"bf");
key_setfkey(18, US"m0");
key_setfkey(19, US"show fkeys");
key_setfkey(20, US"format");
key_setfkey(30, US"unformat");
key_setfkey(57, US"front");
key_setfkey(58, US"topline");
key_setfkey(59, US"back");
key_setfkey(60, US"overstrike");
}



/*************************************************
*            Initialize tables                   *
*************************************************/

/* The character table is initialized dynamically, just in case we ever want
to port this to a non-ASCII system. It has to be writable, because the user
can change the definition of what constitutes a "word". */

static void
tables_init(void)
{
uschar *ucletters = US"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
uschar *lcletters = US"abcdefghijklmnopqrstuvwxyz";
uschar *digits    = US"0123456789";
uschar *hexdigits = US"0123456789ABCDEFabcdef";
uschar *delims    = US",.:\'\"!+-*/";

for (int i = 0; i < 26; i++)
  {
  ch_tab[lcletters[i]] = ch_lcletter + ch_word;
  ch_tab[ucletters[i]] = ch_ucletter + ch_word;
  }
for (int i = 0; i < 10; i++) ch_tab[digits[i]] = ch_digit + ch_word;
for (int i = 0; i < 22; i++) ch_tab[hexdigits[i]] |= ch_hexch;
for (int i = 0; i < (int)Ustrlen((const uschar *)delims); i++)
  ch_tab[delims[i]] |= (ch_delim + ch_filedelim);

for (int i = 0; i < (int)Ustrlen(cmd_qualletters); i++)
  ch_tab[cmd_qualletters[i]] =
  ch_tab[cmd_qualletters[i]] | ch_qualletter;

/* Table to translate a single letter key name into a "control code". This
table is used only in implementing the KEY command in a way that is independent
the machine's character set */

for (int i = 0; i < 26; i++)
  {
  key_codes[ucletters[i]] = i+1;
  key_codes[lcletters[i]] = i+1;
  }
key_codes['\\']  = 28;
key_codes[']']   = 29;
key_codes['^']   = 30;
key_codes['_']   = 31;
}



/*************************************************
*            Obey initialization file            *
*************************************************/

/* Function to obey an initialization file. The existence of the file has been
tested by system-specific code. Any errors that arise will be treated as
disasters.

Argument:    the file name
Returns:     nothing (errors cause bomb out)
*/

void
obey_init(uschar *filename)
{
FILE *f = sys_fopen(filename, US"r");
if (f == NULL) error_moan(5, filename, "reading", strerror(errno));
while (Ufgets(cmd_buffer, CMD_BUFFER_SIZE, f) != NULL) cmd_obey(cmd_buffer);
fclose(f);
}



/*************************************************
*            Run in line-by-line mode            *
*************************************************/

static void
main_runlinebyline(void)
{
uschar *fromname = arg_from_name;
uschar *toname = (arg_to_name == NULL)? arg_from_name : arg_to_name;

if (main_interactive)
  {
  /* LCOV_EXCL_START */
  printf("NE %s %s using PCRE %s\n", version_string, version_date,
    version_pcre);
  main_verify = main_shownlogo = TRUE;
  /* LCOV_EXCL_STOP */
  }
else
  {
  main_verify = FALSE;
  if (arg_with_name != NULL && Ustrcmp(arg_with_name, "-") != 0)
    {
    cmdin_fid = sys_fopen(arg_with_name, US"r");
    if (cmdin_fid == NULL)
      error_moan(5, arg_with_name, "reading", strerror(errno));  /* hard because not initialized */
    }
  }

if (init_init(NULL, fromname, toname))
  {
  if (!main_noinit && main_einit != NULL) obey_init(main_einit);

  main_initialized = TRUE;
  if (main_opt != NULL) cmd_obey(main_opt);

  while (!main_done)
    {
    main_cicount = 0;
    (void)main_interrupted(ci_read);
    if (main_verify) line_verify(main_current, TRUE, TRUE);
    signal(SIGINT, fgets_sigint_handler);
    if (main_interactive) main_rc = error_count = 0;
    if (setjmp(rdline_env) == 0)
      {
      int n;
      if (cmdin_fid == NULL ||
          Ufgets(cmd_buffer, CMD_BUFFER_SIZE, cmdin_fid) == NULL)
        {
        Ustrcpy(cmd_buffer, "w\n");
        }

      main_flush_interrupt();           /* resets handler */
      n = Ustrlen(cmd_buffer);
      if (n > 0 && cmd_buffer[n-1] == '\n') cmd_buffer[n-1] = 0;
      (void)cmd_obey(cmd_buffer);
      }
    else
      {
      /* LCOV_EXCL_START */
      if (!main_interactive) break;
      clearerr(cmdin_fid);
      /* LCOV_EXCL_STOP */
      }
    }

  if (cmdin_fid != NULL && cmdin_fid != stdin) fclose(cmdin_fid);
  }
}



/*************************************************
*          Set up default tab options            *
*************************************************/

/* All 3 Boolean flags are FALSE on entry; main_tabs is set to "tabs" by
default, but may have been overridden by the NETABS environment variable. */

static void
tab_init(void)
{
if (Ustrcmp(main_tabs, "notabs") == 0) return;
if (Ustrcmp(main_tabs, "tabs") == 0) main_tabin = main_tabflag = TRUE;
else if (Ustrcmp(main_tabs, "tabin") == 0) main_tabin = TRUE;
else if (Ustrcmp(main_tabs, "tabout") == 0) main_tabout = TRUE;
else if (Ustrcmp(main_tabs, "tabinout") == 0) main_tabin = main_tabout = TRUE;
else error_moan(71, main_tabs);   /* Hard error because still initializing */
}



/*************************************************
*           Exit tidy-up function                *
*************************************************/

/* Automatically called for any exit. Free the extensible buffers and other
memory. */

static void
tidy_up(void)
{
if (debug_file != NULL) fclose(debug_file);
if (crash_logfile != NULL) fclose(crash_logfile);

/* The PCRE2 free() functions do nothing if the argument is NULL (not
initialised) so there's no need to check. */

#ifndef USE_PCRE1
pcre2_general_context_free(re_general_context);
pcre2_compile_context_free(re_compile_context);
pcre2_match_data_free(re_match_data);
#endif

sys_tidy_up();
store_free_all();
}



/*************************************************
*                Entry Point                     *
*************************************************/

int
main(int argc, char **argv)
{
uschar cbuffer[CMD_BUFFER_SIZE];
uschar fbuffer1[FNAME_BUFFER_SIZE];
uschar fbuffer2[FNAME_BUFFER_SIZE];
uschar fbuffer3[FNAME_BUFFER_SIZE];

cmd_buffer = cbuffer;                /* Make globally available */
kbd_fid = cmdin_fid = stdin;         /* Default */
msgs_fid = stdout;                   /* Default */
setvbuf(msgs_fid, NULL, _IONBF, 0);  /* Set unbuffered */

if (atexit(tidy_up) != 0) error_moan(68);  /* Hard */

store_init();         /* Initialize memory handler */
tables_init();        /* Initialize tables */
keystrings_init();    /* Set up default variable keystrings */

sys_init1();          /* Early local initialization */
version_init();       /* Set up for messages */
tab_init();           /* Set default tab options */

decode_command(argc, argv, fbuffer1, fbuffer2);  /* Decode command line */
if (main_binary && allow_wide) error_moan(64);   /* Hard */
sys_init2(fbuffer3);  /* Final local initialization */

/* If input is stdin or output is stdout, force line mode and non-interactive. */

if ((arg_from_name != NULL && Ustrcmp(arg_from_name, "-") == 0) ||
    (arg_to_name != NULL && Ustrcmp(arg_to_name, "-") == 0))
      main_interactive = main_screenmode = main_screenOK = FALSE;

/* Note if messages are to a terminal */

msgs_tty = isatty(fileno(msgs_fid));

/* Set a handler to trap keyboard interrupts */

signal(SIGINT, sigint_handler);

/* Set a handler for all other relevant signals to dump buffers and exit from
NE. The list of signals is system-dependent. Trapping these interrupts can be
cut out when debugging so the original cause of a fault can more easily be
found. */

if (!no_signal_traps)
  {
  for (int sigptr = 0; signal_list[sigptr] > 0; sigptr++)
    signal(signal_list[sigptr], crash_handler);
  }

/* The variables main_screenmode and main_interactive indicate the style of
editing run. */

if (main_screenmode)
  {
  cmdin_fid = NULL;
  sys_runscreen();
  }
else main_runlinebyline();

/* A final newline is needed in some circumstances. */

if (main_screenOK && main_nlexit && main_pendnl) sys_mprintf(msgs_fid, "\r\n");
return sys_rc(main_rc);
}

/* End of einit.c */
