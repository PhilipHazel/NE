/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: January 2023 */


/* This file contains code for reading search expressions and qualified
strings. */


#include "ehdr.h"

/* This table is global because it is used when initializing the ch_tab. */

uschar *cmd_qualletters = US"pbehlnrsuvwx";

/* Table of flag bits corresponding to the above letters. */

static int qualbits[] = {
  qsef_B + qsef_E,   /* P is shorthand for B+E */
  qsef_B, qsef_E, qsef_H, qsef_L, qsef_N, qsef_R,
  qsef_S, qsef_U, qsef_V, qsef_W, qsef_X };

/* Table of flag bits that are not allowed with each qualifier. */

static int qualXbits[] = {
  /* p */ qsef_B + qsef_E + qsef_L,
  /* b */ qsef_B + qsef_E + qsef_L + qsef_H,
  /* e */ qsef_B + qsef_E + qsef_L + qsef_H,
  /* h */ qsef_B + qsef_E + qsef_L + qsef_H + qsef_S,
  /* l */ qsef_B + qsef_E + qsef_L + qsef_H,
  /* n */ 0,
  /* r */ 0,
  /* s */ 0,
  /* u */ qsef_V,
  /* v */ qsef_U,
  /* w */ 0,
  /* x */ 0 };



/*************************************************
*      Pack a (non-R) qualified hex string       *
*************************************************/

/* Retain the original string (for messages), and construct the packed form in
another piece of memory.

Argument:  qualified string
Returns:   TRUE if OK, FALSE for invalid hex string
*/

static BOOL
hexqs(qsstr *qs)
{
int len = qs->length;
uschar *s = qs->text + 1;  /* First byte is the delimiter */
uschar *hex;

if ((len%2) != 0)
  {
  error_moan(18, "character count is odd");
  return FALSE;
  }

qs->hexed = hex = store_Xget(len/2);

for (int i = 0; i < len; i += 2)
  {
  int x = 0;
  for (int j = 0; j <= 1; j++)
    {
    int ch = toupper(*s++);
    x = x << 4;
    if ((ch_tab[ch] & ch_hexch) != 0)
      {
      if (isalpha(ch)) x += ch - 'A' + 10; else x += ch - '0';
      }
    else
      {
      error_moan(19, j, "not a hex digit");
      return FALSE;
      }
    }
  *hex++ = x;
  }

return TRUE;
}



/*************************************************
*      Read qstring with supplied qualifiers     *
*************************************************/

/* This function can never fail for ordinary strings, as we are already
pointing at a valid delimiter character on entry. However, it can fail to
compile a regular expression or to construct the packed form of a hex string.
The last argument is a flag indicating whether it is a replacement string that
is being read. In this case, the R qualifier does not cause compilation.

Arguments:
  a_qs       where to return the qualified string
  count      value of "count" qualifier
  flags      qualifier flag bits
  wleft      ) edges set by the [n] qualifier
  wright     )
  rflag      TRUE for a replacement string, FALSE for a search string

Returns:     TRUE on success, FALSE after any error
*/

static BOOL
readsq(qsstr **a_qs, int count, int flags, int wleft, int wright, int rflag)
{
qsstr *qs;
size_t n;
uschar *p = cmd_ptr;     /* Remember starting delimiter */
int dch = *cmd_ptr++;    /* Pick up delimiter character */

while (*cmd_ptr != 0 && *cmd_ptr != dch) cmd_ptr++;    /* Find end of string */
if (*cmd_ptr == 0) cmd_ist = dch;  /* If eol, set implied string terminator */
n = cmd_ptr - p - 1;               /* Byte length, excluding delimiter */

/* Set up qualified string block */

*a_qs = qs = store_Xget(sizeof(qsstr));
qs->type = cb_qstype;
qs->count = count;
qs->windowleft = wleft;
qs->windowright = wright;
qs->length = n;
qs->cre = NULL;
qs->hexed = NULL;

/* Copy in the actual string, including the leading delimiter. */

qs->text = store_Xget(n+2);
memcpy(qs->text, p, n + 1);
qs->text[n + 1] = 0;

/* Advance past an actual delimiter and any following spaces */

if (*cmd_ptr != 0) cmd_ptr++;
mac_skipspaces(cmd_ptr);

/* If the string is empty, ignore the R qualifier; if the X (hex) qualifier is
set, force caseful matching. */

if (n == 0) flags &= ~qsef_R;
if ((flags & qsef_X) != 0) flags = (flags & ~qsef_U) | qsef_V;

qs->flags = flags;

/* If the R qualifier is set there is nothing more to do for a replacement
string, but a search string must be compiled. When R is combined with X, the
handling of X is done within the code for R. */

if ((flags & qsef_R) != 0) return rflag? TRUE : cmd_makeCRE(qs);

/* For non-R strings, if the X qualifier is set, interpret the string as a
sequence of hex pairs to make the actual string that is used. */

if ((flags & qsef_X) != 0)
  {
  if (!hexqs(qs)) return FALSE;
  p = qs->hexed;
  n /= 2;
  }
else p = qs->text + 1;

/* If this is not a replacement string, set up a bit map indicating which bytes
are present in the string. This speeds up searching no end (see algorithm in
matchqs()). If this string does not have the V qualifier, we don't know at this
time whether the match will be cased or not so we have to assume caseless. */

if (!rflag)
  {
  memset((void *)qs->map, 0, sizeof(qs->map));

  /* We know this is caseful */

  if ((flags & qsef_V) != 0)
    {
    while (n-- > 0)
      {
      uschar c = *p++;
      qs->map[c/intbits] |= 1 << (c%intbits);
      }
    }

  /* Might be caseless */

  else
    {
    while (n-- > 0)
      {
      uschar c = *p++;
      c = toupper(c);
      qs->map[c/intbits] |= 1 << (c%intbits);
      c = tolower(c);
      qs->map[c/intbits] |= 1 << (c%intbits);
      }
    }
  }

return TRUE;
}



