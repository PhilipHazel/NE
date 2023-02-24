/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */


/* This file contains code for making changes to individual lines */

#include "ehdr.h"



/*************************************************
*  Get byte offset for character in byte string  *
*************************************************/

/* Given a character number, find the byte offset in the string.

Arguments:
  s           start of string
  e           end of string
  c           character number

Returns:      the byte offset
*/

int
line_soffset(uschar *s, uschar *e, int c)
{
uschar *ss = s;
while (c > 0 && ss < e)
  {
  SKIPCHAR(ss, e);
  c--;
  }
return ss - s + c;
}



/*************************************************
*     Get byte offset for character in line      *
*************************************************/

/* Given a character column, scan the line to find the byte offset. If the
character column is past the end of the line, behave as if there were following
spaces.

Arguments:
  line      the line structure
  col       the character column

Returns:    the byte offset
*/

usint
line_offset(linestr *line, int col)
{
uschar *pe;
uschar *p = line->text;
if (p == NULL) return col;  /* Empty line may be NULL with zero length */
pe = p + line->len;
while (col > 0 && p < pe)
  {
  SKIPCHAR(p, pe);
  col--;
  }
return p - line->text + col;
}



/*************************************************
*         Count characters in a byte string      *
*************************************************/

/* Scan the byte string, counting characters (possibly multibyte).

Arguments:
  ptr        points to the string
  len        the byte length of the string

Returns:     the character count
*/

usint
line_charcount(uschar *ptr, usint len)
{
uschar *pe;
int yield = 0;
if (len == 0) return 0;  /* Avoids problem if ptr is NULL */
pe = ptr + len;
while (ptr < pe)
  {
  SKIPCHAR(ptr, pe);
  yield++;
  }
return yield;
}



/*************************************************
*         Count bytes in a character string      *
*************************************************/

/* Scan the character string, counting bytes. We assume that the character
string is well defined in the sense that all the bytes are present.

Arguments:
  ptr         points to the string
  len         the character length of the string

Returns:      the byte count
*/

int
line_bytecount(uschar *ptr, int len)
{
uschar *ps = ptr;
while (len-- > 0) SKIPCHAR(ptr, ptr + 10);
return ptr - ps;
}



/*************************************************
*           Check position of line               *
*************************************************/

/* If the given line is above the current line, the yield is the count of lines
between them; otherwise it is -1. */

int
line_checkabove(linestr *line)
{
int count = 0;
linestr *up = main_current;
while (up != line && up != NULL)
  {
  count++;
  up = up->prev;
  }
return (up == NULL)? -1 : count;
}



/*************************************************
*             Make a copy of a line              *
*************************************************/

linestr *
line_copy(linestr *line)
{
linestr *nline = store_getlbuff(line->len);
nline->key = line->key;
nline->flags = line->flags;
if (line->len > 0) memcpy(nline->text, line->text, line->len);
return nline;
}



/*************************************************
*           Insert bytes into a line             *
*************************************************/

/* The bytes are inserted either at a given character column or at a given byte
"column", the line being extended with spaces if necessary. Note that this
function does NOT set lf_shn in the line.

Arguments:
  line          the line
  acol          the character column or -1
  abcol         the byte offset or -1
  ptr           point to replacement string or NULL for no insert (just pad)
  count         number of bytes in replacement string
  padcount      number of spaces to be added *after* the insert

Returns:        nothing
*/

