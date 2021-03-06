# $OpenBSD: Makefile,v 1.28 2013/05/31 18:03:43 lum Exp $

PROG=	mg

LDADD+=	-L/usr/local/lib -lcurses -lutil -lclens
DPADD+=	${LIBCURSES} ${LIBUTIL}

# (Common) compile-time options:
#
#	FKEYS		-- add support for function key sequences.
#	REGEX		-- create regular expression functions.
#	STARTUP		-- look for and handle initialization file.
#	XKEYS		-- use termcap function key definitions.
#				note: XKEYS and bsmap mode do _not_ get along.
#
CFLAGS+=-I/usr/local/include/clens -Wall -DFKEYS -DREGEX -DXKEYS -DSTARTUP -DNEED_LIBCLENS

SRCS=	autoexec.c basic.c bell.c buffer.c cinfo.c dir.c display.c \
	echo.c extend.c file.c fileio.c funmap.c help.c kbd.c keymap.c \
	line.c macro.c main.c match.c modes.c paragraph.c random.c \
	re_search.c region.c search.c spawn.c tty.c ttyio.c ttykbd.c \
	undo.c version.c window.c word.c yank.c

#
# More or less standalone extensions.
#
SRCS+=	cmode.c cscope.c dired.c grep.c tags.c theo.c

afterinstall:
	${INSTALL} -d ${DESTDIR}${DOCDIR}/mg
	${INSTALL} -m ${DOCMODE} -c ${.CURDIR}/tutorial \
		${DESTDIR}${DOCDIR}/mg

install:
	cp mg /usr/local/bin/
	cp mg.1 /usr/local/share/man/man1

.include <bsd.prog.mk>
