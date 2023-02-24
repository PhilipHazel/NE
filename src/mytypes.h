/***********************************************************
*             The E text editor - 2rd incarnation          *
***********************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */

/* See the file NOTICE for conditions of use and distribution. */

/* This header file contains type definitions and related macros. */

#define FALSE         0
#define TRUE          1
#define TRUE_UNSET    2


/* If gcc is being used to compile NE, we can use its facility for checking the
arguments of printf-like functions. This is done by a macro. */

#ifdef __GNUC__
#define PRINTF_FUNCTION   __attribute__((format(printf,1,2)))
#define FPRINTF_FUNCTION  __attribute__((format(printf,2,3)))
#else
#define PRINTF_FUNCTION
#define FPRINTF_FUNCTION
#endif


/* Some operating systems (naughtily, imo) include a definition for "uchar" in
the standard header files, so we use "uschar" (and then "usint" for
consistency). Solaris has u_char in sys/types.h. This is just a typing
convenience, of course. */

typedef int BOOL;
typedef unsigned char CBOOL;
typedef unsigned char uschar;
typedef unsigned int  usint;


/* These macros save typing for the casting that is needed to cope with the
mess that is "char" in ISO/ANSI C. Having now been bitten enough times by
systems where "char" is actually signed, I've converted NE to use entirely
unsigned chars, except in a few special places such as arguments that are
almost always literal strings. */

#define CS   (char *)
#define CCS  (const char *)
#define CSS  (char **)
#define US   (unsigned char *)
#define CUS  (const unsigned char *)
#define USS  (unsigned char **)

/* The C library string functions expect "char *" arguments. Use macros to
avoid having to write a cast each time. We do this for string and file
functions that are called quite often; for other calls to external libraries
(which are on the whole special-purpose) we just use individual casts. */

#define Uatoi(s)           atoi(CCS(s))
#define Uatol(s)           atol(CCS(s))
#define Uchdir(s)          chdir(CCS(s))
#define Uchmod(s,n)        chmod(CCS(s),n)
#define Uchown(s,n,m)      chown(CCS(s),n,m)
#define Ufgets(b,n,f)      fgets(CS(b),n,f)
#define Ufopen(s,t)        fopen(CCS(s),CCS(t))
#define Ulink(s,t)         link(CCS(s),CCS(t))
#define Ulstat(s,t)        lstat(CCS(s),t)

#ifdef O_BINARY                                        /* This is for Cygwin,  */
#define Uopen(s,n,m)       open(CCS(s),(n)|O_BINARY,m) /* where all files must */
#else                                                  /* be opened as binary  */
#define Uopen(s,n,m)       open(CCS(s),n,m)            /* to avoid problems    */
#endif                                                 /* with CRLF endings.   */
#define Uread(f,b,l)       read(f,CS(b),l)
#define Urename(s,t)       rename(CCS(s),CCS(t))
#define Ustat(s,t)         stat(CCS(s),t)
#define Ustrcat(s,t)       strcat(CS(s),CCS(t))
#define Ustrchr(s,n)       US strchr(CCS(s),n)
#define CUstrchr(s,n)      CUS strchr(CCS(s),n)
#define CUstrerror(n)      CUS strerror(n)
#define Ustrcmp(s,t)       strcmp(CCS(s),CCS(t))
#define Ustrcpy(s,t)       strcpy(CS(s),CCS(t))
#define Ustrcspn(s,t)      strcspn(CCS(s),CCS(t))
#define Ustrftime(s,m,f,t) strftime(CS(s),m,f,t)
#define Ustrlen(s)         (usint)strlen(CCS(s))
#define Ustrncat(s,t,n)    strncat(CS(s),CCS(t),n)
#define Ustrncmp(s,t,n)    strncmp(CCS(s),CCS(t),n)
#define Ustrncpy(s,t,n)    strncpy(CS(s),CCS(t),n)
#define Ustrpbrk(s,t)      strpbrk(CCS(s),CCS(t))
#define Ustrrchr(s,n)      US strrchr(CCS(s),n)
#define CUstrrchr(s,n)     CUS strrchr(CCS(s),n)
#define Ustrspn(s,t)       strspn(CCS(s),CCS(t))
#define Ustrstr(s,t)       US strstr(CCS(s),CCS(t))
#define CUstrstr(s,t)      CUS strstr(CCS(s),CCS(t))
#define Ustrtod(s,t)       strtod(CCS(s),CSS(t))
#define Ustrtol(s,t,b)     strtol(CCS(s),CSS(t),b)
#define Ustrtoul(s,t,b)    strtoul(CCS(s),CSS(t),b)
#define Uunlink(s)         unlink(CCS(s))

/* End of mytypes.h */