void
line_insertbytes(linestr *line, int acol, int abcol, uschar *ptr, int count,
  usint padcount)
{
usint oldlen = line->len;
usint extra, newlen;
usint leftcount, rightcount;
usint col, bcol;
uschar *newtext, *np;

/* Set up according to byte or character offset */

if (abcol < 0)    /* Character offset given */
  {
  col = (usint)acol;
  bcol = line_offset(line, acol);
  }
else              /* Byte offset given */
  {
  col = line_charcount(line->text, abcol);
  bcol = (usint)abcol;
  }

/* Now do the business */

extra = (bcol > oldlen)? bcol - oldlen : 0;
newlen = oldlen + extra + count + padcount;
newtext = store_Xget(newlen);
np = newtext;

leftcount = (extra == 0)? bcol : oldlen;
rightcount = (oldlen >= bcol)? oldlen - bcol : 0;

/* Before each memcpy(), check for a zero length, because ptr or line->text may
be NULL when using this function just for padding. */

if (leftcount > 0)
  {
  memcpy(np, line->text, leftcount);
  np += leftcount;
  }
for (usint i = 0; i < extra; i++) *np++ = ' ';
if (count > 0)
  {
  memcpy(np, ptr, count);
  np += count;
  }
for (usint i = 0; i < padcount; i++) *np++ = ' ';
if (rightcount > 0) memcpy(np, line->text + bcol, rightcount);

if (line->text != NULL) store_free(line->text);
line->text = newtext;
line->len = newlen;

/* If we have added data to the end-of-file line, make a new, null eof line and
add it on the end. */

if ((line->flags & lf_eof) != 0)
  {
  main_bottom = store_getlbuff(0);
  line->next = main_bottom;
  main_bottom->prev = line;
  line->flags &= ~lf_eof;
  main_bottom->flags |= lf_eof + lf_shn;
  main_linecount++;

  if (extra == 0)
    {
    line->flags &= ~lf_shn;     /* must have been previous insert */
    line->flags |= lf_clend;    /* clear old eof marker */
    }
  else line->flags |= lf_shn;
  }

/* Sometimes data is added to an eof line on the screen more than once before
the main part of NE gets control. We need to make sure that the whole line is
shown if necessary. The following fudge covers this case. NB This probably no
longer applies. NB */

if ((line->flags & lf_clend) != 0 && extra > 0) line->flags |= lf_shn;

/* Adjust the marks if necessary; note that here we are dealing with character
columns. Then note that the line has changed. */

if (mark_line == line && col <= mark_col)
  mark_col += line_charcount(ptr, count) + padcount;

if (mark_line_global == line && col <= mark_col_global)
  mark_col_global += line_charcount(ptr, count) + padcount;

cmd_recordchanged(line, col + count + padcount);
}




/*************************************************
*        Delete chars or bytes from line         *
*************************************************/

/* With the advent of UTF-8 support, sometimes we know what to delete in terms
of characters (typically when screen handling) and sometimes in terms of bytes
(typically after a command such as a, b, or e). This function deals with
handling either, but we call it via two simpler interfaces.

Note that lf_shn is NOT set by this function, because it is called from the
screen handler when deletion is reported from the terminal[*], in which case a
reshow is not wanted. However, the mark may be flagged for redisplay. [*] I
suspect this relates to old terminal types that are no longer supported, so
perhaps this could be tidied up.

Arguments:
  line          the line
  col           the character column in the line or -1
  count         the number of characters to delete or -1
  bcol          the byte offset in the line or -1
  bcount        the byte count to delete or -1
  forwardsflag  TRUE for delete to right, FALSE for delete to left

Returns:        nothing
*/

