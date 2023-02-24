/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */


/* This file contains code for handling errors */

#include "ehdr.h"
#include "shdr.h"

#define rc_noerror   0
#define rc_warning   4
#define rc_serious   8
#define rc_failed   12
#define rc_disaster 16


/* Buffer for the use of error_printf */

static uschar printf_buff[256];
static usint printf_buffptr = 0;



/*************************************************
*            Texts and return codes              *
*************************************************/

typedef struct {
  uschar rc;
  CBOOL  showcmd;
  uschar *text;
} error_struct;

static error_struct error_data[] = {

/* 0-4 */
{ rc_disaster, FALSE, US"Failed to decode command line: \"%s\" %s\n" },
{ rc_disaster, FALSE, US"Ran out of memory: %lu bytes unavailable\n" },
{ rc_disaster, FALSE, US"Internal failure - store overlap upwards (%x %d %x)\n" },
{ rc_disaster, FALSE, US"Internal failure - store overlap downwards (%x %d %x)\n" },
{ rc_disaster, FALSE, US"Internal failure - %s in %s (%d, %d, %d, %d, %d)\n" },
/* 5-9 */
{ rc_serious,  FALSE, US"Failed to open file \"%s\" for %s: %s\n" },
{ rc_disaster, FALSE, US"Internal failure - current line pointer is NULL\n" },
{ rc_serious,  TRUE,  US"Unmatched closing bracket\n" },
{ rc_serious,  TRUE,  US"Semicolon expected (characters after command end)\n" },
{ rc_serious,  TRUE,  US"Unexpected ELSE (prematurely terminated IF or UNLESS?)\n" },
/* 10-14 */
{ rc_serious,  TRUE,  US"Unknown command \"%s\"\n" },
{ rc_serious,  FALSE, US"Unexpected response (\"to\" is required before a file name)\n" },
{ rc_serious,  FALSE, US"Illegal filename \"%s\" (%s)\n" },
{ rc_serious,  TRUE,  US"%s expected\n" },
{ rc_serious,  TRUE,  US"Error in key definition string (at character %ld):\n%s\n" },
/* 15-19 */
{ rc_serious,  FALSE, US"%s not allowed %s\n" },
{ rc_serious,  FALSE, US"No previous %s\n" },
{ rc_serious,  FALSE, US"%s not found\n" },
{ rc_serious,  TRUE,  US"Error in hexadecimal string: %s\n" },
{ rc_serious,  TRUE,  US"Error in hexadecimal string at character %d: %s\n" },
/* 20-24 */
{ rc_serious,  TRUE,  US"Repeated or incompatible qualifier\n" },
{ rc_serious,  TRUE,  US"Only %s qualifiers allowed on insertion strings for this command\n" },
{ rc_serious,  TRUE,  US"n, s, u and w are the only qualifiers allowed with a search expression\n" },
{ rc_serious,  FALSE, US"Keyboard interrupt\n" },
{ rc_warning,  FALSE, US"The contents of buffer %d have not been saved\n" },
/* 25-29 */
{ rc_serious,  TRUE,  US"Line %d not found\n" },
{ rc_serious,  TRUE,  US"Buffer %d does not exist\n" },
{ rc_serious,  TRUE,  US"The B, E, or P qualifier is required for an empty search string in a global command\n" },
{ rc_warning,  FALSE, US"The contents of the cut buffer have not been pasted.\n" },
{ rc_serious,  FALSE, US"Unexpected %s in %s command\n" },
/* 30-34 */
{ rc_serious,  FALSE, US"Unexpected %s while obeying \"%s\" command\n" },
{ rc_serious,  FALSE, US"Procedure calls too deeply nested\n" },
{ rc_serious,  FALSE, US"Unexpected end of file while reading NE commands\n" },
{ rc_serious,  TRUE,  US"Missing second argument for \"%s\" command\n" },
{ rc_serious,  TRUE,  US"Incorrect value for %s (%s)\n" },
/* 35-39 */
{ rc_serious,  TRUE,  US"Function key number not in range 1-%d\n" },
{ rc_disaster, FALSE, US"Sorry! NE has crashed on receiving signal %d %s\n" },
{ rc_serious,  FALSE, US"I/O error while writing file \"%s\": %s\n" },
{ rc_serious,  TRUE,  US"Error in regular expression (at character %d):\n   %s\n" },
{ rc_serious,  FALSE, US"Cannot concatenate line that has the line mark\n" },
/* 40-44 */
{ rc_serious,  FALSE, US"Cursor must be at line start for whole-line change\n" },
{ rc_serious,  FALSE, US"No appropriate mark set for %s command\n" },
{ rc_serious,  FALSE, US"Sorry, no help%s%s is available\n"
                      "(Use \"show keys\" for keystroke information)\n" },
{ rc_serious,  FALSE, US"Cannot set %s mark because %s mark is already set\n" },
{ rc_serious,  TRUE,  US"Error in argument for \"word\" (at character %d): %s\n" },
/* 45-49 */
{ rc_serious,  TRUE,  US"Procedure %s already exists\n" },
{ rc_serious,  TRUE,  US"Malformed procedure name (must be \'.\' followed by letters or digits\n" },
{ rc_serious,  FALSE, US"Attempt to cancel active procedure %s\n" },
{ rc_serious,  TRUE,  US"Procedure %s not found\n" },
{ rc_serious,  FALSE, US"\"%s\" cannot be opened because %s\n" },
/* 50-54 */
{ rc_serious,  FALSE, US"Commands are being read from buffer %d, so it cannot be %s\n" },
{ rc_serious,  FALSE, US"Buffer %d already exists\n" },
{ rc_serious,  FALSE, US"The \"%s\" command is not allowed in a read-only buffer\n" },
{ rc_serious,  FALSE, US"The current buffer is read-only\n" },
{ rc_serious,  FALSE, US"Character U+%04x is not displayable\n" },
/* 55-59 */
{ rc_noerror,  FALSE, US"Cut buffer is empty\n" },  /* Not even a warning */
{ rc_serious,  FALSE, US"Command line in buffer is too long\n" },
{ rc_serious,  FALSE, US"DBUFFER interrupted - lines have been deleted\n" },
{ rc_serious,  FALSE, US"Binary file contains \"%c\" where a hex digit is expected\n" },
{ rc_serious,  FALSE, US"Output file not specified for buffer %d - not written\n" },
/* 60-64 */
{ rc_serious,  FALSE, US"Only one of %s may be the standard %s\n" },
{ rc_serious,  TRUE,  US"Commands cannot be read from binary buffers\n" },
{ rc_serious,  FALSE, US"Internal failure - 'back' line not found\n" },
{ rc_serious,  TRUE,  US"Error in regular expression at offset %d:\n   %s\n" },
{ rc_serious,  FALSE, US"-binary and -widechars are mutually exclusive\n" },
/* 65-69 */
#ifdef USE_PCRE1
{ rc_serious,  TRUE,  US"Error %d while matching regular expression%s\n" },
#else
{ rc_serious,  TRUE,  US"Error while matching regular expression:\n   %s\n" },
#endif
{ rc_serious,  FALSE, US"A line longer than %d bytes has been split\n" },
{ rc_warning,  FALSE, US"Ignored \"key\" command in line mode\n" },
{ rc_disaster, FALSE, US"Call to atexit() failed\n" },
{ rc_serious,  TRUE,  US"Buffer %d is the current data buffer, so it cannot be a command buffer\n" },
/* 70-74 */
{ rc_serious,  FALSE, US"Internal failure: buffer %d not found in buffer list\n" },
{ rc_serious,  FALSE, US"Environment variable NETABS value \"%s\" is invalid\n" },
{ rc_disaster, FALSE, US"\"-withkeys\" is not valid in line-by-line mode\n" },
{ rc_serious,  FALSE, US"\"%s\" in -withkeys file is not a known keyname - NE abandoned\n" },
{ rc_disaster, FALSE, US"Internal failure: -withkeys name \"%s\": Pkey code %d not found - NE abandoned\n" }
};