/*************************************************
*           Read string qualifiers               *
*************************************************/

/* Qualifiers must be terminated by a legal string delimiter character, or '('
when they may be for a search expression.

Arguments:
  a_count    where to return the count qualifier
  a_flags    where to return qualifier flag bits
  a_wleft    ) where to return edge values for
  a_wright   )   the "window" [] qualifier
  seposs     TRUE if these may be for a search expression; FALSE if qs only

Returns:     TRUE if OK, FALSE after an error
*/

static BOOL
readqual(int *a_count, int *a_flags, int *a_wleft, int *a_wright, BOOL seposs)
{
BOOL countread = FALSE;
BOOL windread = FALSE;
int count = 1;
int flags = 0;
int wleft = qse_defaultwindowleft;
int wright = qse_defaultwindowright;

for (;;)
  {
  int ch = tolower(*cmd_ptr);

  /* Handle a valid qualifier letter */

  if ((ch_tab[ch] & ch_qualletter) != 0)
    {
    int p = Ustrchr(cmd_qualletters, ch) - cmd_qualletters;
    int q = qualbits[p];
    int r = qualXbits[p];

    /* Most checking for illegal combinations is done using the tables.
    However, we must check explicitly for H following P because P = B+E and the
    tables forbid PB and PE. */

    if (ch == 'h' && (flags & qsef_EB) == qsef_EB) r &= ~qsef_EB;

    /* Check permitted combinations */

    if ((flags & (q | r)) != 0)
      {
      error_moan(20);
      return FALSE;
      }

    /* Accept this qualifier */

    flags |= q;
    cmd_ptr++;
    }

  /* Not a qualifier letter - try for digit */

  else if ((ch_tab[ch] & ch_digit) != 0)
    {
    if (countread)
      {
      error_moan(20);
      return FALSE;
      }
    count = cmd_readnumber();
    countread = TRUE;
    }

  /* Not a qualifier letter or a digit - try for column qualifier */

  else if (ch == '[')
    {
    if (windread)
      {
      error_moan(20);
      return FALSE;
      }

    windread = TRUE;
    cmd_ptr++;
    wleft = cmd_readnumber();
    wright = wleft;
    if (wleft > 0) wleft--; else wleft = 0;

    ch = *cmd_ptr;
    if (ch == ',')
      {
      cmd_ptr++;
      wright = cmd_readnumber();
      if (wright < 0) wright = qse_defaultwindowright;  /* No number found */
      ch = *cmd_ptr;
      }

    if (ch != ']')
      {
      error_moan(13, "\"]\"");
      return FALSE;
      }

    cmd_ptr++;
    }

  /* Not a recognized qualifier in any form; try for valid end of qualifiers,
  then forbid a count with either B or E. */

  else if ((seposs && ch == '(') || (ch_tab[ch] & ch_delim) != 0)
    {
    if (countread)
      {
      if ((flags & qsef_EB) != 0)
        {
        error_moan(20);
        return FALSE;
        }
      }

    /* Hand back qualifiers via address arguments */

    *a_count = count;
    *a_flags = flags;
    *a_wleft = wleft;
    *a_wright = wright;

    return TRUE;
    }

  /* Neither a valid qualifier, nor a valid terminator */

  else
    {
    error_moan(13, seposs? "String or search expression" : "String");
    return FALSE;
    }

  /* Valid qualifier read, but expect  */

  if (cmd_atend())
    {
    error_moan(13, "String");
    return FALSE;
    }
  }
}



