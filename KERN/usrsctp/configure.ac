dnl
dnl Copyright (C) 2011-2012 Michael Tuexen
dnl
dnl All rights reserved.
dnl
dnl Redistribution and use in source and binary forms, with or without
dnl modification, are permitted provided that the following conditions
dnl are met:
dnl 1. Redistributions of source code must retain the above copyright
dnl    notice, this list of conditions and the following disclaimer.
dnl 2. Redistributions in binary form must reproduce the above copyright
dnl    notice, this list of conditions and the following disclaimer in the
dnl    documentation and/or other materials provided with the distribution.
dnl 3. Neither the name of the project nor the names of its contributors
dnl    may be used to endorse or promote products derived from this software
dnl    without specific prior written permission.
dnl
dnl THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
dnl ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
dnl IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
dnl ARE DISCLAIMED.	IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
dnl FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
dnl DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
dnl OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
dnl HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
dnl LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
dnl OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
dnl SUCH DAMAGE.
dnl

AC_INIT([libusrsctp], [0.9.2.1])
AM_INIT_AUTOMAKE

AC_PROG_CC_C99
AC_PROG_LIBTOOL
AC_CANONICAL_HOST
AC_CONFIG_MACRO_DIR([m4])

LIBCFLAGS="-DSCTP_PROCESS_LEVEL_LOCKS -DSCTP_SIMPLE_ALLOCATOR -D__Userspace__"
case $host_os in
darwin*)
  CFLAGS="$CFLAGS -pedantic -Wall -std=c99 -Werror -D__APPLE_USE_RFC_2292"
  LIBCFLAGS="$LIBCFLAGS -U__APPLE__ -D__Userspace_os_Darwin"
  ;;
dragonfly*)
  CFLAGS="$CFLAGS -pedantic -Wall -Werror -pthread"
  LIBCFLAGS="$LIBCFLAGS -U__DragonFly__ -D__Userspace_os_DragonFly"
  ;;
freebsd*)
  CFLAGS="$CFLAGS -pedantic -Wall -Werror -pthread"
  if $CC --version | grep -q clang; then
    LDFLAGS="$LDFLAGS -Qunused-arguments"
  fi
  LIBCFLAGS="$LIBCFLAGS -U__FreeBSD__ -D__Userspace_os_FreeBSD"
  ;;
linux*)
  CFLAGS="$CFLAGS -pedantic -Wall -Werror -pthread -D_GNU_SOURCE"
  LIBCFLAGS="$LIBCFLAGS  -D__Userspace_os_Linux"
  ;;
netbsd*)
  CFLAGS="$CFLAGS -pedantic -Wall -Werror -pthread"
  LIBCFLAGS="$LIBCFLAGS -U__NetBSD__ -D__Userspace_os_NetBSD"
  ;;
openbsd*)
  CFLAGS="$CFLAGS -pedantic -Wall -Werror -pthread"
  LIBCFLAGS="$LIBCFLAGS -U__OpenBSD__ -D__Userspace_os_OpenBSD"
  ;;
solaris*)
  CFLAGS="$CFLAGS -D_XPG4_2"
  ;;
esac
LIBCFLAGS="$CFLAGS $LIBCFLAGS"

AC_ARG_ENABLE(debug,
  AC_HELP_STRING( [--enable-debug],
                  [provide debug information @<:@default=yes@:>@]),
    enable_debug=$enableval,enable_debug=yes)
if test x$enable_debug = xyes; then
        AC_DEFINE(SCTP_DEBUG, 1, [Provide debug information])
fi

AC_ARG_ENABLE(inet,
  AC_HELP_STRING( [--enable-inet],
                  [Support IPv4 @<:@default=yes@:>@]),
    enable_inet=$enableval,enable_inet=yes)
if test x$enable_inet = xyes; then
        AC_DEFINE(INET, 1, [Support IPv4])
fi

AC_ARG_ENABLE(inet6,
  AC_HELP_STRING( [--enable-inet6],
                  [Support IPv6 @<:@default=yes@:>@]),
    enable_inet6=$enableval,enable_inet6=yes)
if test x$enable_inet6 = xyes; then
        AC_DEFINE(INET6, 1, [Support IPv6])
fi

AC_CHECK_TYPE(size_t)
AC_CHECK_TYPE(ssize_t)

AC_CHECK_FUNCS(socket, , AC_CHECK_LIB(socket, socket))
AC_CHECK_FUNCS(inet_addr, , AC_CHECK_LIB(nsl, inet_addr))

AC_CHECK_HEADER(sys/types.h)
AC_CHECK_HEADERS(linux/if_addr.h, [], [], [#include <sys/socket.h>])
AC_CHECK_HEADERS(linux/rtnetlink.h, [], [], [#include <sys/socket.h>])
AC_CHECK_HEADERS(netinet/ip_icmp.h, [], [], [#include <netinet/ip.h>])

AC_CHECK_MEMBER(struct sockaddr.sa_len,
                AC_DEFINE(HAVE_SA_LEN, 1, [Define this if your stack has sa_len in sockaddr struct.]),,
                [#ifdef HAVE_SYS_TYPES_H
                 #include <sys/types.h>
                 #endif
                 #include <sys/socket.h>])

AC_CHECK_MEMBER(struct sockaddr_in.sin_len,
                AC_DEFINE(HAVE_SIN_LEN, 1, [Define this if your IPv4 has sin_len in sockaddr_in struct.]),,
                [#ifdef HAVE_SYS_TYPES_H
                 #include <sys/types.h>
                 #endif
                 #include <netinet/in.h>])

AC_CHECK_MEMBER(struct sockaddr_in6.sin6_len,
                AC_DEFINE(HAVE_SIN6_LEN, 1, [Define this if your IPv6 has sin6_len in sockaddr_in6 struct.]),,
                [#ifdef HAVE_SYS_TYPES_H
                 #include <sys/types.h>
                 #endif
                 #include <netinet/in.h>])

AC_CHECK_MEMBER(struct sockaddr_conn.sconn_len,
                AC_DEFINE(HAVE_SCONN_LEN, 1, [Define this if your userland stack has sconn_len in sockaddr_conn struct.]),,
                [#include "usrsctplib/usrsctp.h"])

AC_MSG_CHECKING(for socklen_t)
AC_TRY_COMPILE([#ifdef HAVE_SYS_TYPES_H
                #include <sys/types.h>
                #endif
                #include <sys/socket.h>],
               [socklen_t x; x = 1; return ((int)x);],
               [AC_MSG_RESULT(yes)],
               [AC_MSG_RESULT(int)
                AC_DEFINE(socklen_t, int, [Define a type for socklen_t.])])

AC_C_BIGENDIAN

AC_SUBST([LIBCFLAGS])
AC_OUTPUT(Makefile usrsctplib/Makefile programs/Makefile)