#define error_maxerror (int)(sizeof(error_data)/sizeof(error_struct))


/*************************************************
*            Display error message               *
*************************************************/

/* These functions are global because they are called throughout the code in
order to write to the error stream. Text from successive calls is buffered up
until a newline or a call to error_printflush, because that gave greater
efficiency in windowing systems (well, on the Archimedes, anyway), but of
course isn't really relevant any more. */

void
error_printflush(void)
{
uschar *CR, *p;

if (printf_buffptr == 0) return;
CR = (msgs_fid == stdout || msgs_fid == stderr)? US"\r": US"";
if (main_logging) debug_writelog("%s", printf_buff);

p = printf_buff;
if (main_screenOK)
  {
  screen_forcecls = TRUE;
  if (s_window() != message_window)
    {
    s_selwindow(message_window, 0, 0);
    s_cls();
    s_flush();
    }
  }

if (main_pendnl)
  {
  sys_mprintf(msgs_fid, "%s\n", CR);
  main_pendnl = main_nowait = FALSE;
  }

while (*p != 0)
  {
  int k;
  uschar *pp = p;
  if (*p == '\n') sys_mprintf(msgs_fid, "%s", CR);
  GETCHARINC(k, p, cmd_buffer + printf_buffptr);
  if (!msgs_tty || main_utf8terminal)
    sys_mprintf(msgs_fid, "%.*s", (int)(p-pp), pp);
  else
    {
    /* LCOV_EXCL_START - never hit in non-interactive tests */
    if (k > 255) k = '?';
    sys_mprintf(msgs_fid, "%c", k);
    /* LCOV_EXCL_STOP */
    }
  }

printf_buffptr = 0;
}