/*************************************************
*           Read a qualified string              *
*************************************************/

/* This function is not used when reading search expressions, because then the
qualifiers may precede an opening bracket. This function is used for the second
arguments of xA, xB, or xE commands, and for the iline command.

Arguments:
  a_qs     where to return the qualified string
  rflag    which qualifiers are allowed (either both R and X or just R)

Returns:   TRUE when all is well
*/

BOOL
cmd_readqualstr(qsstr **a_qs, int rflag)
{
int count, flags, tflags, wleft, wright;
mac_skipspaces(cmd_ptr);
if (!readqual(&count, &flags, &wleft, &wright, FALSE)) return FALSE;
tflags = flags;

switch (rflag)
  {
  case rqs_XRonly: tflags &= ~qsef_R;
  /* Fall through */
  case rqs_Xonly:  tflags &= ~qsef_X;
  }

if (count != 1 || tflags != 0 ||
    wleft != qse_defaultwindowleft || wright != qse_defaultwindowright)
  {
  error_moan(21, (rflag == rqs_Xonly)? "x" : "x or r");
  return FALSE;
  }

return readsq(a_qs, count, flags, wleft, wright, rflag);
}



/*************************************************
*            Read Search Expression              *
*************************************************/

/* Local subroutine to read a group of search expressions connected by '&' and
build them into a tree.

Argument:  where to return the result
Returns:   TRUE on success, FALSE after error
*/

static BOOL
readANDseq(sestr **a_se)
{
sestr *yield;
if (!cmd_readse(&yield)) return FALSE;

while (*cmd_ptr == '&')
  {
  sestr *new = store_Xget(sizeof(sestr));
  new->type = cb_setype;
  new->count = 1;
  new->flags = qsef_AND;
  new->windowleft = qse_defaultwindowleft;
  new->windowright = qse_defaultwindowright;
  new->left.se = yield;
  new->right.se = NULL;
  yield = new;
  cmd_ptr++;
  if (!cmd_readse(&yield->right.se))
    {
    cmd_freeblock((cmdblock *)yield);
    return FALSE;
    }
  }

*a_se = yield;
return TRUE;
}


/* Start of cmd_readse() proper */

/* The yield is TRUE if data was successfully read. The returned control block
may be either a search expression or a qualified string. */

BOOL
cmd_readse(sestr **a_se)
{
sestr *left;
int count, flags, op, wleft, wright;

mac_skipspaces(cmd_ptr);
if (!readqual(&count, &flags, &wleft, &wright, TRUE)) return FALSE;

/* Qualifier read; if the terminator is not "(" return a single qualified
string. */

if (*cmd_ptr != '(')
  return readsq((qsstr **)a_se, count, flags, wleft, wright, FALSE);

/* Search expression expected; check for disallowed qualifiers. */

if ((flags & qsef_NotSe) != 0 || count != 1 ||
    wleft  != qse_defaultwindowleft || wright != qse_defaultwindowright)
  {
  error_moan(22);
  return FALSE;
  }

/* Read sequence connected by '&' (most binding) */

cmd_ptr++;
if (!readANDseq(&left)) return FALSE;

/* Now deal with '|' */

while ((op = *cmd_ptr++) == '|')
  {
  sestr *right;
  if (readANDseq(&right))
    {
    sestr *x = store_Xget(sizeof(sestr));
    x->type = cb_setype;
    x->count = 1;
    x->flags = 0;
    x->windowleft = wleft;
    x->windowright = wright;
    x->left.se = left;
    x->right.se = right;
    left = x;
    }
  else
    {
    cmd_freeblock((cmdblock *)left);
    return FALSE;
    }
  }

/* Check for valid end of search expression */

if (op == ')')
  {
  mac_skipspaces(cmd_ptr);

  /* If we have read a single search expression in brackets, convert it to an
  OR node with a zero right hand address, to ensure that matching yields a
  complete line. */

  if (left->type == cb_qstype)
    {
    sestr *node = store_Xget(sizeof(sestr));
    node->type = cb_setype;
    node->count = 1;
    node->flags = 0;
    node->windowleft = qse_defaultwindowleft;
    node->windowright = qse_defaultwindowright;
    node->left.se = left;
    node->right.se = NULL;
    left = node;
    }

  /* Set qualifiers for the whole search expression */

  left->flags |= flags;
  left->windowleft = wleft;
  left->windowright = wright;

  /* Pass back address and return success */

  *a_se = left;
  return TRUE;
  }

/* Invalid search expression */

else
  {
  cmd_freeblock((cmdblock *)left);
  error_moan(13, "\"&\" or \"|\"");
  return FALSE;
  }
}

/* End of erdseqs.c */
