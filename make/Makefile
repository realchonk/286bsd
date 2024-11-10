CC = cc
CFLAGS =  ${TOP_CFLAGS} -ansi -Wall -Wno-deprecated-non-prototype -Wno-comment -Wno-implicit-int
PWD != pwd


all: make
	echo ${PWD}
	echo ${SHELL}
	echo ${MAKE}

clean:
	rm -f make *.core

run: make
	./make

make: make.c make.h
	${CC} -o make make.c ${CFLAGS}

