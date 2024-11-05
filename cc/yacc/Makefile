PWD != pwd
CFLAGS = -ansi -Wno-return-type -Wno-comment -Wno-deprecated-non-prototype
CPPFLAGS = -DWORD32 -DPARSER=\"${PWD}/yaccpar\"
SRC != echo y?.c

all: yacc

yacc: ${SRC}
	${CC} -o $@ ${SRC} ${CFLAGS} ${CPPFLAGS}

clean:
	rm -f yacc *.o *.core

run: yacc
	./yacc calc.y
