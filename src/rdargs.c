/*************************************************
*    Command Line Decoding Routines (rdargs)     *
*************************************************/

/* Copyright (c) University of Cambridge 1990 - 2023 */
/* Created by Philip Hazel, November 1989 */
/* Last major modification: January 1991 */

/* July 1993: Allow an unkeyed item consisting solely of "-" */
/* Feb  1994: Clear out data.text as well as data.number; this matters when
              these two union fields are not actually the same length. */
/* Feb  2004: Convert to unsigned char */
/* Dec  2010: Add casts to avoid warnings for signed/unsigned */
/* Dec  2010: Fixed uninitialized memory bug  */
/* Jan  2016: Removed obsolete LONGINT */
/* Nov  2922: Fix typos in comments */

/* Command line argument decoding. These functions provide a straight-
forward means of decoding a command line that consists of keyword/
value pairs. The style is very much that of the Tripos system. The
externally-visible function is called rdargs(), and it takes the
following arguments:

(1) int argc and (2) char **argv are passed on directly from main();

(3) uschar *keystring is a string of keywords, possibly qualified, in the form

      keyitem[,keyitem]*

    where each keyitem is of the form

      keyname[=keyname][/a][/k][/n|/s][/<d>]

    Keynames separated by '=' are synonyms. The flags are as follows:

    /a    Item must always be present in the command line.
    /k    Item, if present, must be keyed.
    /n    Item expects numeric argument(s).
    /s    Item has no argument; it is a switch.
    /<d>  Item may have up to <d> arguments, where <d> is a sequence of digits.

    A default value can be given for a numerical argument by following /n
    with '=' and a number, e.g. /n=0. The default will be returned if the
    keyword is present without a numeric value following it.

    A typical string might be "from/a,to/k,debug=d/s". Non-keyed items may
    only appear in the command line if all preceding keyed items in the
    keylist have been satisfied.

    A key may be specified as a single question mark. When this is done, any
    keyword specified on the command line is accepted, and its value is the
    particular keyword. This provides a method for dealing with Unix options
    strings starting with a minus and containing an arbitrary collection of
    characters. A question mark must always be the last keyword, since anything
    matches it, and the key string is searched from left to right.

(4) arg_results *results is a pointer to a vector of structures of type
    arg_results. There must be as many elements in the vector as there are
    keyitems in the string, plus extras for any that expect multiple arguments,
    and in any event, there must be a minimum of three, as in the event of
    certain errors, the space is used for constructing error messages and
    returning pointers to them. Each element contains:

    (a) int presence, which, on successful return, contains one of the values

        arg_present_not      item was not present on the command line
        arg_present_keyed    item was present and was keyed
        arg_present_unkeyed  item was present and was not keyed

    (b) union { int number; uschar *text } data, which contains either a number,
        for a numerical or switch argument, or a pointer for a string.

    For non-existent arguments the data.number field is set to zero (i.e. 
    FALSE) for switches, and NULL for strings via data.text. The presence field
    need only be inspected for numerical arguments if it is necessary to
    distinguish between zero supplied explicitly and zero defaulted.

For multiply-valued items, the maximum number of results slots is reserved.
For example, if the keystring is "from/4", then slots 0 to 3 are associated
with the potential four values for the "from" argument. How many there are can
be detected by inspecting the presence values. Unkeyed multiple values cannot
be split by keyed values, e.g. the string "a b -to c d" is not a valid for a
keystring "from/3,to/k". The "d" item will be faulted.

If rdargs is successful, it returns zero. Otherwise it returns non-zero, and
the first two results elements contain pointers in their data.text fields to
two error message strings. If the error involves a particular keyword, the
first string contains that keyword. The yield value is -1 if the error is not
related to a particular input line string; otherwise it contains the index of
the string at fault. */


#include "ehdr.h"

/* Flags for key types */

#define rdargflag_a       1*256
#define rdargflag_k       2*256
#define rdargflag_s       4*256
#define rdargflag_n       8*256
#define rdargflag_q      16*256
#define rdargflag_d      32*256

/* Mask for presence value of control word */

#define argflag_presence_mask   255


/*************************************************
*          Search keystring for i-th keyword     *
*************************************************/

/* Given the index of a key (counting from zero), find the
name of the key. This function is used only for generating
the name in cases of error. */

static uschar *findkey(int number, uschar *keys, uschar *word)
{
int ch, i;
int j = 0;
int k = 0;
word[k++] = '-';
for (i = 0; i < number; i++) while (keys[j++] != ',');
while ((ch = keys[j++]) != 0 && ch != ',' && ch != '/' && ch != '=')
  word[k++] = ch;
word[k] = 0;
return word;
}


/*************************************************
*          Search keystring for keyword          *
*************************************************/

/* Given a keyword starting with "-", find its index in
the keystring. Keys are separated by ',' in the string,
and there may be synonyms separated by '='. */

