/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2022 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: December 2022 */


/* This file contains code for making cutting, pasting, and copying blocks of
lines and rectangles. */

#include "ehdr.h"



/*************************************************
*          Cut out or copy a text block          *
*************************************************/

/* A text block is considered to be a string of characters with included
"newlines". It is stored as a chain of line buffers, each break between buffers
signifying the appearance of a "newline".

Arguments:
  startline   the starting line
  endline     the ending line
  startcol    the starting character column
  endcol      the ending character column
  copyflag    TRUE to copy, FALSE to cut

Returns:      nothing
*/

static void
cut_text(linestr *startline, linestr *endline, int startcol, int endcol,
  BOOL copyflag)
{
linestr *nextline, *cutline;
int clen = line_charcount(startline->text, startline->len);
int firstright = (startline == endline)? endcol : clen;

if ((startline->flags & lf_eof) != 0) return;

if (startcol > clen) startcol = clen;
if (firstright > clen) firstright = clen;

/* Extract data from the first line and either add to the last existing buffer,
or create a new buffer if there are none already. Existing buffers are present
when appending. */

if (cut_last == NULL)
  {
  cutline = line_cutpart(startline, startcol, firstright, copyflag);
  cut_buffer = cutline;
  cut_last = cutline;
  }
else
  {
  uschar *a = startline->text + line_offset(startline, startcol);
  uschar *b = startline->text + line_offset(startline, firstright);
  line_insertbytes(cut_last, -1, cut_last->len, a, b - a, 0);
  if (!copyflag)
    {
    line_deletech(startline,  startcol, firstright - startcol, TRUE);
    startline->flags |= lf_shn;
    }
  }

/* Now deal with any subsequent complete lines */

if (!copyflag) cursor_col = startcol;    /* final cursor position */
if (startline == endline) return;        /* operation is all on one line */

/* The operation spreads over at least one line end */

nextline = startline->next;
while (nextline != endline && (nextline->flags & lf_eof) == 0)
  {
  linestr *nnextline = nextline->next;

  /* Copy the line or cut it out of the normal chain. */

  if (copyflag)
    {
    nextline = line_copy(nextline);
    }
  else
    {
    startline->next = nnextline;
    nnextline->prev = startline;
    main_linecount--;

    /* Remove line from the back list. There is only ever one instance of a
    line on this list. */

    for (usint i = 0; i <= main_backtop; i++)
      {
      if (main_backlist[i].line == nnextline)
        {
        if (main_backtop == 0)
          {
          /* Not sure if this can ever actually be the case. */
          main_backlist[0].line = NULL;  /* LCOV_EXCL_LINE */
          }
        else
          {
          memmove(main_backlist + i, main_backlist + i + 1,
            (main_backtop - i) * sizeof(backstr));
          if (main_backnext == main_backtop) main_backnext--;
          main_backtop--;
          }
        break;
        }
      }
    }

  /* Flatten line number, and add to cut buffer chain */

  nextline->key = 0;
  cut_last->next = nextline;
  nextline->prev = cut_last;
  nextline->next = NULL;
  cut_last = nextline;

  /* And carry on */

  nextline = nnextline;
  }

/* Now take out the initial part of the final line */

clen = line_charcount(nextline->text, nextline->len);
if (endcol > clen) endcol = clen;
cutline = line_cutpart(nextline, 0, endcol, copyflag);
cut_last->next = cutline;
cutline->prev = cut_last;
cut_last = cutline;

/* Now, unless copying, join the two lines on either side of the split; if
there is no data left before the split, cancel that line. */

if (!copyflag)
  {
  if (startline->len == 0)
    {
    line_delete(startline, FALSE);
    main_current = endline;
    }
  else
    {
    if ((nextline->flags & lf_eof) != 0) main_current = endline;
      else main_current = line_concat(endline, 0);
    }
  }
}



/*************************************************
*             Delete text block                  *
*************************************************/

/*
Arguments:
  startline   the starting line
  endline     the ending line
  startcol    the starting character column
  endcol      the ending character column

Returns:      nothing
*/

