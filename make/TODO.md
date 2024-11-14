# Write a useful README about how this make differs from all the other makes
- Current situation of using make in big projects

# Write a tutorial on how to correctly use this make

# Options
-q	check if anythings needs to be built
-k	keep going, even after an error happened
-h	print a generated help page

# `+=` Operator
## MyMakefile
```make
CFLAGS = -ansi
.export CFLAGS
```

## sub/MyMakefile
```
CFLAGS += -Wall
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

# Position-dependent macro
## MyMakefile
```make
TOP = $.

print:
    @echo TOP: ${TOP}
```

## sub/MyMakefile
```make
print:
    @echo TOP: ${TOP}
```

## Output
```
$ make print
TOP: .

$ make sub/print
TOP: ./..
```

# Support for artifact directories

# Check file names for validity

# Create a specification

# Create a freestanding version, written in Rust, so that other projects can use this make
