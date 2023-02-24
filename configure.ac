dnl Process this file with autoconf to produce a configure script.

dnl This is required at the start; the name is the name of a file
dnl it should be seeing, to verify it is in the same directory.

AC_INIT
AC_CONFIG_SRCDIR([NOTICE])

dnl Arrange to build config.h from config.in.
dnl Manual says this macro should come right after AC_INIT.

AC_CONFIG_HEADERS([src/config.h])

dnl Checks for programs.

AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL

dnl Checks for header files.

AC_CHECK_HEADERS_ONCE(sys/fcntl.h termio.h termios.h)

dnl Checks for typedefs, structures, and compiler characteristics.

AC_C_CONST
AC_TYPE_SIZE_T

dnl Handle --disable-pcre2

use_pcre="PCRE2"

AC_ARG_ENABLE(pcre2,
  [  --disable-pcre2         disable use of PCRE2 (use PCRE1)],
  if test "$enableval" = "no"; then
    use_pcre="PCRE1"
    USE_PCRE1=-DUSE_PCRE1
  fi
)
  
dnl Check PCRE library's existence

pcre_lib_found="yes"

if test "$use_pcre" = "PCRE2"; then
  AC_CHECK_LIB(pcre2-8, pcre2_config_8, [], 
    [ echo "** PCRE2 library not found" 
      echo "** Use --disable-pcre2 to try PCRE1" 
      pcre_lib_found="no"
    ])
else      
  AC_CHECK_LIB(pcre, pcre_config, [], 
    [ echo "** PCRE1 library not found" 
      echo "** Omit --disable-pcre2 to try PCRE2" 
      pcre_lib_found="no"
    ])
fi    

if test "$pcre_lib_found" != "yes"; then
  echo "** If your PCRE library is not in a standard library path, try"
  echo "** LDFLAGS=-L/some/path/lib ./configure". 
  AC_MSG_ERROR(cannot find $use_pcre library, 1)  
fi

dnl Handle --disable-vdiscard

use_vdiscard="yes"
AC_ARG_ENABLE(vdiscard,
[  --disable-vdiscard      disable use of VDISCARD],
if test "$enableval" = "no"; then
  VDISCARD=-DNO_VDISCARD
  use_vdiscard="no"
fi
)

dnl Handle --enable-termcap

use_capinfo="terminfo"
AC_ARG_ENABLE(termcap,
[  --enable-termcap        enable TERMCAP support],
if test "$enableval" = "yes"; then
  TERMCAP=-DHAVE_TERMCAP
  use_capinfo="termcap"
fi
)

dnl Check terminal library's existence

if test "$use_capinfo" = "terminfo"; then
  AC_CHECK_LIB(ncurses, tigetstr, [], 
    [ echo "** terminfo library not found" 
      echo "** Use --enable-termcap to try termcap" 
      AC_MSG_ERROR(cannot find terminfo library, 1)
    ])
else      
  AC_CHECK_LIB(termcap, tgetstr, [], 
    [ echo "** termcap library not found" 
      echo "** Omit --enable-termcap to try terminfo" 
      AC_MSG_ERROR(cannot find termcap library, 1)
    ])
fi    

dnl Variables that are substituted

AC_SUBST(LDFLAGS)
AC_SUBST(LIBS)
AC_SUBST(TERMCAP)
AC_SUBST(USE_PCRE1)
AC_SUBST(VDISCARD)

dnl Write these files

AC_CONFIG_FILES(
  Makefile
)

dnl This must be last
AC_OUTPUT

# Show chosen options.

cat <<EOF

NE configuration summary:

    Install prefix .................. : ${prefix}
    C preprocessor .................. : ${CPP}
    C compiler ...................... : ${CC}
    C compiler flags ................ : ${CFLAGS}
    Link flags ...................... : ${LDFLAGS}
    Extra libraries ................. : ${LIBS}
    Use PCRE1 or PCRE2 .............. : ${use_pcre}
    Use termcap or terminfo ......... : ${use_capinfo}
    Use VDISCARD .................... : ${use_vdiscard}

EOF

dnl end configure.ac
