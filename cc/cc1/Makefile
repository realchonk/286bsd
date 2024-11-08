YACC = ../yacc/yacc
YFLAGS = -d -v
CFLAGS = -ansi -g -O0 -Wno-deprecated-non-prototype -Wno-comment
OBJ = lex.o parse.o main.o

all: cc1

clean:
	rm -f cc1 *.o y.tab.h y.output

run: cc1
	./cc1 < test.c

cc1: ${OBJ}
	${CC} -o $@ ${OBJ} ${CFLAGS}

parse.o y.tab.h: parse.y
	${YACC} ${YFLAGS} parse.y
	${CC} -c -o parse.o y.tab.c ${CFLAGS}
	rm -f y.tab.c

main.o lex.o: y.tab.h
${OBJ}: cc1.h
