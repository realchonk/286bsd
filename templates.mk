## Directory Template
.template dir
__CLEAN = ${.SUBDIRS:=/clean}
__INSTALL = ${.SUBDIRS:=/install}

.DEFAULT: all

## Build: ${.SUBDIRS}
all: ${.SUBDIRS}

## Clean: ${.SUBDIRS}
clean: ${__CLEAN}

## Install: ${.SUBDIRS}
install: ${__INSTALL}
.endt


## Program Template
.template prog
## Installation prefix for ${NAME}
PREFIX ?= /usr

.if !"${NOMAN}"
## Installation prefix for ${NAME}'s manual
MANPREFIX ?= ${PREFIX}/man/man${MANSECTION}
MANSECTION ?= 1
.endif

## Installation prefix for ${NAME}'s binary
BINPREFIX ?= ${PREFIX}/bin

.DEFAULT: all

.if !target(all)
## Build ${NAME}
all: ${NAME}

.if target(all-extra)
all: all-extra
.endif
.endif

.if !target(clean)
## Clean build artifacts for ${NAME}
clean:
	rm -f ${NAME} *.core *.o

.if target(clean-extra)
clean: clean-extra
.endif
.endif

.if !target(install)
## Install ${NAME} into ${DESTDIR}${PREFIX}
install: install-bin

## Install ${NAME}'s binary into ${DESTDIR}${BINPREFIX}
install-bin: ${NAME}
	mkdir -p ${DESTDIR}${BINPREFIX}/
	cp -f ${NAME} ${DESTDIR}${BINPREFIX}/

.if !"${NOMAN}"
install: install-man

## Install ${NAME}'s manual into ${DESTDIR}${MANPREFIX}
install-man: ${NAME}.${MANSECTION}
	mkdir -p ${DESTDIR}${MANPREFIX}
	cp -f ${NAME}.${MANSECTION} ${DESTDIR}${MANPREFIX}/

.endif

.if target(install-extra)
install: install-extra
.endif
.endif
.endt
