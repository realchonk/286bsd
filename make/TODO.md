# Write a useful README about how this make differs from all the other makes
- Current situation of using make in big projects


# Options
-q	        check if anythings needs to be built
-k	        keep going, even after an error happened
-S          opposite of `-k`
-h	        print a generated help page
-s          silent, do not write commands to stdout
-f file     read `file` instead of `MyMakefile`


# "normal" `include file`


# Macro Definition Operators
## `+=`: Append Assignment
### MyMakefile
```make
CFLAGS = -ansi
.export CFLAGS
```
### sub/MyMakefile
```
CFLAGS += -Wall
```

## `:=`: Immediate Assignment (easy)
```make
MACRO := value
```


# Doc comments for help page
```make
## build the code base
all: a b

## delete build artifacts
clean:

## build component "a"
a:

## build component "b"
b:
```


# Templates
## MyMakefile
```make
.template prog
PREFIX ?= /usr
SRC ?= ${NAME}.c

all: ${NAME}

clean:
	rm -f ${NAME} *.o

install: ${NAME}
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f ${NAME} ${DESTDIR}${PREFIX}/bin/

${NAME}: ${SRC}
	${CC} -o $@ ${SRC}
.endt
```

## cat/MyMakefile
```make
NAME = cat

.inst prog
```


# Conditionals
```make
NOMAN = 1

.if ${NOMAN} == 1
install-man:
.else
install-man: ${NAME}.1
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	cp -f ${NAME}.1 ${DESTDIR}${MANPREFIX}/man1/
.endif
```


# Special macros
## `$.`: Position-dependent macro
### MyMakefile
```make
TOP = $.
print:
    @echo TOP: ${TOP}
```
### sub/MyMakefile
```make
print:
    @echo TOP: ${TOP}
```
### Output
```
$ make print
TOP: .
$ make sub/print
TOP: ./..
```

## `${.SUBDIRS}`
Useful for this:
```make
all:
clean:

.include sub1
.include sub2
.include sub3

all: ${.SUBDIRS}
clean: ${.SUBDIRS/clean}
```

## `${MAKE}`, `${.MAKE}`, `${MAKEFLAGS}` and `${.MAKEFLAGS}`

## `${name:subst1=subst2}`: Macro Substitution
Substitute all occurences of `subst1` with `subst2` in macro `name`.

## `${name/suffix}`
Append `suffix` to each of the files in `${name}`.

## `$(...)`
This syntax is reserved for future use.
A hint about `${...}` should be printed.


# Special targets
## `.TOP:` to stop the upward recursion
This must be defined at the top of the file.

## `.POSIX:` and `.SUFFIXES:` should give a warnings about this make not being POSIX


# Future TODOs
## Support for artifact directories
## Check file names for validity
## Write a tutorial on how to correctly use this make
## Write a specification
## Create a freestanding version, written in Rust, so that other projects can use this make
