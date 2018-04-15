# Makefile for OBS server
PROG=	obs
SRCS=	main.c sem.c shm.c utility.c

INC= -I ../../kernel/

DPADD+=	${LIBSYS}
LDADD+=	-lsys

MAN=

CFLAGS = -Iinclude

BINDIR?= /usr/sbin
FILES=obs.conf
FILESNAME=obs
FILESDIR= /etc/system.conf.d

CPPFLAGS+=	-I${NETBSDSRCDIR}

.include <minix.bootprog.mk>
