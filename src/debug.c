/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2022 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: December 2022 */


/* This file contains debugging code, so exclude it from coverage testing. */

#include "ehdr.h"

/* LCOV_EXCL_START */


/*************************************************
*           Display debugging output             *
*************************************************/

/* Made global so it can be called from anywhere. In screen mode it outputs to
a file called NEdebug; otherwise it writes to stdout. */

void
debug_printf(const char *format, ...)
{
va_list ap;
va_start(ap, format);
if (main_screenmode)
  {
  if (debug_file == NULL)
    {
    debug_file = fopen("NEdebug", "w");
    if (debug_file == NULL)
      {
      printf("\n**** Can't open debug file NEdebug (%s) - aborting ****\n\n",
        strerror(errno));
      exit(99);
      }
    }
  vfprintf(debug_file, format, ap);
  fflush(debug_file);
  }
else
  {
  vprintf(format, ap);
  fflush(stdout);
  }
va_end(ap);
}


/*************************************************
*        Give information about the screen       *
*************************************************/

void
debug_screen(void)
{
debug_printf("main_linecount = %2d\n", main_linecount);
debug_printf("cursor_offset  = %2d\n", cursor_offset);
debug_printf("cursor_row     = %2d cursor_col   = %2d\n", cursor_row,
  cursor_col);
debug_printf("window_width   = %2d window_depth = %2d\n", window_width,
  window_depth);
debug_printf("-------------------------------------\n");
}


/*************************************************
*            Write to a crash log file           *
*************************************************/

void
debug_writelog(const char *format, ...)
{
va_list ap;
va_start(ap, format);

if (crash_logfile == NULL)
  {
  uschar *name = sys_crashfilename(FALSE);
  if (name == NULL) return;
  crash_logfile = Ufopen(name, "w");
  if (crash_logfile == NULL)
    {
    main_logging = FALSE;    /* Prevent recursion */
    error_printf("Failed to open crash log file %s: %s\n", name,
      strerror(errno));
    return;
    }
  }

vfprintf(crash_logfile, format, ap);
fflush(crash_logfile);
va_end(ap);
}

/* LCOV_EXCL_STOP */

/* End of debug.c */
