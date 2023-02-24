/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */


/* This file contains code for handling input and output */

#include "ehdr.h"

#define BUFFGETSIZE 1024



/*************************************************
*         Support functions for backups          *
*************************************************/

/* Support for systems where file names are caseless is a historical hangover,
but leave it around in case... */

BOOL
file_written(uschar *name)
{
filewritstr *p = files_written;
while (p != NULL)
  {
  #ifdef FILE_CASELESS
  uschar *s = name;
  uschar *t = p->name;
  while ((*t || *s) && tolower(*t) == tolower(*s)) { t++; s++; }
  if (*t == 0 && *s == 0) return TRUE;
  #else
  if (Ustrcmp(name, p->name) == 0) return TRUE;
  #endif
  p = p->next;
  }
return FALSE;
}

void
file_setwritten(uschar *name)
{
filewritstr *p;
if (file_written(name)) return;
p = store_Xget(sizeof(filewritstr));
p->name = store_copystring(name);
p->next = files_written;
files_written = p;
}



/*************************************************
*      Get next input line and binarize it       *
*************************************************/

/* The next sixteen bytes are read from the file, and a line is constructed
that has the file offset first, followed by the hexadecimal representations of
the 16 bytes, followed by the actual one-byte characters if they are
displayable. For example:

0002b0  0a 0a 42 4f 4f 4c 20 0a  66 69 6c 65 5f 77 72 69  * ..BOOL .file_wri *

Arguments:
  f           the file to read from
  binoffset   pointer to the file offset value; this gets updated

Returns:      a line structure (if EOF, it's set as an EOF line)
*/

static linestr *
file_nextbinline(FILE *f, size_t *binoffset)
{
linestr *line = store_getlbuff(80);
uschar *s = line->text;
uschar *p = s + 8;
uschar cc[17];

sprintf(CS s, "%06zx  ", *binoffset);
*binoffset += 16;

for (int i = 0; i < 16; i++)
  {
  int c = fgetc(f);
  if (c == EOF)
    {
    if (i == 0)
      {
      line->len = 0;
      line->flags |= lf_eof;
      return line;
      }
    sprintf(CS p, "   ");
    }
  else sprintf(CS p, "%02x ", c);
  p += 3;
  if (i == 7) *p++ = ' ';
  cc[i] = isprint(c)? c : '.';
  }

cc[16] = 0;
sprintf(CS p, " * %s *", cc);
line->len = (p - s + 21);

return line;
}



/*************************************************
*            Get next input line                 *
*************************************************/

/* Returns an eof line at end of file. In binary mode, if binoffset is not
NULL, the input is read and converted to hex by calling the above function. In
non-binary mode, or if binoffset is NULL (indicating reading from a command
file) the next line is read as is. In this case we get a line buffer of
large(ish) size, and free off the end of it if possible. When a line is very
long, we have to copy it into a longer buffer.

Arguments:
  f           the file to read from
  binoffset   pointer to the file offset value for use when operating in
                binary; the value is updated

Returns:      a line structure (if EOF, it's set as an EOF line)
*/

linestr *
file_nextline(FILE *f, size_t *binoffset)
{
BOOL eof, tabbed;
size_t length, maxlength;
linestr *line;
uschar *s;

/* In binary mode, binoffset is passed as NULL when reading a command line or
other non-data line that should not be binarized. */

if (main_binary && binoffset != NULL) return file_nextbinline(f, binoffset);

/* Not a binary read */

eof = tabbed = FALSE;
length = 0;
maxlength = BUFFGETSIZE;
line = store_getlbuff(BUFFGETSIZE);
s = line->text;

for (;;)
  {
  uschar *newtext;
  int need;
  int c = fgetc(f);

  if (c == '\n') break;
  if (c == EOF)
    {
    if (length == 0 || ferror(f)) eof = TRUE;
    break;
    }

  need = (c == '\t' && main_tabin)? 8 - length % 8 : 1;  /* Space needed */

  /* If the current buffer is full, extend the line up to MAX_LINELENGTH. If we
  hit the absolute maximum, end the line, which has the effect of splitting it
  into two. As well as at other times, this function is called during
  initialization, when loading a file named on the command line. At that point,
  main_initialized is FALSE. We temporarily make it TRUE so that error_moan()
  does not make this a hard error. */

  if (length > maxlength - need)
    {
    if (length > MAX_LINELENGTH)
      {
      /* LCOV_EXCL_START */
      BOOL temp = main_initialized;
      main_initialized = TRUE;
      error_moan(66, MAX_LINELENGTH);
      main_initialized = temp;
      break;
      /* LCOV_EXCL_STOP */
      }

    /* Extend the line buffer */

    newtext = store_get(length + BUFFGETSIZE);
    if (newtext == NULL)
      {
      /* LCOV_EXCL_START */
      error_moan(1, length+BUFFGETSIZE);
      break;
      /* LCOV_EXCL_STOP */
      }

    memcpy(newtext, line->text, length);
    store_free(line->text);
    line->text = newtext;
    s = line->text + length;
    maxlength += BUFFGETSIZE;
    }

  /* Now have room for the byte or tab spaces */

  if (c == '\t' && main_tabin)
    {
    tabbed = main_tabflag;
    while (need -- > 0)
      {
      *s++ = ' ';
      length++;
      }
    }
  else
    {
    *s++ = c;
    length++;
    }
  }

/* Line is complete */

line->len = length;
if (eof) line->flags |= lf_eof;
if (tabbed) line->flags |= lf_tabs;

/* Free up unwanted memory at end of the buffer, or free the whole buffer if
this is an empty line. */

if (length > 0) store_chop(line->text, length); else
  {
  store_free(line->text);
  line->text = NULL;
  }

return line;
}



