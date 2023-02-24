/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: January 2023 */


/* This file contains code for matching a search expression. The global
variables match_leftpos and match_rightpos must be set to the byte offsets
within which to search. */


#include "ehdr.h"



/*************************************************
*               Check 'word'                     *
*************************************************/

/* This is called after a match has been found if the W qualifier is present.
If there are characters within the search window before or after the match,
they must not be "word" characters.

Arguments:
  p           offset of match start
  len         length of matched string
  t           pointer to line
  wleft       offset of window left
  wright      offset of window right

Returns:      TRUE if the matched substring is a "word"
*/

static BOOL
chkword(int p, int len, uschar *t, int wleft, int wright)
{
int n = p + len;
if (p > wleft && (ch_tab[t[p-1]] & ch_word) != 0) return FALSE;
if (n < wright && (ch_tab[t[n]] & ch_word) != 0) return FALSE;
return TRUE;
}



/*************************************************
*           Match byte string to line            *
*************************************************/

/* This checks a fixed byte string at a given offset in a line.

Arguments:
  s           string to match
  len         number of bytes
  t           point in line to compare
  U           TRUE for caseless matching

Returns:      TRUE if matched
*/

static BOOL
matchbytes(uschar *s, int len, uschar *t, BOOL U)
{
if (!U) return memcmp(s, t, len) == 0;  /* Caseful match */

/* Caseless matching is supported only for ASCII (single-byte) characters, so
UTF-8 mode must be handled specially. */

if (allow_wide)
  {
  for (int i = 0; i < len; i++)
    {
    if (s[i] > 127 || t[i] > 127)
      {
      if (s[i] != t[i]) return FALSE;
      }
    else
      {
      if (toupper(s[i]) != toupper(t[i])) return FALSE;
      }
    }
  }

/* Not UTF-8 */

else for (int i = 0; i < len; i++)
  if (toupper(s[i]) != toupper(t[i])) return FALSE;

return TRUE;
}



/*************************************************
*        Match qualified string to line          *
*************************************************/

/* Called only from within matchse() (match search expression) below.

Arguments:
  qs         qualified string
  line       line to match against
  USW        US and W flags imposed externally (casematch or se flags)

Returns:     MATCH_OK or MATCH_FAILED
*/

static int
matchqs(qsstr *qs, linestr *line, usint USW)
{
uschar *t = line->text;
uschar *s = qs->text + 1;      /* first byte is the string delimiter */

usint p;                       /* match point */
usint count = qs->count;
usint flags = qs->flags;
usint len = qs->length;
usint leftpos = match_leftpos;                     /* lhs byte in line */
usint rightpos = match_rightpos;                   /* rhs byte in line */
usint wleft = line_offset(line, qs->windowleft);   /* lhs window byte offset */
usint wright = line_offset(line, qs->windowright); /* rhs window byte offset */
usint *map = qs->map;

BOOL yield = MATCH_FAILED;                         /* default result */
BOOL W = ((flags | USW) & qsef_W) != 0;            /* wordsearch state */

/* Caseless matching occurs if this qualified string has the U qualifier, or if
a containing search expression has the U qualfier and this qualified string
does not have the V qualifier. (U and V are incompatible, so no item can have
both set.) */

BOOL U = (flags & qsef_U) != 0 ||
  ((USW & qsef_U) != 0 && (flags & qsef_V) == 0);  /* caseless state */

/* If hex string, change the pointer to the interpreted string and halve the
length. */

if ((flags & qsef_X) != 0)
  {
  s = qs->hexed;
  len /= 2;
  }

/* An empty line may be represented as a NULL pointer and zero length. */

if (t == NULL) t = (uschar *)"";

/* Take note of line length & significant space qualifier */

if (wright > line->len) wright = line->len;
if (((flags | USW) & qsef_S) != 0)
  {
  while (wleft < wright && t[wleft] == ' ') wleft++;
  while (wleft < wright && t[wright-1] == ' ') wright--;
  }

/* Take note of window qualifier */

if (leftpos < wleft) leftpos = wleft;
if (rightpos > wright) rightpos = wright;
if (rightpos < leftpos) rightpos = leftpos;

/* In most cases, search starts at left */

p = leftpos;

/* If the line is long enough, match according to the flags. Otherwise, the
default MATCH_FAILED applies. */

if ((leftpos + len <= rightpos))
  {
  /* Deal with B and P (P = B + E); if H is on it must be PH because H is not
  allowed with either B or E on its own. */

  if ((flags & qsef_B) != 0)
    {
    if (p == wleft || (flags & qsef_H) != 0)  /* must be at line start or H */
      {
      /* Deal with P */

      if ((flags & qsef_E) != 0)
        {
        if (len == rightpos - p && matchbytes(s, len, t+p, U)) yield = MATCH_OK;
        }

      /* Deal with B on its own */

      else if (matchbytes(s, len, t+p, U) &&
        (!W || chkword(p, len, t, p, wright))) yield = MATCH_OK;
      }
    }

  /* Deal with E on its own */

  else if ((flags & qsef_E) != 0)
    {
    if (rightpos == wright)   /* must be at line end */
      {
      p = rightpos - len;
      if (matchbytes(s, len, t+p, U) &&
        (!W || chkword(p, len, t, wleft, wright))) yield = MATCH_OK;
      }
    }

  /* Deal with H */

  else if ((flags & qsef_H) != 0)
    {
    if (matchbytes(s, len, t+p, U) &&
      (!W || chkword(p, len, t, wleft, wright))) yield = MATCH_OK;
    }

  /* Deal with L (last); if the line byte at the first position is not in the
  string, skip backwards the whole length. The global match_L is set when
  searching backwards. */

  else if (match_L || (flags & qsef_L) != 0)
    {
    p = rightpos - len;
    if (len == 0) yield = MATCH_OK; else for(;;)
      {
      int c = t[p];
      if ((map[c/intbits] & (1 << (c%intbits))) == 0)
        {
        if (p >= len) p -= len; else break;
        }
      else
        {
        if (matchbytes(s, len, t+p, U) &&
            (!W || chkword(p, len, t, wleft, wright)))
          {
          if (--count == 0)  { yield = MATCH_OK; break; }
          if (p >= len) p -= len; else break;
          }
        else
          {
          if (p > leftpos) p--; else break;
          }
        }
      }
    }

  /* Else it's a forward search; if the line byte at the last position is not
  in the string, skip forward by the whole length. */

  else
    {
    if (len == 0) yield = MATCH_OK; else while (p + len <= rightpos)
      {
      int c = t[p+len-1];
      if ((map[c/intbits] & (1u << (c%intbits))) == 0) p += len; else
        {
        if (matchbytes(s, len, t+p, U) &&
          (!W || chkword(p, len, t, wleft, wright)))
            {
            if (--count <= 0) { yield = MATCH_OK; break; }
            p += len;
            }
        else p++;
        }
      }
    }

  /* If successful, set start & end */

  if (yield == MATCH_OK)
    {
    match_start = p;
    match_end = p + len;
    }
  }

/* If N is present, reverse the result */

if ((flags & qsef_N) != 0)
  {
  if (yield == MATCH_FAILED)
    {
    match_start = 0;    /* give whole line */
    match_end = line->len;
    yield = MATCH_OK;
    }
  else yield = MATCH_FAILED;
  }

return yield;
}



