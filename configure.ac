#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

define(VERSION_ID,esyscmd([
	git show --format=%H | head -n 1 | tr -d '\n';
]))

AC_PREREQ([2.69])
AC_INIT([Portsplit], VERSION_ID, [kheops@ceops.eu], [portsplit], [https://github.com/kheops2713/portsplit])
AM_INIT_AUTOMAKE([-Wall])
AC_CONFIG_SRCDIR([include/main.h src/main.c])
AC_CONFIG_HEADERS([include/config.h])
AC_CANONICAL_HOST

AM_CFLAGS([-ansi -pedantic -O2 -Wchar-subscripts -Wcomment -Wformat=2 -Wimplicit-int -Werror-implicit-function-declaration -Wmain -Wparentheses -Wsequence-point -Wreturn-type -Wswitch -Wtrigraphs -Wunused -Wuninitialized -Wunknown-pragmas -Wfloat-equal -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast -Wwrite-strings -Wconversion -Wsign-compare -Waggregate-return -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wmissing-noreturn -Wformat -Wmissing-format-attribute -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wlong-long -Wunreachable-code -Wno-missing-braces -Wno-missing-field-initializers -D_POSIX_C_SOURCE=199309L])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([util], [openpty])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h
                  fcntl.h
                  netdb.h
                  stdio.h
                  stdlib.h
                  string.h
                  strings.h
                  sys/socket.h
                  sys/select.h
                  time.h
                  sys/types.h
                  sys/wait.h
                  sys/stat.h
                  unistd.h
                  pty.h
                  endian.h
                  errno.h
                  signal.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([dup2 memset select socket strerror])

AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT