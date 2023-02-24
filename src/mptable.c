/*************************************************
*       The E text editor - 3rd incarnation      *
*************************************************/

/* Copyright (c) University of Cambridge, 1991 - 2023 */

/* Written by Philip Hazel, starting November 1991 */
/* This file last modified: January 2023 */

/* This file is not part of NE itself. It is an auxiliary program that tests
UTF-8 characters for screen displayability. It creates the source of
chdisplay.c, which contains a bit table for those that are. Characters with
code points in the range U+0100 to U+FFFF are tested; those with lesser code
points are assumed.

The program works only in an xterm that has UTF-8 enabled, by reading the
cursor position, outputting a candidate character in UTF-8, re-reading the
cursor position, and determining whether the character has displayed something
using precisely one character box. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <termio.h>
#include <unistd.h>

static const int utf8_table1[] = {
  0x0000007f, 0x000007ff, 0x0000ffff, 0x001fffff, 0x03ffffff, 0x7fffffff};

static const int utf8_table2[] = {
  0, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc};

static int ioctl_fd;


static int
ord2utf8(int cvalue, unsigned char *buffer)
{
register int i, j;
for (i = 0; i < sizeof(utf8_table1)/sizeof(int); i++)
  if (cvalue <= utf8_table1[i]) break;
if (i >= sizeof(utf8_table1)/sizeof(int)) return 0;
if (cvalue < 0) return -1;
buffer += i;
for (j = i; j > 0; j--)
 {
 *buffer-- = 0x80 | (cvalue & 0x3f);
 cvalue >>= 6;
 }
*buffer = utf8_table2[i] | cvalue;
return i + 1;
}


static void
read_position(int *row, int *col)
{
char buff[8];
if (write(ioctl_fd, "\x1b\x5b\x36\x6e", 4));

*row = 0;
*col = 0;

if (read(ioctl_fd, buff, 2));    /* Reads ESC [ */

for (;;)
  {
  if (read(ioctl_fd, buff, 1));  /* Reads one digit */
  if (buff[0] == ';') break;
  *row = *row * 10 + buff[0] -'0';
  }

for (;;)
  {
  if (read(ioctl_fd, buff, 1));  /* Reads one digit */
  if (buff[0] == 'R') break;
  *col = *col * 10 + buff[0] -'0';
  }
}


static int
is_printable(int c)
{
unsigned char buff[8];
int len = ord2utf8(c, buff);
int i, rowa, rowb, cola, colb;
if (len <= 0)
  {
  printf("**** UTF-8 error %d for U+%0x\n", len, c);
  exit(1);
  }
read_position(&rowb, &colb);
if (write(ioctl_fd, buff, len));
read_position(&rowa, &cola);
if (write(ioctl_fd, "\r", 1));
return rowa == rowb && cola - colb == 1;
}


static void
setrange(unsigned char *p, int start, int end, int *pcount)
{
if (*pcount >= 7)
  {
  printf("%04x-%04x\n", start, end);
  *pcount = 0;
  }
else
  {
  printf("%04x-%04x ", start, end);
  *pcount += 1;
  }

for (; start <= end; start++) p[start/8] |= (1 << (start%8));
}


int
main(void)
{
struct termios oldparm, newparm;
unsigned char *p = malloc(8192);
int startrange = -1;
int count = 0;
int c;

printf("/*************************************************\n"
       "*       The E text editor - 3rd incarnation      *\n"
       "*************************************************/\n\n"

       "/* Copyright (c) University of Cambridge, 1991 - 2023 */\n"
       "/* Written by Philip Hazel, starting November 1991 */\n\n"

       "/* This file is generated by the mptable program. It defines a table\n"
       "of bits that identifies which Unicode characters in the range 0 to\n"
       "0xffff are displayable. A one bit means 'not displayable'. These are\n"
       "the non-displayable ranges:\n\n");

memset((char *)p, 0, 8192);
setrange(p, 0, 0x1f, &count);
setrange(p, 0x7f, 0x9f, &count);

ioctl_fd = open("/dev/tty", O_RDWR);
tcgetattr(ioctl_fd, &oldparm);
newparm = oldparm;
newparm.c_iflag &= ~(IGNCR | ICRNL);
newparm.c_lflag &= ~(ICANON | ECHO | ISIG);
newparm.c_cc[VMIN] = 1;
newparm.c_cc[VTIME] = 0;
newparm.c_cc[VSTART] = 0;
newparm.c_cc[VSTOP] = 0;
#ifndef NO_VDISCARD
newparm.c_cc[VDISCARD] = 0;
#endif
tcsetattr(ioctl_fd, TCSANOW, &newparm);

for (c = 0x100; c < 0xffff; c++)
  {
  if (is_printable(c))
    {
    if (startrange < 0) continue;
    setrange(p, startrange, c - 1, &count);
    startrange = -1;
    }
  else
    {
    if (startrange < 0) startrange = c;
    }
  }

if (startrange >= 0) setrange(p, startrange, c - 1, &count);

tcsetattr(ioctl_fd, TCSANOW, &oldparm);
close(ioctl_fd);

printf("*/\n\n#include \"ehdr.h\"\n\n"
       "uschar ch_displayable[] = {\n  0x%02x,", p[0]);
for (c = 1; c < 8191; c++)
  {
  if (c%8 == 0) printf("  /* %04x */\n  ", (c - 8)*8);
  printf("0x%02x,", p[c]);
  }
printf("0x%02x}; /* ffc0 */\n", p[8191]);

return 0;
}

/* End of mptable.c */
