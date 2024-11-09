CFLAGS = -g -O0 -ansi -w -Wno-return-type -DFLEXNAMES -DBSD2_86 -Di286=1
YACC = ../yacc/yacc
OBJ = cpp.o cpy.o

all: cpp

clean:
	rm -f cpp y.tab.c *.o *.core

cpp: ${OBJ}
	${CC} -o $@ ${OBJ} ${CFLAGS}

cpy.o: cpy.y yylex.c
	${YACC} cpy.y
	${CC} -c -o $@ y.tab.c ${CFLAGS}
	rm -f y.tab.c
