# The normal targets in this Makefile are not called directly, but should be
# called from the top-level Makefile. To make it easy when developing from
# within the source directory, a call with no target gets passed back up to
# the outer Makefile.

intcall:;      @cd ..; $(MAKE);

# As well as compiler-specific flags, CFLAGS may contain the following:
#
#   -DUSE_PCRE1     Use the PCRE1 library (old API) instead of the default
#                   PCRE2 library (new API)
#
#   -DHAVE_TERMCAP  Should be set on systems where termcap is used. If
#                   it is not set, the use of terminfo is assumed.
#
#   -DNO_VDISCARD   Should be set for Unix systems where the VDISCARD
#                   terminal control character is not supported.

# INCLUDE contains any -I options that are necessary for compilation.

# LDFLAGS contains options for the linker. This can include -L options
# that come before NE's object modules in the command.

# LIBS contains libraries, including the appropriate library for the type of
# terminal handling in use (typically either -lcurses or -ltermcap), and
# the PCRE library (-lpcre or -lpcre2-8).


# Compile step for the modules

.SUFFIXES:  .o .c
.c.o:;      @echo "CC $*.c";
	    $(FE)$(CC) -c $(CFLAGS) $(INCLUDE) $*.c

HDRS = cmdhdr.h config.h ehdr.h keyhdr.h mytypes.h scomhdr.h shdr.h structs.h \
  unixhdr.h

OBJ = debug.o chdisplay.o ecrash.o ecmdarg.o ecmdcomp.o ecmdsub.o ecompP.o \
  ecutcopy.o edisplay.o eerror.o ee1.o ee2.o ee3.o ee4.o efile.o eglobals.o \
  einit.o ekey.o ekeysub.o eline.o ematch.o erdseqs.o escrnrdl.o \
  escrnsub.o estore.o rdargs.o scommon.o sunix.o sysunix.o eversion.o utf8.o

# Link

ne:           $(OBJ)
	      @echo "LD ne"
	      $(FE)$(CC) $(CFLAGS) -o ne $(LDFLAGS) $(OBJ) $(LIBS) -lc
	      @echo ">>> ne built >>>"

# Dependencies

chdisplay.o:  Makefile ../Makefile $(HDRS) chdisplay.c
debug.o:      Makefile ../Makefile $(HDRS) debug.c
ecmdarg.o:    Makefile ../Makefile $(HDRS) ecmdarg.c
ecmdcomp.o:   Makefile ../Makefile $(HDRS) ecmdcomp.c
ecmdsub.o:    Makefile ../Makefile $(HDRS) ecmdsub.c
ecompP.o:     Makefile ../Makefile $(HDRS) ecompP.c
ecrash.o:     Makefile ../Makefile $(HDRS) ecrash.c
ecutcopy.o:   Makefile ../Makefile $(HDRS) ecutcopy.c
edisplay.o:   Makefile ../Makefile $(HDRS) edisplay.c
ee1.o:        Makefile ../Makefile $(HDRS) ee1.c
ee2.o:        Makefile ../Makefile $(HDRS) ee2.c
ee3.o:        Makefile ../Makefile $(HDRS) ee3.c
ee4.o:        Makefile ../Makefile $(HDRS) ee4.c
eerror.o:     Makefile ../Makefile $(HDRS) eerror.c
efile.o:      Makefile ../Makefile $(HDRS) efile.c
eglobals.o:   Makefile ../Makefile $(HDRS) eglobals.c
einit.o:      Makefile ../Makefile $(HDRS) einit.c
ekey.o:       Makefile ../Makefile $(HDRS) ekey.c
ekeysub.o:    Makefile ../Makefile $(HDRS) ekeysub.c
eline.o:      Makefile ../Makefile $(HDRS) eline.c
ematch.o:     Makefile ../Makefile $(HDRS) ematch.c
erdseqs.o:    Makefile ../Makefile $(HDRS) erdseqs.c
escrnrdl.o:   Makefile ../Makefile $(HDRS) escrnrdl.c
escrnsub.o:   Makefile ../Makefile $(HDRS) escrnsub.c
estore.o:     Makefile ../Makefile $(HDRS) estore.c
rdargs.o:     Makefile ../Makefile $(HDRS) rdargs.c
scommon.o:    Makefile ../Makefile $(HDRS) scommon.c
sunix.o:      Makefile ../Makefile $(HDRS) sunix.c
sysunix.o:    Makefile ../Makefile $(HDRS) sysunix.c
utf8.o:       Makefile ../Makefile $(HDRS) utf8.c

# Tidying

clean:;       /bin/rm -f ne *.o

# End