static void
line_deletepart(linestr *line, int col, int count, int bcol, int bcount,
  BOOL forwardsflag)
{
uschar *a, *b, *t, *z;
int backcol;

if (line->text == NULL) return;    /* Empty line */

/* Set up with byte data; convert to characters */

if (col < 0)
  {
  col = line_charcount(line->text, bcol);
  count = line_charcount(line->text + bcol, bcount);
  }

/* Set up with character data (byte count not needed) */

else
  {
  bcol = line_offset(line, col);
  }

backcol = col;

a = b = line->text + bcol;
z = line->text + line->len;

if (forwardsflag)
  {
  if (a >= z) return;              /* Past the end of the line */
  for (int i = 0; i < count; i++)
    if (b < z) { SKIPCHAR(b, z); } else break;
  }

else
  {
  for (int i = 0; i < count; i++)
    if (a > z) a--; else BACKCHAR(a, line->text);
  if (a >= z) return;
  if (b >= z) b = z;
  backcol -= count;
  if (backcol < 0) backcol = 0;
  }

/* We now have a and b pointing to the start and end of at least one byte
within the data of the line. These bytes are to be deleted. First, transfer
characters to the undelete queue. We put them in an existing line if possible;
else get a new line and ensure there aren't too many lines extant. */

if (main_undelete == NULL || (main_undelete->flags & lf_udch) == 0 ||
      main_undelete->len + 2*(b-a) > 256)
  {
  linestr *new = store_getlbuff((b-a > 128)? 2*(b-a) : 256);
  new->flags |= lf_udch;
  new->len = 0;
  new->next = main_undelete;
  if (main_lastundelete == NULL) main_lastundelete = new;
    else main_undelete->prev = new;
  main_undelete = new;
  main_undeletecount++;
  while (main_undeletecount > max_undelete)
    {
    linestr *prev = main_lastundelete->prev;
    if (prev == NULL) break;   /* Should not occur */
    prev->next = NULL;
    store_free(main_lastundelete->text);
    store_free(main_lastundelete);
    main_lastundelete = prev;
    main_undeletecount--;
    }
  }

t = main_undelete->text + main_undelete->len;

if (forwardsflag)
  {
  uschar *s = a;
  while (s < b)
    {
    uschar *ss = s;
    *t++ = TRUE;
    SKIPCHAR(s, b);    /* This checks for valid UTF-8 */
    memcpy(t, ss, s - ss);
    t += s - ss;
    }
  }
else
  {
  uschar *s = b;
  while (s > a)
    {
    uschar *ss = s;
    *t++ = FALSE;
    BACKCHAR(s, a);
    memcpy(t, s, ss - s);
    t += ss - s;
    }
  }

main_undelete->len = t - main_undelete->text;

/* Close up the line, adjust its length, mark it changed, and sort out the mark
positions if necessary. */

line->len -= b - a;
memmove(a, b, z - b);

store_chop(line->text, line->len);
cmd_recordchanged(line, backcol);

if (mark_line == line)
  {
  int gap = mark_col - col + (forwardsflag? 0 : count);
  if (gap > 0) mark_col -= (gap > count)? count : gap;
  }

if (mark_line_global == line)
  {
  int gap = mark_col_global - col + (forwardsflag? 0 : count);
  if (gap > 0) mark_col_global -= (gap > count)? count : gap;
  }
}



/*************************************************
*           Delete chars from line               *
*************************************************/

/* This is just a convenient wrapper for line_deletepart().

Arguments:
  line          the line
  col           the character column in the line
  count         the number of characters to delete
  forwardsflag  TRUE for delete to right, FALSE for delete to left

Returns:        nothing
*/

void
line_deletech(linestr *line, int col, int count, BOOL forwardsflag)
{
line_deletepart(line, col, count, -1, -1, forwardsflag);
}



/*************************************************
*           Delete bytes from line               *
*************************************************/

/* This is just a convenient wrapper for line_deletepart().

Arguments:
  line          the line
  col           the byte offset in the line
  count         the number of bytes to delete
  forwardsflag  TRUE for delete to right, FALSE for delete to left

Returns:        nothing
*/

void
line_deletebytes(linestr *line, int offset, int count, BOOL forwardsflag)
{
line_deletepart(line, -1, -1, offset, count, forwardsflag);
}



/*************************************************
*              Delete line                       *
*************************************************/

/* The line is either added to the "deleted" list, so that it can be undeleted,
or completely discarded.

Arguments:
  line        line to be deleted
  undelete    TRUE to keep on the deleted list

Returns:      the following line
*/