void
error_printf(const char *format, ...)
{
va_list ap;
va_start(ap, format);

/* If the last thing that was output was a line verification with a non-null
pointer, we need to output a newline. */

if (main_verified_ptr)
  {
  main_verified_ptr = FALSE;
  error_printf("\n");
  }

vsprintf(CS printf_buff + printf_buffptr, format, ap);
printf_buffptr = Ustrlen(printf_buff);
if (printf_buffptr > 200 || printf_buff[printf_buffptr-1] == '\n')
  error_printflush();
va_end(ap);
}



/*************************************************
*            Generate error message              *
*************************************************/

/* This internal function is called from several interface functions below with
differing requirements. */

static void
error_moan_internal(int n, va_list ap)
{
int rc, orig_rc;
uschar buff[256];

/* Show logo if not shown. In a screen operation, it will have been shown at
the start. */

if (!main_shownlogo)
  {
  error_printf("NE %s %s using PCRE %s\n", version_string, version_date,
    version_pcre);
  main_shownlogo = TRUE;
  }

/* Set up the error text */

if (n > error_maxerror)
  {
  /* LCOV_EXCL_START - should never occur */
  sprintf(CS buff, "** Unknown error number %d", n);
  orig_rc = rc = rc_disaster;
  /* LCOV_EXCL_STOP */
  }
else
  {
  Ustrcpy(buff, "** ");
  vsprintf(CS buff + 3, CS (error_data[n].text), ap);
  orig_rc = rc = error_data[n].rc;
  }
va_end(ap);

/* If not yet fully initialized, all errors are disasters */

if (!main_initialized) rc = rc_disaster;

/* If too many errors, replace the text and force a high return code. Reset the
error count so that we don't get into a loop if there's a subsequent error. */

if (rc > rc_warning && ++error_count > MAX_ERRORS)
  {
  /* LCOV_EXCL_START - won't occur in standard tests */
  if (rc < rc_failed) rc = rc_failed;
  Ustrcpy(buff, "** Too many errors\n");
  error_count = 0;
  /* LCOV_EXCL_STOP */
  }

/* Set the return code for the run */

if (rc > main_rc) main_rc = rc;

/* If in a non-windowing screen run, position to the bottom of the screen for
disastrous errors - this has to be a system-specific thing as we may be in the
middle of screen handling when the error occurs. */

if (rc >= rc_failed && main_screenmode) sys_crashposition();

/* If we have a disastrous error, turn on logging to the crash log file. This
will cause all subsequent output via error_printf to be copied to this file.
Don't do this for error 0, however (failure to decode command line), or normal
errors when not initialized (i.e. errors in the init line). */

if (n != 0 && (orig_rc > rc_serious || main_initialized) && rc >= rc_failed)
  main_logging = TRUE;   /* LCOV_EXCL_LINE */

/* Show the command line if relevant */

if (error_data[n].showcmd &&
    (!main_initialized || !main_interactive || main_screenmode))
  {
  int i = line_charcount(cmd_cmdline, cmd_ptr - cmd_cmdline);
  error_printf("%s", cmd_cmdline);
  if (cmd_cmdline[Ustrlen(cmd_cmdline) - 1] != '\n') error_printf("\n");
  if (i > 0)
    {
    while (i--) error_printf(" ");
    error_printf(">\n");
    }
  }

/* Now output the error message */

error_printf("%s", buff);

/* NE exits on a disaster. */

if (rc >= rc_failed)
  {
  /* LCOV_EXCL_START - no disasters in standard tests */
  if (main_logging)
    error_printf("** Error information is being written to the crash log\n");
  crash_handler(-rc);   /* -n is a pseudo signal value */
  /* LCOV_EXCL_STOP */
  }
}