static void
cut_deletetext(linestr *startline, linestr *endline, int startcol, int endcol)
{
linestr *nextline;
int clen = line_charcount(startline->text, startline->len);
int firstright = (startline == endline)? endcol : clen;

if ((startline->flags & lf_eof) != 0) return;

if (startcol > clen) startcol = clen;
if (firstright > clen)
  {
  firstright = clen;
  startline->flags |= lf_clend;
  }
else startline->flags |= lf_shn;

/* Delete data from first line */

line_deletech(startline, startcol, firstright - startcol, TRUE);

/* Delete any subsequent complete lines */

cursor_col = startcol;               /* final cursor position */
if (startline == endline) return;    /* it all happens on one line */

/* The action spreads over more than one line */

nextline = startline->next;
while (nextline != endline && (nextline->flags & lf_eof) == 0)
  nextline = line_delete(nextline, TRUE);

/* Now take out the initial part of the final line */

clen = line_charcount(nextline->text, nextline->len);
if (endcol > clen) endcol = clen;
line_deletech(nextline, 0, endcol, TRUE);
nextline->flags |= lf_shn;

/* Now joint the two lines on either side of the split */

if (startline->len == 0)
  {
  line_delete(startline, FALSE);
  main_current = endline;
  }
else
  {
  if ((nextline->flags & lf_eof) != 0) main_current = endline;
    else main_current = line_concat(endline, 0);
  }
}



/*************************************************
*             Paste in a text block              *
*************************************************/

/* This function is called only when there is at least one buffer of data in
the cut buffer.

Arguments:   none
Returns:     the number of "newlines" inserted
*/

int
cut_pastetext(void)
{
linestr *line = main_current;
linestr *pline = cut_buffer;
int oldlinecount = main_linecount;
int added;
BOOL ateof = (line->flags & lf_eof) != 0;

cut_pasted = TRUE;

/* If the cursor is at the left hand side, move the line pointer back to the
previous line. If the cursor is not at the left hand side, insert the first
buffer's chars into the current line, and split if there is more data. */

if (cursor_col == 0)
  {
  line = line->prev;
  if (line != NULL) cmd_recordchanged(line, 0);
  }
else
  {
  line_insertbytes(line, cursor_col, -1, pline->text, pline->len, 0);
  line->flags |= lf_shn;
  cursor_col += line_charcount(pline->text, pline->len);
  pline = pline->next;

  /* If there are no more buffers of text, we are done. Otherwise, split the
  current line after the first insertion, but if we were at end of file, the
  action of inserting characters will have split the line already. */

  if (pline == NULL) return 0;
  main_current = ateof? line->next : line_split(line, cursor_col);
  }

/* Now insert all but the last buffer as independent lines. Ensure that the
chain of lines is correct at all times. */

while (pline->next != NULL)
  {
  linestr *nline = line_copy(pline);
  if (line == NULL) main_top = nline; else line->next = nline;
  main_current->prev = nline;
  nline->next = main_current;
  nline->prev = line;
  nline->flags |= lf_shn;
  line = nline;
  pline = pline->next;
  main_linecount++;
  }

/* Now insert final section of data. However, we want to avoid adding zero
chars to the eof line, as this causes an unwanted null line to be created. Not
inserting zero bytes to other lines does no harm! */

if (pline->len > 0)
  line_insertbytes(main_current, 0, -1, pline->text, pline->len, 0);

main_current->flags |= lf_shn;
cursor_col = line_charcount(pline->text, pline->len);
cmd_recordchanged(main_current, cursor_col);

/* Compute the number of "newlines" added */

added = main_linecount - oldlinecount;
return added;
}



/*************************************************
*          Cut out a rectangle of data           *
*************************************************/

/*
Arguments:
  startline   the starting line
  endline     the ending line
  startcol    the starting character column
  endcol      the ending character column
  copyflag    TRUE for copy, FALSE for cut

Returns:      nothing
*/

static void
cut_rect(linestr *startline, linestr *endline, int startcol, int endcol,
  BOOL copyflag)
{
linestr *line = startline;
int left, right;

if (startcol < endcol)
  { left = startcol; right = endcol; }
else
  { left = endcol; right = startcol; }

if (startcol != endcol) for (;;)
  {
  linestr *cutline = line_cutpart(line, left, right, copyflag);
  if (cut_last == NULL) cut_buffer = cutline;
    else cut_last->next = cutline;
  cutline->prev = cut_last;
  cut_last = cutline;
  if (line == endline) break;
  line = line->next;
  }

if (!copyflag) cursor_col = left;
}



/*************************************************
*          Delete a rectangle of data            *
*************************************************/

/*
Arguments:
  startline   the starting line
  endline     the ending line
  startcol    the starting character column
  endcol      the ending character column

Returns:      nothing
*/

static void
cut_deleterect(linestr *startline, linestr *endline, int startcol, int endcol)
{
linestr *line = startline;
int left, right;

if (startcol < endcol)
  { left = startcol; right = endcol; }
else
  { left = endcol; right = startcol; }

if (startcol != endcol) for (;;)
  {
  line_deletech(line, left, right - left, TRUE);
  line->flags |= lf_shn;
  if (line == endline) break;
  line = line->next;
  }

cursor_col = left;
}



/*************************************************
*           Paste in rectangular data            *
*************************************************/

/* This function is called only when there is something in the cut buffer. The
cursor is at the top lefthand corner of the pasted in rectangle. */

void
cut_pasterect(void)
{
linestr *line = main_current;
linestr *pline = cut_buffer;
usint maxwidth = 0;

cut_pasted = TRUE;

/* Find maximum width */

while (pline != NULL)
  {
  if (pline->len > maxwidth) maxwidth = pline->len;
  pline = pline->next;
  }

/* Now do the pasting */

pline = cut_buffer;
while (pline != NULL)
  {
  int ateof = (line->flags & lf_eof) != 0;
  int width = pline->len;

  /* Modify the line if there is data, or if it's the eof line (which will
  be converted into a non-eof line and a new eof line invented). */

  if (width > 0 || ateof)
    line_insertbytes(line, cursor_col, -1, pline->text, width, maxwidth-width);
  line->flags |= lf_shn;
  pline = pline->next;
  line = line->next;
  }
}



/*************************************************
*         Check for permitted cut overwrite      *
*************************************************/

/*
Argument:   prompt string
Returns:    TRUE/FALSE for yes/no
*/

BOOL
cut_overwrite(uschar *s)
{
int i;
linestr *line = cut_buffer;
error_moan(28);
error_printf("** The first few lines are:\n");
for (i = 0; i < 10 && line != NULL; i++)
  {
  line_verify(line, FALSE, FALSE);
  line = line->next;
  }
return cmd_yesno(CCS s, 0, 0, 0, 0);
}



/*************************************************
* Cut out, copy out, or delete text or rectangle *
*************************************************/

/* The line and column that were marked are given as arguments, so that the
mark can be deleted before calling this function (before anything is disturbed).

Arguments:
  markline    the marked line
  markcol     the marked column
  type        mark_text or mark_rect
  copyflag    TRUE to copy, FALSE to cut
  deleteflag  TRUE to delete (as opposed to copy or cut)

Returns:      normally TRUE; FALSE if abort because of an unpasted cut buffer
*/

BOOL
cut_cut(linestr *markline, usint markcol, int type, BOOL copyflag,
  BOOL deleteflag)
{
linestr *startline, *endline;
int above = line_checkabove(markline);
int startcol, endcol;

if (!cut_pasted && !deleteflag && !main_appendswitch && cut_buffer != 0 &&
  (cut_buffer->len != 0 || cut_buffer->next != NULL) && main_warnings)
  {
  if (!cut_overwrite(US"Continue with CUT or COPY (Y/N)? ")) return FALSE;
  }

if (!deleteflag) cut_pasted = FALSE;

if (above > 0 || (above == 0 && markcol < cursor_col))
  {
  startline = markline;
  endline = main_current;
  startcol = markcol;
  endcol = cursor_col;
  }
else
  {
  startline = main_current;
  endline = markline;
  startcol = cursor_col;
  endcol = markcol;
  }

if (!deleteflag) cut_type = (type == mark_text)? cuttype_text : cuttype_rect;

/* Take appropriate actions for cut/copy/delete */

if (deleteflag)
  {
  if (type == mark_text)
    cut_deletetext(startline, endline, startcol, endcol);
  else
    cut_deleterect(startline, endline, startcol, endcol);
  }
else
  {
  /* Delete stuff already in the cut buffer, unless the global "append while
  cutting" switch is set. */

  if (!main_appendswitch)
    {
    while (cut_buffer != NULL)
      {
      linestr *next = cut_buffer->next;
      store_free(cut_buffer->text);
      store_free(cut_buffer);
      cut_buffer = next;
      }
    cut_last = NULL;
    }

  /* Cut rectangle or text */

  if (type == mark_text)
    cut_text(startline, endline, startcol, endcol, copyflag);
  else
    cut_rect(startline, endline, startcol, endcol, copyflag);
  }

return TRUE;
}

/* End of ecutcopy.c */
