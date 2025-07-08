dnl SPDX-License-Identifier: OSL-3.0
dnl Copyright (C) 2025  Trevor Woerner <twoerner@gmail.com>

nl AC_PREREQ(2.57)
AC_INIT([mqtt-shelly-ht], [0.1.0], [twoerner@gmail.com], [mqtt-shelly-ht])
AC_CONFIG_SRCDIR(src/mqtt-shelly-ht.c)
AC_CONFIG_AUX_DIR(cfg)
AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-xz])
AM_CONFIG_HEADER(cfg/config.h)

SUBDIRS="src extras"

dnl **********************************
dnl checks for programs
dnl **********************************
AC_PROG_CC
AC_PROG_CPP
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AC_PROG_LN_S

dnl **********************************
dnl checks for libraries
dnl **********************************
AC_SEARCH_LIBS(mosquitto_lib_init,mosquitto,,AC_MSG_ERROR([can't find mosquitto library]),)
AC_SEARCH_LIBS(json_tokener_parse,[json-c json],,AC_MSG_ERROR([can't find json-c library]),)

dnl **********************************
dnl checks for header files
dnl **********************************
AC_HEADER_STDC
AC_CHECK_HEADERS(stdio.h stdlib.h stdbool.h string.h)
AC_CHECK_HEADERS(unistd.h getopt.h errno.h fcntl.h sys/types.h sys/stat.h time.h sys/time.h)
AC_CHECK_HEADERS(mosquitto.h json/json.h)

dnl **********************************
dnl checks for typedefs, structs, and
dnl compiler characteristics
dnl **********************************
AC_TYPE_SIZE_T

dnl **********************************
dnl output
dnl **********************************
AC_SUBST(SUBDIRS)
AC_OUTPUT(Makefile
cfg/Makefile
extras/Makefile
src/Makefile)