static int findarg(uschar *keys, uschar *s)
{
int matching = TRUE;
int argnum = 0;
int i = 0;
int j = 1;
int ch;

while ((ch = keys[i++]) != 0)
  {
  if (matching)
    {
    if (ch == '?') return argnum;
    if ((ch == '=' || ch == '/' || ch == ',') &&
        s[j] == 0) return argnum;
    if (ch != s[j++]) matching = FALSE;
    }

  if (ch == ',' || ch == '=')
    {
    matching = TRUE;
    j = 1;
    if (ch == ',') argnum++;
    }
  }

if (matching && s[j] == 0) return argnum;
return -1;
}


/*************************************************
*             Set up for error return            *
*************************************************/

/* Copy the argument name and error message into the first
two return string slots. */

static void arg_error(arg_result *results, uschar *arg, uschar *message)
{
results[0].data.text = arg;
results[1].data.text = message;
}


/*************************************************
*           Set up zero or more values           *
*************************************************/

/* Zero values are allowed only if the defaulted flag is set */

static int arg_setup_values(int argc, char **argv, int *a_argindex,
  int *a_argnum, int argflags, arg_result *results, uschar *arg,
    int present_value)
{
int argnum = *a_argnum;
int argindex = *a_argindex;
int argcount = (argflags & 0x00FF0000) >> 16;   /* max args */
if (argcount == 0) argcount = 1;

/* Loop to deal with the arguments */

for (;;)
  {
  uschar *nextstring;
  results[argnum].presence = present_value;

  /* Deal with a numerical or string value -- but if the first time for
  a defaulted item, skip so as to let the optional test work. */

  if ((argflags & rdargflag_d) != 0)
    {
    argflags &= ~rdargflag_d;
    argcount++;   /* Go round the loop once more */
    }
  else if ((argflags & rdargflag_n) != 0)
    {
    char *endptr;
    results[argnum++].data.number =
      (int)strtol(argv[argindex++], &endptr, 0);
    if (*endptr)
      return arg_error(results, arg, US"requires a numerical argument"),
        argindex;
    }
  else results[argnum++].data.text = US argv[argindex++];

  /* If there are no more arguments on the line, or if we have read
  the maximum number for this keyword, break out of the loop. */

  if (argindex >= argc || (--argcount) < 1) break;

  /* Examine the next item on the line. If a numerical argument is
  expected and it begins with a digit or a minus sign, followed by
  a digit, accept it. Otherwise accept it as a string unless it
  begins with a minus sign. */

  nextstring = (uschar *)(argv[argindex]);
  if ((argflags & rdargflag_n) != 0)
    {
    if (!isdigit(nextstring[0]) &&
      (nextstring[0] != '-' || !isdigit(nextstring[1]))) break;
    }
  else if (nextstring[0] == '-') break;
  }

*a_argnum = argnum;
*a_argindex = argindex;
return 0;
}


/*************************************************
*              Decode argument line              *
*************************************************/

/* This is the function that is visible to the outside world. */