/* Give error, nothing fancy. */

void
error_moan(int n, ...)
{
va_list ap;
va_start(ap, n);
error_moan_internal(n, ap);
}


/* Give error, set cmd_faildecode . */

void
error_moan_decode(int n, ...)
{
va_list ap;
va_start(ap, n);
error_moan_internal(n, ap);
cmd_faildecode = TRUE;
}



/*************************************************
*               QS/SE formatter                  *
*************************************************/

/* Sub-function to format flags */

static int flagbits[] = {
  0x0003, 0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020,
  0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0};

static uschar *
flagchars = US "pbehlnrsuvwx";

static uschar *
format_qseflags(uschar *b, int flags)
{
int i;
for (i = 0; flagbits[i]; i++)
  {
  if ((flags & flagbits[i]) == flagbits[i])
    {
    *b++ = flagchars[i];
    flags &= ~flagbits[i];
    }
  }
return b;
}

/* Function for formatting a qualified string or search expression. */

static uschar *
format_qse(uschar *b, sestr *se)
{
if (se->type == cb_setype)
  {
  b = format_qseflags(b, se->flags);
  *b++ = '(';
  b = format_qse(b, se->left.se);
  if (se->right.se != NULL)
    {
    sprintf(CS b, " %s ", ((se->flags & qsef_AND) != 0)? "&" : "|");
    b += 3;
    b = format_qse(b, se->right.se);
    }
  *b++ = ')';
  }

else
  {
  qsstr *qs = (qsstr *)se;
  if (qs->count != 1)
    {
    b += sprintf(CS b, "%d", qs->count);
    }
  if (qs->windowleft != qse_defaultwindowleft || qs->windowright != qse_defaultwindowright)
    {
    b += sprintf(CS b, "[%d,%d]", qs->windowleft+1, qs->windowright);
    }
  b = format_qseflags(b, qs->flags);
  Ustrncpy(b, qs->text, qs->length + 1);   /* extra 1 to include delimiter */
  b += qs->length + 1;
  *b++ = qs->text[0];
 }

*b = 0;
return b;
}



/*************************************************
*       Wrapper for unpicking se/qs args         *
*************************************************/

/* This function is used in a few cases when a qualified string or search
expression needs to be expanded for inclusion in an error message. So far
there is no requirement for other arguments. */

void
error_moanqse(int n, sestr *p)
{
uschar sebuff[256];
(void)format_qse(sebuff, p);
error_moan(n, sebuff);
}

/* End of eerror.c */
