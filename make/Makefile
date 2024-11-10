all: make

clean:
	rm -f make *.core

run: make
	./make

make: make.c make.h
	cc -o make make.c -ansi -g -O0 -Wall -Wno-deprecated-non-prototype -Wno-comment -Wno-implicit-int

