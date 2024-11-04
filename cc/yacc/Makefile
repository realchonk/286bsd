CFLAGS = -ansi -Wno-return-type -Wno-comment -Wno-deprecated-non-prototype
SRC != echo y*.c

all: yacc

yacc: ${SRC}
	${CC} -o $@ ${SRC} ${CFLAGS}

clean:
	rm -f yacc *.o *.core

run: yacc
	./yacc calc.y