/*************************************************
*        Match a search expression to a line     *
*************************************************/

/* If successful, the global variables match_start and match_end contain the
start and end of the matched string (or whole line). The left and right margins
for the search are set in globals to avoid too many arguments. There is also
the global flag match_L, set during backwards find operations.

A recursively called subroutine is used to handle AND and OR branches, passing
down externally imposed USW flags.

Arguments:
  se         search expression
  line       line to search
  USW        external flags

Returns:     MATCH_OK, MATCH_FAILED or MATCH_ERROR
*/

static int
matchse(sestr *se, linestr *line, usint USW)
{
int yield;

/* Update U, S and W flags -- V turns U off */

if ((se->flags & qsef_U) != 0) USW |= qsef_U;
if ((se->flags & qsef_V) != 0) USW &= ~qsef_U;
if ((se->flags & qsef_S) != 0) USW |= qsef_S;
if ((se->flags & qsef_W) != 0) USW |= qsef_W;

/* Test for qualified string */

if (se->type == cb_qstype)
  return ((se->flags & qsef_R) != 0)?
    cmd_matchqsR((qsstr *)se, line, USW) : matchqs((qsstr *)se, line, USW);

/* Got a search expression node - yield is a line */

yield = matchse(se->left.se, line, USW);
if (yield == MATCH_ERROR) return yield;

/* Deal with OR */

if ((se->flags & qsef_AND) == 0)
  {
  if (yield == MATCH_FAILED && se->right.se != NULL)
    yield = matchse(se->right.se, line, USW);
  }

/* Deal with AND */

else if (yield == MATCH_OK) yield = matchse(se->right.se, line, USW);

if (yield == MATCH_ERROR) return yield;

/* Invert yield if N set */

if ((se->flags & qsef_N) != 0)
  yield = (yield == MATCH_OK)? MATCH_FAILED : MATCH_OK;

/* If matched, return whole line */

if (yield == MATCH_OK)
  {
  match_start = 0;
  match_end = line->len;
  }

return yield;
}


/*************************************************
This is the externally called funtion, which starts the match with the default 
caseless state in the USW parameter.

Arguments:
  se         search expression
  line       line to search

Returns:     MATCH_OK, MATCH_FAILED or MATCH_ERROR
*/

int
cmd_matchse(sestr *se, linestr *line)
{
return matchse(se, line, cmd_casematch? 0 : qsef_U);
}

/* End of ematch.c */
