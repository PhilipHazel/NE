/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */


/* This file contains functions for configuring keystrokes - the execution
action of the KEY command. */


#include "ehdr.h"
#include "cmdhdr.h"
#include "keyhdr.h"



/*************************************************
*                Error handling                  *
*************************************************/

/* There may be different numbers of arguments, so we wrap this extra function
round the call to error_moan().

Arguments:
  offset     offset in the key-setting string
  format...  customized error string

Returns:     nothing
*/

static void
keymoan(size_t offset, uschar *format, ...)
{
uschar buff[256];
va_list ap;
va_start(ap, format);
vsprintf(CS buff, CS format, ap);
error_moan(14, offset, buff);
va_end(ap);
}



/*************************************************
*              Get key identity                  *
*************************************************/

/*
Arguments:
  string    entire setting string
  p         current position in string
  ak        where to put the key identity, -ve if error given

Returns:    updated value of p
*/

static uschar *
getkey(uschar *string, uschar *p, int *ak)
{
uschar *e1 = US"   Key name or number expected";
uschar *e2 = US"   \"%s%s\" cannot be independently configured in this "
               "version of NE%s";
*ak = -1;    /* default bad */

mac_skipspaces(p);

if (*p == 0)
  {
  keymoan(p - string, e1);
  return p;
  }

/* Deal with a number, indicating a function key */

if (isdigit(*p))
  {
  int chcode = 0;
  while (*p != 0 && isdigit(*p)) chcode = chcode*10 + *p++ - '0';
  if (1 <= chcode && chcode <= max_fkey)
    {
    if ((key_functionmap & (1L << chcode)) == 0)
      /* LCOV_EXCL_START */
      keymoan(p - string,
        US"   Function key %d not available in this version of NE", chcode);
      /* LCOV_EXCL_STOP */
    else *ak = chcode + s_f_umax;
    }
  else keymoan(p - string,
    US"   Incorrect function key number (not in range 1-%d)", max_fkey);
  }

/* Deal with a single letter key name, indicating ctrl/<something> */

else if (!isalpha(p[1]) && p[1] != '/')
  {
  int chcode = key_codes[*p++];
  if (chcode == 0) keymoan(p - string, e1);
  else if ((key_controlmap & (1L << chcode)) == 0)
    {
    /* LCOV_EXCL_START - currently no exclusions */
    uschar name[8];
    Ustrcpy(name, "ctrl/");
    name[5] = p[-1];
    name[6] = 0;
    keymoan(p - string, e2, US"", name, sys_keyreason(chcode));
    /* LCOV_EXCL_STOP */
    }
  else *ak = chcode;
  }

/* Deal with multi-letter key name, possibly preceded by "s/" or "c/" */

else
  {
  uschar name[20];
  int n = 0;
  int chcode = -1;
  int shiftbits = 0;

  while(p[1] == '/')
    {
    if (*p == 's') shiftbits += s_f_shiftbit; else
      {
      if (*p == 'c') shiftbits += s_f_ctrlbit;
        else keymoan(p - string, US"   s/ or c/ expected");
      }
    p += 2;
    if (*p == 0) break;
    }

  while (isalpha(*p)) name[n++] = *p++;
  name[n] = 0;

  for (n = 0; key_names[n].name[0] != 0; n++)
    if (Ustrcmp(name, key_names[n].name) == 0)
      { chcode = key_names[n].code; break; }

  if (chcode > 0)
    {
    int mask = (int)(1L << ((chcode-s_f_ubase)/4));
    if ((key_specialmap[shiftbits] & mask) == 0)
      {
      uschar *sname = US"";
      switch (shiftbits)
        {
        case s_f_shiftbit: sname = US"s/"; break;
        case s_f_ctrlbit:  sname = US"c/"; break;
        case s_f_shiftbit+s_f_ctrlbit: sname = US"s/c/"; break;
        }
      keymoan(p - string, e2, sname, name, sys_keyreason(chcode+shiftbits));
      }
    else *ak = chcode + shiftbits;
    }
  else keymoan(p - string, US"   %s is not a valid key name", name);
  }

return p;
}



/*************************************************
*                Get key action                  *
*************************************************/

/*
Arguments:
  string       entire setting string
  p            current point in string
  aa           where to return the action, -ve after an error

Returns:       updated value of p
*/

static uschar *
getaction(uschar *string, uschar *p, int *aa)
{
*aa = -1;    /* default bad */

mac_skipspaces(p);

if (*p == 0 || *p == ',')
  {
  *aa = 0;    /* The unset action */
  }

else if (isalpha(*p))
  {
  int n = 0;
  char name[8];
  while (isalpha(*p)) name[n++] = *p++;
  name[n] = 0;

  for (n = 0; n < key_actnamecount; n++)
    if (Ustrcmp(key_actnames[n].name, name) == 0)
      { *aa = key_actnames[n].code; break; }

  if (*aa < 0) keymoan(p - string, US"   Unknown key action");
  }

else if (isdigit(*p))
  {
  int code = 0;
  while (isdigit(*p)) code = code*10 + *p++ - '0';
  if (1 <= code && code <= max_keystring) *aa = code;
    else keymoan(p - string,
      US"   Incorrect function keystring number (not in range 1-%d)",
        max_keystring);
  }

else keymoan(p - string, US"   Key action (letters or a number) expected");

return p;
}



/*************************************************
*     Handle setting string for keystrokes       *
*************************************************/

/* This function is called twice for every KEY command - during decoding to
check the syntax, and during execution to do the job. This is a bit wasteful,
but it is a rare enough operation, and it saves having variable-length argument
lists to commands. Do nothing unless running in screen mode.

Arguments:
  string    key setting string
  goflag    TRUE to obey, FALSE for just syntax check

Returns:    TRUE if OK, FALSE for syntax error
*/

BOOL
key_set(uschar *string, BOOL goflag)
{
uschar *p = string;

if (!main_screenmode) 
  {
  if (main_initialized && !goflag) error_moan(67);
  return TRUE;
  } 

while (*p)
  {
  int a, k;
  p = getkey(string, p, &k);
  if (k < 0) return FALSE;
  mac_skipspaces(p);
  if (*p != '=' && *p != ':')
    {
    keymoan(p - string, US"   Equals sign or colon expected");
    return FALSE;
    }
  p = getaction(string, ++p, &a);
  if (a < 0) return FALSE;
  mac_skipspaces(p);
  if (*p != 0 && *p != ',')
    {
    keymoan(p - string, US"   Comma expected");
    return FALSE;
    }
  if (*p != 0) p++;

  /* All well; if executing, commit this setting. */

  if (goflag) key_table[k] = a;
  }

return TRUE;
}


/* End of ekeysub.c */
