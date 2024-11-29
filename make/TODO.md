# Choose a name for this make
- hmake (hireachical make)
- rmake (recursive make)
- bmake (already used by BSD Make)
- dmake/dirmk/dmk (directory make)


# Write a useful README about how this make differs from all the other makes
- Motivation for writing this make
    - Make is great, but not really suitable for large projects
        - Either recursive make, or one gigantic Makefile (both are bad)
    - Alternatives
        - Autotools is way too painful and uses recursive make
        - CMake is too fat and is written in C++
        - Meson is written in Python
        - All of the above require learning new languages
- Fundamental difference between this make, and all the others
    - Targets/Dependencies are treated as paths, not strings
    - Every directory is associated with a scope
    - There is a Tree of Scopes
    - Minor differences
        - Inference rules can be arbitrarily deep
        - No need for `.SUFFIXES:`
        - Macro assignment works slightly different
            - use `?=` if you want to allow overriding an assignment
- Explain how this make works
    - Data Structures
        - `scope`
        - `directory` and `gnu`
        - `file`
        - `rule`
        - `dep`
    - Algorithms
        - `parse_recursive()` (bottom-up)
        - `parse()` (top-down)
        - lazy includes
        - `expand()`
        - `build()` and `build_file()`
        - Inference rules
- List of Features
    - "Usual" Make Features
    - Scoped Include
    - Integration with "foreign" make directories (GNU, BSD, ...)
    - "Exporting" macros into child scopes
- Where this make could be useful
    - Large projects with many subdirectories
- Future Work


# Options
-q	        check if anythings needs to be built
-k	        keep going, even after an error happened
-S          opposite of `-k`
-h	        print a generated help page
-s          silent, do not write commands to stdout


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


# Conditional Operators
< <= >= >


# Special macros
## Rename `$*` to something different
POSIX specifies, that `$*` evaluates to the current target's basename, without the suffix.


# Special targets
## `.TOP:` to stop the upward recursion
This must be defined at the top of the file.

## `.POSIX:` and `.SUFFIXES:` should give a warnings about this make not being POSIX

## `.PATH:`
Allow specifying additional `PATH` directories.


# Reconsider `.include` syntax
Maybe change to something like this:
```make
# In this example sys is a foreign-make project

# Check if target has to be rebuilt in sys
# If this is omitted, make will always see the target as outdated.
sys?:
    gmake -q -C sys $<

# Actually build target for sys
sys!:
    gmake -C sys $<

# List of subdirectories
.SUBDIRS: cc make sys
```

## Example for projects using Autotools
This is an example, of how an autotools-dependency might look like.
```make
.SUBDIRS: libedit
LIBEDIT = libedit-20240808-3.1

${LIBEDIT}.tgz:
    ftp -o $@ https://www.thrysoee.dk/editline/${LIBEDIT}.tar.gz

.libedit-extract: ${LIBEDIT}.tgz
    tar -xzf $<
    touch $@

.libedit-configured: .libedit-extracted
    mkdir -p build-libedit
    (cd build-libedit               \
        && ../${LIBEDIT}/configure  \
            --prefix=${PREFIX}      \
            --enable-static         \
            --disable-shared)
    touch $@

# Look if $< needs to be built for libedit
libedit?: .libedit-configured
    gmake -C build-libedit $<

# Actually build $< for libedit
libedit!: .libedit-configured
    gmake -q -C build-libedit $<

```


# Provide implementations of non-standard functions (libcompat)
- `err(3)` and `errx(3)`
- `reallocarray(3)`
- `strsep(3)`


# Future TODOs
## Support for artifact directories
```make
# With .OBJ set, make will first search in
# the obj directory for artifacts, before
# looking in the current directory.
# Make will also put generated files into obj,
# by changing the value of $@.
.OBJ: obj

all: example

example: example.c
    ${CC} -o $@ example.c
```
## Check names for validity
Allowed charset for variable names: `[a-zA-Z_.]+`
Allowed charset for file names: `[a-zA-Z_.-]+`
## Include file and position in every error message
## Write a tutorial on how to correctly use this make
## Write a specification
## Create a freestanding version, written in Rust, so that other projects can use this make

# Known Bugs
## Unnamed bug #1
### Problem
`clean: ${.SUBDIRS:=/clean}`
is interpreted as macro definition:
`clean: ${.SUBDIRS:` = `/clean}`
### Workaround
```make
CLEAN = ${.SUBDIRS:=/clean}
`clean: ${CLEAN}`
```
