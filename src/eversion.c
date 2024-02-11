/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: February 2024 */

/* This file contains just one function that sets up the current version and
copyright strings. */

#define VERSION    "3.22"
#define COPYRIGHT  "Copyright (c) University of Cambridge 2024"

#include "ehdr.h"

void
version_init(void)
{
uschar *p;
int i = 0;
uschar today[20];

version_copyright = US COPYRIGHT;
version_string = US VERSION;

#ifdef USE_PCRE1
Ustrcpy(version_pcre, pcre_version());
#else  /* PCRE2 */
(void)pcre2_config(PCRE2_CONFIG_VERSION, version_pcre);
#endif

p = version_pcre + Ustrlen(version_pcre);
while (p > version_pcre && !isspace(p[-1])) p--;
while (p > version_pcre && isspace(p[-1])) p--;
*p = 0;

Ustrcpy(today, __DATE__);
if (today[4] == ' ') i = 1;
today[3] = today[6] = '-';

/* We used to use Ustrncat() to extract and re-arrange the date string, but GCC
now warns when the whole string isn't copied. */

version_date[0] = '(';
p = memcpy(version_date+1, today+4+i, 3-i);
p = memcpy(p + 3-i, today, 4);
p = memcpy(p + 4, today+7, 4);
(void)Ustrcpy(p+4, ")");
}

/* End of c.eversion */