/*************************************************
*           Write a line's characters            *
*************************************************/

/* This is used only for data lines; hence testing main_binary is sufficient
(unlike file_nextline()).

Arguments:
  line        line to write
  f           file to write to

Returns:      -1 writing error
               0 error in binary file (bad hex)
              +1 OK
*/

int
file_writeline(linestr *line, FILE *f)
{
int len = line->len;
uschar *p = line->text;

/* Handle binary output */

if (main_binary)
  {
  BOOL ok = TRUE;
  while (len > 0 && isxdigit((usint)(*p))) { len--; p++; }  /* Skip offset */

  while (len-- > 0)
    {
    int cc;
    int c = *p++;
    if (c == ' ') continue;
    if (c == '*') break;

    if ((ch_tab[c] & ch_hexch) != 0)
      {
      int uc = toupper(c);
      cc = (isalpha(uc)?  uc - 'A' + 10 : uc - '0') << 4;
      }
    else
      {
      error_moan(58, c);
      ok = FALSE;
      continue;
      }

    c = *p++;
    len--;
    if ((ch_tab[c] & ch_hexch) != 0)
      {
      int uc = toupper(c);
      cc += isalpha(uc)?  uc - 'A' + 10 : uc - '0';
      }
    else
      {
      error_moan(58, c);
      ok = FALSE;
      }

    fputc(cc, f);
    }

  return ok? 1 : 0;
  }

/* Handle normal (non-binary) output; we need to scan the line only if it is to
have tabs inserted into it. First check for detrailing. */

if (main_detrail_output && p != NULL)
  {
  uschar *pp = p + len - 1;
  while (len > 0 && *pp-- == ' ') len--;
  }

if (main_tabout || (line->flags & lf_tabs) != 0)
  {
  for (int i = 0; i < len; i++)
    {
    int c = p[i];

    /* Search for string of 2 or more spaces, ending at a tabstop. */

    if (c == ' ')
      {
      int n;
      int k = i + 1;
      while (k < len) { if (p[k] != ' ') break; k++; }
      while (k > i + 1 && (k & 7) != 0) k--;

      if ((n = (k - i)) > 1 && (k & 7) == 0)
        {
        /* Found suitable string - use tab(s) and skip spaces */
        c = '\t';
        i = k - 1;
        for (; n > 8; n -= 8) fputc('\t', f);
        }
      }
    fputc(c, f);
    }
  }

/* Untabbed line -- optimize, but don't write if len == 0 because p may be
NULL, and ASAN complains. */

else if (len > 0 && fwrite(p, 1, len, f)){};  /* Avoid compiler warning */

/* Add final LF */

fputc('\n', f);

/* Check that the line was successfully written, and yield the result. */

return ferror(f)? (-1) : (+1);
}



/*************************************************
*           Write current buffer to file         *
*************************************************/

/*
Argument:   file name
Return:     TRUE if OK
*/

BOOL
file_save(uschar *name)
{
FILE *f;
linestr *line = main_top;
BOOL yield = TRUE;

if (name == NULL || name[0] == 0)
  {
  error_moan(59, currentbuffer->bufferno);
  return FALSE;
  }

if (Ustrcmp(name, "-") == 0) f = stdout;
else if ((f = sys_fopen(name, US"w")) == NULL)
  {
  error_moan(5, name, "writing", strerror(errno));
  return FALSE;
  }

while ((line->flags & lf_eof) == 0)
  {
  int rc = file_writeline(line, f);
  if (rc < 0)
    {
    /* LCOV_EXCL_START */
    error_moan(37, name, strerror(errno));
    return FALSE;
    /* LCOV_EXCL_STOP */
    }
  else if (rc == 0) yield = FALSE;   /* Binary failure */
  line = line->next;
  }

if (f != stdout)
  {
  if (fclose(f) != 0)
    {
    /* LCOV_EXCL_START */
    error_moan(37, name, strerror(errno));
    return FALSE;
    /* LCOV_EXCL_STOP */
    }
  }

return yield;
}

/* End of efile.c */
