CC = cc
CFLAGS =  -ansi -g -O0 -Wall -Wno-deprecated-non-prototype -Wno-comment -Wno-implicit-int

all: make

clean:
	rm -f make *.core

run: make
	./make

make: make.c make.h
	${CC} -o make make.c ${CFLAGS}

