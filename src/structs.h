/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2023 */

/* This file contains all the structure definitions, together with parameters
that control the size of some of them. */

/* Result from rdargs routine */

typedef struct {
  int presence;
  union {
    int number;
    uschar *text;
  } data;
} arg_result;

enum { arg_present_not, arg_present_unkeyed, arg_present_keyed };


/* Line */

typedef struct line {
  struct line *next;         /* chain to next */
  struct line *prev;         /* chain to previous */
  uschar      *text;         /* the characters themselves */
  int          key;          /* line number */
  usint        len;          /* number of bytes */
  uschar       flags;        /* various flag bits */
} linestr;

/* Bits in line flags byte */

#define lf_eof     1         /* eof line */
#define lf_shn     2         /* show needed */
#define lf_clend   4         /* clear out end of line */
#define lf_tabs    8         /* expanded tabs in this line */
#define lf_udch   16         /* chars for undelete */
#define lf_shbits (lf_shn|lf_clend)  /* show request bits */


/* Entry in "back" vector */

typedef struct {
  linestr *line;
  int col;
} backstr;


/* Buffer */

typedef struct buffer {
  struct buffer *next;       /* next buffer block */

  linestr *bottom;           /* last line in buffer */
  linestr *current;          /* current line in buffer */
  linestr *markline;         /* the line with the mark */
  linestr *markline_global;  /* the global limit line */
  linestr *scrntop;          /* saving top line on screen */
  linestr *top;              /* first line in buffer */

  backstr *backlist;         /* vector of saved positions */
  size_t binoffset;          /* offset for reading file in binary */
   
  usint backtop;             /* top of list */
  usint backnext;            /* position in list */
  int bufferno;              /* buffer number */
  int col;                   /* cursor column */
  int commanding;            /* count of active cbuffer commands */
  int imax;                  /* max input line number */
  int imin;                  /* max inserted line (-ve) number */
  int linecount;             /* number of lines in the buffer */
  int markcol;               /* the mark column */
  int markcol_global;        /* the global limit column */
  int marktype;              /* the non-global mark's type */
  int offset;                /* the cursor offset */
  int row;                   /* cursor row */
  int rmargin;               /* right margin */

  uschar *filealias;         /* name to display */
  uschar *filename;          /* real name */

  CBOOL changed;             /* buffer edited */
  CBOOL noprompt;            /* no prompting wanted */
  CBOOL readonly;            /* readonly flag */
  CBOOL saved;               /* saved to "own" file */
} bufferstr;



/* === Control blocks for command processing === */

/* The first byte of all these blocks is called "type" and identifies the kind
of block. The various types are: */

#define  cb_setype  1        /* search expression */
#define  cb_qstype  2        /* qualified string */
#define  cb_sttype  3        /* plain string */
#define  cb_cmtype  4        /* command block */
#define  cb_iftype  5        /* 'if' 2nd arg block */
#define  cb_prtype  6        /* procedure */

/* Here is a generic structure for addressing the type field */

typedef struct {
  uschar type;
} cmdblock;


/* Qualified string */

typedef struct qsstr {
  uschar type;                 /* cb_qstype */
  uschar count;                /* count qualifier */
  short int flags;             /* see below */
  short int windowleft;        /* window values */
  short int windowright;
  short int length;            /* length of original string */
  #ifdef USE_PCRE1
  pcre *cre;                   /* pointer to compiled regex */
  #else
  pcre2_code *cre;             /* pointer to compiled regex */
  #endif
  uschar *hexed;               /* hexed chars for non-R */
  uschar *text;                /* data chars */
  usint map[qsmapsize];        /* bit map for contained chars */
} qsstr;

/*  Search Expression; its left/right pointers can point to either
another search expression or a qualified string. Hence the messing
about with the union to achieve this. */

struct sestr;

typedef union {
  struct qsstr *qs;
  struct sestr *se;
} qseptr;

typedef struct sestr {
  uschar type;               /* cb_setype */
  uschar count;              /* count qualifier */
  unsigned short int flags;  /* see below */
  short int windowleft;      /* window values */
  short int windowright;
  qseptr left;               /* left subtree */
  qseptr right;              /* right subtree */
} sestr;


/* Default window values */

#define qse_defaultwindowleft   0
#define qse_defaultwindowright  0x7fff

/* Qualifier flags; 16-bit field */

#define qsef_B      0x0001u
#define qsef_E      0x0002u
#define qsef_H      0x0004u
#define qsef_L      0x0008u
#define qsef_N      0x0010u
#define qsef_R      0x0020u
#define qsef_S      0x0040u
#define qsef_U      0x0080u
#define qsef_V      0x0100u
#define qsef_W      0x0200u
#define qsef_X      0x0400u
#define qsef_AND    0x0800u  /* AND flag for se nodes */
#define qsef_REV    0x1000u  /* Reversed flag for regular expressions */
#define qsef_FV     0x2000u  /* Regex compiled verbatim by USW */

/* Both of E and B */
#define qsef_EB     (qsef_E + qsef_B)

/* Not allowed in search expressions */

#define qsef_NotSe (qsef_REV+qsef_AND+qsef_X+qsef_R+qsef_L+qsef_H+qsef_E+qsef_B)

/* Plain String */

typedef struct {
  uschar type;
  uschar delim;
  uschar hexed;
  uschar *text;
} stringstr;

/* Command structures and arguments are mutually recursive */

struct cmd;

/* Structure for if and unless */

typedef struct {
  uschar type; /* cb_iftype */
  struct cmd *if_then;
  struct cmd *if_else;
} ifstr;

/* Union type for command arguments */

typedef union {
  void *block;
  stringstr *string;
  qsstr *qs;
  sestr *se;
  int value;
  struct cmd *cmds;
  ifstr *ifelse;
} cmdarg;

/* Command block */

typedef struct cmd {
  uschar type;
  uschar id;                /* identity of command */
  uschar flags;             /* argument flags */
  uschar misc;              /* switch for miscellaneous cmd options */
  uschar ptype1;            /* prompt type for arg1 */
  uschar ptype2;            /* prompt type for arg2 */
  uschar arg1type;          /* type of arg1 */
  uschar arg2type;          /* type of arg2 */
  struct cmd *next;         /* next cmd on chain */
  usint  count;             /* repeat count */
  cmdarg arg1;              /* 1st argument */
  cmdarg arg2;              /* 2nd argument */
} cmdstr;


/* Argument flag values */

#define  cmdf_arg1   1  /* arg1 present */
#define  cmdf_arg2   2  /* arg2 present */
#define  cmdf_arg1F  4  /* arg1 is ptr to control block */
#define  cmdf_arg2F  8  /* arg2 is ptr to control block */
#define  cmdf_group 16  /* this is cmd group */


/* Procedure structure */

typedef struct procstr {
  uschar   type;
  uschar   flags;
  uschar   *name;
  cmdstr *body;
  struct procstr *next;
} procstr;

#define pr_active        1


/* Layout of key names table entries */

typedef struct {
  uschar *name;
  int   code;
} keynamestr;

/* Structure for remembering the names of written files */

typedef struct filewritten {
  struct filewritten *next;
  uschar *name;
} filewritstr;


/* End of structs.h */