linestr *
line_delete(linestr *line, BOOL undelete)
{
linestr *prevline = line->prev;
linestr *nextline = line->next;

nextline->prev = prevline;
if (prevline == NULL) main_top = nextline; else prevline->next = nextline;

/* If required, add the line to the deleted list, ensuring that there are
only so many lines on the list. */

if (undelete)
  {
  line->prev = NULL;
  line->next = main_undelete;
  if (main_lastundelete == NULL) main_lastundelete = line;
    else main_undelete->prev = line;
  main_undelete = line;
  main_undeletecount++;
  while (main_undeletecount > max_undelete)
    {
    linestr *prev = main_lastundelete->prev;
    if (prev == NULL) break;   /* Should not occur */
    prev->next = NULL;
    store_free(main_lastundelete->text);
    store_free(main_lastundelete);
    main_lastundelete = prev;
    main_undeletecount--;
    }
  }

/* Otherwise free the line's memory. If we are in screen mode, ensure that the
table of lines that are currently displayed does not contain the line we are
about to throw away. This is necessary because a new line could be read before
re-display happens, and it might re-use the same block of memory, leading to
confusion. We can't set the value to NULL, as that implies the screen line is
blank. Use (+1) and assume that will never be a valid line address... */

else
  {
  if (main_screenOK)
    {
    for (usint i = 0; i <= window_depth; i++)
      if (window_vector[i] == line) window_vector[i] = (linestr *)(+1);
    }

  store_free(line->text);
  store_free(line);
  }

/* Remove deleted lines from the back list. There is only ever one instance of
a line on the list. */

for (usint i = 0; i <= main_backtop; i++)
  {
  if (main_backlist[i].line == line)
    {
    if (main_backtop == 0)
      {
      main_backlist[0].line = NULL;
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

/* Mark the next line changed, and move any marks from the deleted line onto
it. */

cmd_recordchanged(nextline, 0);

if (mark_line == line)
  {
  mark_line = nextline;
  mark_col = 0;
  nextline->flags |= lf_shn;
  }

if (mark_line_global == line)
  {
  mark_line_global = nextline;
  mark_col_global = 0;
  nextline->flags |= lf_shn;
  }

main_linecount--;
return nextline;
}



/*************************************************
*               Align line                       *
*************************************************/

/* The final argument is the address of a second result -- the number of spaces
inserted into the line. It may be negative.

Arguments:
  line      the line
  col       the character column
  a_count   where to return number of spaces inserted (negative for deleted)

Returns:    nothing
*/

void
line_leftalign(linestr *line, int col, int *a_count)
{
int leftsig = -1;
int oldlen = line->len;
uschar *p = line->text;

for (int i = 0; i < oldlen; i++) if (p[i] != ' ') { leftsig = i; break; }

if (leftsig == col)
  {
  *a_count = 0;
  return;
  }

if (leftsig < col)
  {
  int insert = col - leftsig;
  *a_count = insert;
  line_insertbytes(line, 0, -1, NULL, 0, insert);
  }

else
  {
  int extract = leftsig - col;
  *a_count = -extract;
  line_deletech(line, 0, extract, TRUE);
  }

cmd_recordchanged(line, col);
}



/*************************************************
*                 Split Line                     *
*************************************************/

/*
Arguments:
  line       the line to be split
  col        the character column

Returns:     pointer to the new line, i.e. the 2nd part of the old line
*/

linestr *
line_split(linestr *line, usint col)
{
usint bcol = line_offset(line, col);
usint newlen = (line->len > bcol)? line->len - bcol : 0;
linestr *splitline = store_getlbuff(newlen);
linestr *nextline = line->next;

splitline->prev = line;
if (nextline == NULL) main_bottom = splitline;
  else nextline->prev = splitline;
splitline->next = nextline;
line->next = splitline;

if (newlen > 0) memcpy(splitline->text, line->text + bcol, newlen);

/* Adjust mark data if a mark was in second part of line */

if (mark_line == line && mark_col >= col)
  {
  mark_line = splitline;
  mark_col -= col;
  }

if (mark_line_global == line && mark_col_global >= col)
  {
  mark_line_global = splitline;
  mark_col_global -= col;
  }

/* If line was marked for showing, mark second part */

if ((line->flags & lf_shn) != 0) splitline->flags |= lf_shn;

/* If line was marked for clearing at end (usually the eof marker) mark the
second half, and unmark the first half if the split was in the real text part.
*/

else if ((line->flags & lf_clend) != 0)
  {
  /* LCOV_EXCL_START - can only happen in screen mode */
  if (bcol <= line->len) line->flags &= ~lf_clend;
  splitline->flags |= lf_clend;
  /* LCOV_EXCL_STOP */
  }

/* Adjust lengths of both parts */

if (bcol < line->len)
  {
  line->len = bcol;
  store_chop(line->text, bcol);
  }
splitline->len = newlen;

/* Deal with a split of the end of file line */

if ((line->flags & lf_eof) != 0)
  {
  line->flags &= ~lf_eof;
  line->flags |= lf_clend;
  splitline->flags |= lf_eof;
  if (col != cursor_offset) splitline->flags |= lf_shn;
  }

cmd_recordchanged(splitline, 0);

main_linecount++;
return splitline;
}



/*************************************************
*       Concatenate line with previous           *
*************************************************/

/* The existence of a previous line has already been checked when this function
is called.

Arguments:
  line        the second of the two lines
  padcount    count of spaces to insert between the lines

Returns:      the combined line
*/

linestr *
line_concat(linestr *line, int padcount)
{
linestr *prev = line->prev;
int newlen = line->len + prev->len + padcount;
int backcol = line_charcount(prev->text, prev->len);
uschar *newtext = store_Xget(newlen);
uschar *p;

if (mark_line == line) mark_col += backcol + padcount;
if (mark_line_global == line) mark_col_global += backcol + padcount;

if (prev->len > 0) memcpy(newtext, prev->text, prev->len);
for (p = newtext + prev->len; p < newtext + prev->len + padcount; p++) *p = ' ';
if (line->len > 0)
  memcpy(newtext + prev->len + padcount, line->text, line->len);

store_free(line->text);
line->text = newtext;
line->len = newlen;
line->key = prev->key;
line->flags |= lf_shn;

(void) line_delete(prev, FALSE);
cmd_recordchanged(line, backcol);
return line;
}



/*************************************************
*                  Verify line                   *
*************************************************/

/* Shouldn't be called for eof line unless shownumber = TRUE, but always print
the line number in that case.

Arguments:
  line         the line to verify
  shownumber   TRUE if the line number is to be shown
  showcursor   TRUE if the cursor position is to be shown

Returns:       nothing
*/

void
line_verify(linestr *line, BOOL shownumber, BOOL showcursor)
{
if ((line->flags & lf_eof) != 0)
  {
  if (line->key > 0) error_printf("%d.*\n", line->key);
    else error_printf("****.*\n");
  return;
  }

if (shownumber)
  {
  if (line->key > 0) error_printf("%d.\n", line->key);
    else error_printf("****.\n");
  }

/* Loop for several lines of output, the number depending on the size of
non-printing characters encountered. */

for (usint n = 1, i = 0; i < n; i++)
  {
  uschar *p = (line->text == NULL)? (uschar *)"" : line->text;
  uschar *pe = p + line->len;

  while (p < pe)
    {
    usint c, m;
    GETCHARINC(c, p, pe);    /* If allow_wide, may be > 256 */

    /* Handle displayable characters < 256; show those that are greater than
    127 if either the terminal is UTF-8 or if eightbit is set. Output the
    character when i == 0; otherwise output a space. */

    if (c < 256 && (ch_displayable[c/8] & (1<<(c%8))) == 0)
      {
      if (c < 127)
        {
        error_printf("%c", (i == 0)? c : ' ');
        continue;
        }
      else if (main_interactive)
        {
        /* LCOV_EXCL_START */
        if (main_utf8terminal)
          {
          if (i == 0)
            {
            uschar buff[8];
            int len = ord2utf8(c, buff);
            error_printf("%.*s", len,  CS buff);
            }
          else error_printf(" ");
          continue;
          }
        else if (main_eightbit)
          {
          error_printf("%c", (i == 0)? c : ' ');
          continue;
          }
        /* LCOV_EXCL_STOP */
        }
      }

    /* The character has not been shown, either because it was > 255 or because
    it was deemed non-printing. In the first pass (i == 0), set the number of
    lines needed to show it vertically in hex. Then output the appropriate
    nibble. */

    m = (c < 0x00000100)? 2 :      /* Lines needed for this char */
        (c < 0x00001000)? 3 :
        (c < 0x00010000)? 4 :
        (c < 0x00100000)? 5 :
        (c < 0x01000000)? 6 :
        (c < 0x10000000)? 7 : 8;

    if (i == 0 && n < m) n = m;

    if (i < m) error_printf("%x", (c >> (4*(m - i - 1))) & 15);
      else error_printf(" ");
    }

  error_printf("\n");
  }

if (showcursor && cursor_col > 0)
  {
  for (usint i = 1; i < cursor_col; i++) error_printf(" ");
  error_printf(">");
  main_verified_ptr = TRUE;
  }
error_printflush();
}



/*************************************************
*         Support functions for formatting       *
*************************************************/

/* Functions for determining paragraph beginnings and endings. If the offset
is non-zero, we want to check from there on. */

static BOOL
parbegin(linestr *line, int offset)
{
if (par_begin == NULL)
  {
  for (usint i = offset; i < line->len; i++)
    if (line->text[i] != ' ') return TRUE;
  return FALSE;
  }
else
  {
  match_leftpos = offset;
  match_rightpos = line->len;
  return cmd_matchse(par_begin, line) == MATCH_OK;
  }
}

/* The indents etc. help with formatting indented and/or tagged paragraphs. If
there is non-zero indent+tag, check the remainder of the line against the
configured ending pattern. */

static BOOL
parend(linestr *line, usint indent, usint indent2, uschar *leftbuf,
  usint leftbuflen)
{
if ((line->flags & lf_eof) != 0) return TRUE;

if (par_end == NULL)
  {
  if (line->len == 0 || line->text[0] == ' ') return TRUE;
  }

else
  {
  match_leftpos = 0;
  match_rightpos = line->len;
  if (cmd_matchse(par_end, line) == MATCH_OK) return TRUE;

  if (indent + leftbuflen > 0)
    {
    match_leftpos = indent + leftbuflen;
    if (cmd_matchse(par_end, line) == MATCH_OK) return TRUE;
    }
  }

/* Check for mismatching indents and tag. */

if (indent + leftbuflen + indent2 > 0)
  {
  usint ii;
  uschar *pp = line->text;

  if (indent < line->len)
    {
    for (ii = 0; ii < indent; ii++) if (*pp++ != ' ') break;
    if (ii < indent) return TRUE;
    }

  if (leftbuflen > 0)
    if (leftbuflen > line->len - indent ||
        Ustrncmp(pp, leftbuf, leftbuflen) != 0) return TRUE;

  if (indent2 > 0)
    {
    if (line->len < indent + leftbuflen + indent2) return TRUE;
    pp += leftbuflen;
    for (ii = 0; ii < indent2; ii++) if (*pp++ != ' ') break;
    if (ii < indent2) return TRUE;
    }
  }

return FALSE;
}


/* Functions for helping decide whether to concatenate lines. The first
computes the number of character spaces left, where width is the splitting
character column. The returned value allows for one space to be inserted at the
join. */

static usint
spaceleft(linestr *line, usint width)
{
usint yield = width - line_charcount(line->text, line->len);
if (line->len > 0 && line->text[line->len-1] != ' ') yield--;
return yield;
}

/* Return the length in characters of the first word in a line. */

static usint
firstwordlen(linestr *line)
{
usint n = 0;
uschar *p = line->text;
uschar *pe = p + line->len;
while (p < pe)
  {
  if (*p == ' ') break;
  n++;
  SKIPCHAR(p, pe);
  }
return n;
}

/* Function to remove trailing spaces in a line */

static void
detrail(linestr *line)
{
int i;
uschar *p = line->text;
for (i = line->len - 1; i >= 0; i--) if (p[i] != ' ') break;
line->len = i + 1;
store_chop(line->text, line->len);
}



/*************************************************
*       Cut out or copy out part of a line       *
*************************************************/

/* The yield is a new line containing the characters that have been cut out. If
cutting, the original line is closed up, but still in the same buffer.

Arguments:
  line      the line we are cutting from
  left      the left most character
  right     after the rightmost character
  copyflag  TRUE for copy, FALSE for cut

Returns:    the characters cut out
*/

linestr *
line_cutpart(linestr *line, int left, int right, BOOL copyflag)
{
usint bleft = line_offset(line, left);
usint bright = line_offset(line, right);
int count = bright - bleft;
linestr *cutline = store_getlbuff(count);
uschar *ip = line->text;
uschar *op = cutline->text;

/* If line->len is zero, line->text (and therefore ip) may be NULL. In this
circumstance we are always generating spaces. Avoid trying to add to NULL. */

if (line->len > 0) ip += bleft;
for (usint i = bleft; i < bright; i++) *op++ = (i >= line->len)? ' ' : *ip++;

cutline->len = count;
cutline->flags |= lf_shn;

if (!copyflag)
  {
  if (bright >= line->len)
    {
    if (bleft < line->len)
      {
      line->len = bleft;
      line->flags |= lf_clend;
      cmd_recordchanged(line, left);
      }
    }
  else
    {
    line_deletebytes(line, bleft, count, TRUE);
    line->flags |= lf_shn;
    }
  }

return cutline;
}



/*************************************************
*              Format Paragraph                  *
*************************************************/

/* This function is called only when there is a non-eof current line. It can
both format and unformat a paragraph - the latter is just formatting with an
"infinite" line length.

Argument: TRUE to unformat, FALSE to format
Returns:  nothing
*/

void
line_formatpara(BOOL unformat)
{
uschar leftbuf[20];
uschar *p, *q;
usint indent = 0;
usint indent2 = 0;
usint leftbuflen = 0;
usint minlen = 0;
usint width = main_rmargin;
usint len;
BOOL one_line_para;
linestr *nextline = main_current->next;

/* If unformatting, set the width "infinite"; otherwise, if margin is disabled,
use the remembered value */

if (unformat) width = (usint)(-1);
  else if (width > MAX_RMARGIN) width -= MAX_RMARGIN;

/* If we are near the end of the file, we want to be sure that the whole screen
is used. The ScreenHint routine can be coerced into doing the right thing. */

if (main_screenOK) scrn_hint(sh_insert, -1, NULL);

/* If we are not at a beginning-of-paragraph line, just move on. */

if (!parbegin(main_current, 0))
  {
  main_current = nextline;
  cursor_col = 0;
  return;
  }

/* Search the start of the current line for initial indentation and tag text
that might apply to the whole paragraph. But ignore trailing spaces. All
characters involved here are ASCII and so must be single-byte. */

p = main_current->text;
q = leftbuf;

len = main_current->len;
while (len > 0 && p[len-1] == ' ') len--;

while (p - main_current->text < len && *p == ' ')
  {
  indent++;
  p++;
  }

while (p - main_current->text < len && strchr("#%*+=|~<> ", *p) != NULL)
  {
  *q++ = *p++;
  if (q - leftbuf > 16) { q = leftbuf; break; }
  }
*q = 0;
leftbuflen = Ustrlen(leftbuf);

/* If we have found any potential indentation and text, use it if either this
is a one-line paragraph, or if it matches the next line. */

one_line_para = parend(nextline, indent, indent2, leftbuf, leftbuflen);

if ((indent > 0 || leftbuflen > 0) && !one_line_para)
  {
  BOOL magic = FALSE;
  p = nextline->text;

/* This code looks redundant: indent must always be less than nextline->len as
otherwise this would be a one_line_para. CHECK parend(). */

  if (indent < nextline->len)
    {
    usint i;
    for (i = 0; i < indent; i++) if (*p++ != ' ') break;
    magic = i >= indent;
    }

  if (magic && leftbuflen > 0)
    {
    if (leftbuflen > nextline->len - indent ||
        Ustrncmp(p, leftbuf, leftbuflen) != 0)
    leftbuflen = 0;  /* LCOV_EXCL_LINE - I think this can never happen */
    }

  if (!magic) indent = leftbuflen = 0;
  }

/* Total length of indent + starting string */

minlen = indent + leftbuflen;

/* Now that we know the indent and text, we must re-check that this line is a
paragraph beginner, taking the preliminary stuff into account. */

if (minlen > 0)
  {
  if (!parbegin(main_current, minlen))
    {
    main_current = nextline;
    cursor_col = 0;
    return;
    }
  }

/* We also want to cope with the case where the second and subsequent lines
of the paragraph are indented more than the first one, after taking the common
indent and start string into account. */

if (!one_line_para)
  {
  p = nextline->text + minlen;
  while (p - nextline->text < nextline->len && *p == ' ')
    {
    indent2++;
    p++;
    }
  }

/* Start of main loop - exit by return. */

for (;;)
  {
  detrail(main_current);

  /* Loop until current line is short enough, counting characters, not bytes. */

  while(line_charcount(main_current->text, main_current->len) > width)
    {
    BOOL ended;
    BOOL gotspace;
    usint ichar;
    usint ibyte;
    usint jchar;
    usint jbyte;
    usint widthoffset;

    cmd_recordchanged(main_current, 0);
    p = main_current->text;
    q = main_current->text + main_current->len;
    for (ichar = 0; ichar < width; ichar++) SKIPCHAR(p, q);
    widthoffset = p - main_current->text;

    /* We now have ichar at the maximum width character offset and p pointing
    after the last possible character that can be on this line. We want to
    search this character and those to the left of it to find a space at which
    to break. */

    gotspace = FALSE;
    while (ichar >= minlen)
      {
      int c;
      GETCHAR(c, p, q);
      if (c == ' ')
        {
        gotspace = TRUE;
        break;
        }
      BACKCHAR(p, main_current->text);
      if (ichar == minlen) break;
      ichar--;
      }

    /* If we have not found a breaking space, break at the character width. In
    both cases we now set i.. to the ending offsets and j.. to the start of the
    rest of the line offsets. */

    if (!gotspace)
      {
      ibyte = jbyte = widthoffset;
      ichar = jchar = width;
      }
    else
      {
      uschar *pj;
      ibyte = p - main_current->text;
      jbyte = ibyte + 1;
      jchar = ichar + 1;

      /* Advance j.. to pass over any spaces */

      pj = p + 1;
      while (jbyte < main_current->len)
        {
        if (*pj++ == ' ')
          {
          jbyte++;
          jchar++;
          }
        else break;
        }

      /* Reverse i.. to cut off trailing spaces */

      while (ibyte > minlen && *(--p) == ' ')
        {
        ibyte--;
        ichar--;
        }
      }

    /* If next is paragraph end, make a new part line and make it the current
    line. */

    ended = parend(nextline, indent, indent2, leftbuf, leftbuflen);

    /* The next line is not to be included in this paragraph. So what we split
    off this line must become a new line on its own. */

    if (ended)
      {
      linestr *extra = line_cutpart(main_current, jchar,
        line_charcount(main_current->text, main_current->len), FALSE);
      main_current->len -= jbyte - ibyte;
      if (main_current->len == widthoffset) main_current->flags |= lf_shn;
      main_current->next = extra;
      extra->next = nextline;
      nextline->prev = extra;
      extra->prev = main_current;
      main_current = extra;
      main_linecount++;

      /* If there's an indent or a tag or 2nd line indent, insert them. */

      if (indent > 0) line_insertbytes(main_current, 0, -1, NULL, 0, indent);
      if (leftbuflen > 0)
        line_insertbytes(main_current, indent, -1, leftbuf, leftbuflen, 0);
      if (indent2 > 0)
        line_insertbytes(main_current, minlen, -1, NULL, 0, indent2);
      }

    /* Otherwise, move text onto the start of the next line, and make it the
    current line. We know that if there's an indent or tag, or second indent,
    they will exist on the next line, so just insert after them. */

    else
      {
      line_insertbytes(nextline, indent + leftbuflen + indent2, -1,
        main_current->text + jbyte,
        main_current->len - jbyte, 1);
      nextline->flags |= lf_shn;
      main_current->len = ibyte;               /* shorten current */
      if (main_current->len == widthoffset) main_current->flags |= lf_shn;
        else main_current->flags |= lf_clend;  /* clear out end */
      main_current = nextline;
      nextline = nextline->next;
      }

    detrail(main_current);                     /* includes store_chop() */
    }

  /* We now have a current line that is shorter than the required width.
  Concatenate with following lines until it exceeds the width, but only if
  there is room for the next word on the line. However, if the end of the
  paragraph is reached, exit from the function. */

  while (line_charcount(main_current->text, main_current->len) <= width)
    {
    BOOL ended = parend(nextline, indent, indent2, leftbuf, leftbuflen);

    /* The next line is not part of this paragraph */

    if (ended)
      {
      main_current = nextline;
      cursor_col = indent;
      return;
      }

    /* We try to decide whether it is worth joining the lines. This is an
    optimisation that works in common cases, but not when the second line
    starts with a space. However, it is fail-safe. */

    if (firstwordlen(nextline) <= spaceleft(main_current, width))
      {
      int sp = 0;
      cmd_recordchanged(main_current, 0);

      /* If an indent or tag exists, it has been checked to be on the next
      line, so we can remove them. (Indents and tags are always ASCII
      characters.) */

      if (minlen > 0) line_deletech(nextline, 0, minlen + indent2, TRUE);

      if (main_current->len > 0 &&
        main_current->text[main_current->len - 1] != ' ' &&
          nextline->len > 0 && nextline->text[0] != ' ')
            sp = 1;

      main_current = line_concat(nextline, sp);
      }

    /* Do not join the lines; just move on to the next. */

    else
      {
      main_current = nextline;
      }

    detrail(main_current);
    nextline = main_current->next;
    }
  }
}

/* End of eline.c */