int 
rdargs(int argc, char **argv, uschar *keystring, arg_result *results)
{
int keyoffset[30];
int argmax = 0;
int argindex = 1;
int argcount = 1;
int keymax = 0;
int i, ch;

/* We first scan the key string and create, in the presence field,
flags indicating which kind of key it is. The flags are disjoint
from the presence flags, which occupy the bottom byte. The
assumption is that there is at least one key! */

results[0].presence = arg_present_not;
keyoffset[0] = 0;

i = -1;
while ((ch = keystring[++i]) != 0)
  {
  if (ch == '?') results[argmax].presence |= rdargflag_q;

  else if (ch == '/') switch(keystring[++i])
    {
    case 'a': results[argmax].presence |= rdargflag_a; break;
    case 'k': results[argmax].presence |= rdargflag_k; break;
    case 's': results[argmax].presence |= rdargflag_s; break;

    case 'n':
      {
      results[argmax].presence |= rdargflag_n;
      if (keystring[i+1] == '=')
        {
        int n;
        results[argmax].presence |= rdargflag_d;  /* flag default exists */
        if (sscanf(CS keystring+i+2, "%d%n", &(results[argmax].data.number), &n) == 0)
          return arg_error(results, findkey(keymax, keystring,
            (uschar *)(results+2)), US"is followed by an unknown option"), -1;
        i += n + 1;
        }
      break;
      }

    default:
    if (isdigit(keystring[i]))
      {
      argcount = keystring[i] - '0';
      while (isdigit(keystring[i+1]))
        {
        argcount = (argcount * 10) + keystring[++i] - '0';
        }
      if (argcount == 0) argcount = 1;
      results[argmax].presence |= argcount << 16;
      }
    else return arg_error(results, findkey(keymax, keystring,
      (uschar *)(results+2)), US"is followed by an unknown option"), -1;
    }

  else if (ch == ',')
    {
    int j;
    for (j = 1; j < argcount; j++)
      results[++argmax].presence = argflag_presence_mask;
    results[++argmax].presence = arg_present_not;
    keyoffset[++keymax] = argmax;
    argcount = 1;
    }
  }

/* Check that no keyword has been specified with incompatible qualifiers */

for (i = 0; i <= argmax; i++)
  {
  int argflags = results[i].presence;
  int keynumber = -1;
  int j;
  
  for (j = 0; j < keymax; j++)
    if (keyoffset[j] == i) { keynumber = j; break; }
    
  /* The keynumber won't be found for extra arguments. */
    
  if (keynumber >= 0)
    { 
    if ((argflags & (rdargflag_s + rdargflag_n)) == rdargflag_s + rdargflag_n)
      return arg_error(results, findkey(keynumber, keystring, (uschar *)(results+2)),
        US"is defined both as a switch and as a key for a numerical value"), -1;
    
    if ((argflags & rdargflag_s) && (argflags & 0x00FF0000) > 0x00010000)
      return arg_error(results, findkey(keynumber, keystring, (uschar *)(results+2)),
        US"is defined as a switch with multiple arguments"), -1;
    }     
  }

/* Loop checking items from the command line and assigning them
to the appropriate arguments. */

while (argindex < argc)
  {
  uschar *arg = US argv[argindex];

  /* Key: find which and get its argument, if any */

  if (arg[0] == '-' && arg[1] != 0)
    {
    int argnum = findarg(keystring, arg);
    int argflags;

    /* Check for unrecognized key */

    if (argnum < 0) return arg_error(results, arg, US"unknown"), argindex;

    /* Adjust argnum for previous keys with multiple values */

    argnum = keyoffset[argnum];

    /* Extract key type flags and and advance to point to the
    argument value(s). */

    argflags = results[argnum].presence;
    if ((argflags & rdargflag_q) == 0) argindex++;

    /* Check for multiple occurrences */

    if ((argflags & argflag_presence_mask) != 0)
      return arg_error(results, arg, US"keyword specified twice"), argindex;

    /* If a switch, set value TRUE, otherwise check that at least one
    argument value is present if required, and then call the function which
    sets up values, as specified. */

    if ((argflags & rdargflag_s) != 0)
      {
      results[argnum].presence = arg_present_keyed;
      results[argnum].data.number = TRUE;
      }
    else
      {
      int rc;
      if (argindex >= argc && (argflags & rdargflag_d) == 0)
        return arg_error(results, arg, US"requires an argument value"), argindex;
      rc = arg_setup_values(argc, argv, &argindex, &argnum, argflags,
        results, arg, arg_present_keyed);
      if (rc) return rc;    /* Error occurred */
      }
    }

  /* Non-key: scan flags for the first unused normal or mandatory
  key which precedes any keyed items other than switches. This
  means that unkeyed items can only appear after any preceding
  keyed items have been explicitly specified. */

  else
    {
    int rc;
    int argflags = 0;    /* keep compiler happy */
    int argnum = -1;

    /* Scan argument list */

    for (i = 0; i <= argmax; i++)
      {
      argflags = results[i].presence;

      /* If unused keyed item, break (error), else if not key, it's usable */

      if ((argflags & argflag_presence_mask) == arg_present_not)
        {
        if ((argflags & rdargflag_k) != 0) break;
        if ((argflags & rdargflag_s) == 0) { argnum = i; break; }
        }
      }

    /* Check for usable item found */

    if (argnum < 0)
      return arg_error(results, arg, US"requires a keyword"), argindex;

    /* Set presence bit and call subroutine to set up argument value(s)
    as specified. */

    rc = arg_setup_values(argc, argv, &argindex, &argnum, argflags,
      results, arg, arg_present_unkeyed);
    if (rc) return rc;    /* Error detected */
    }
  }

/* End of string reached: check for missing mandatory args. Other
missing args have their values and the presence word set zero. */

for (i = 0; i <= argmax; i++)
  {
  int argflags = results[i].presence;
  int p = argflags & argflag_presence_mask;
  if (p == arg_present_not || p == argflag_presence_mask)
    {
    if ((argflags & rdargflag_a) == 0)
      {
      results[i].data.number = 0;
      results[i].data.text = NULL;
      results[i].presence = 0;
      }
    else
      {
      int keynumber = 0;
      int j;
      for (j = 0; j < 30; j++)
        if (keyoffset[j] == i) { keynumber = j; break; }
      arg_error(results, findkey(keynumber, keystring, (uschar *)(results+2)),
        US"is a mandatory keyword which is always required");
      return -1;
      }
    }
  }

/* Indicate successful decoding */

return 0;
}

/* End of rdargs.c */
