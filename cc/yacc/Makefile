PWD != pwd
CFLAGS = -ansi -Wno-return-type -Wno-comment -Wno-deprecated-non-prototype
CPPFLAGS = -DWORD32 -DPARSER=\"${PWD}/yaccpar\"
SRC != echo y?.c

all: yacc calc

yacc: ${SRC}
	${CC} -o $@ ${SRC} ${CFLAGS} ${CPPFLAGS}

clean:
	rm -f yacc *.o *.core y.tab.c calc.c

calc.c: yacc calc.y
	./yacc calc.y
	mv y.tab.c calc.c

run: calc
	./calc
