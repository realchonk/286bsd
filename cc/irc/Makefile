CFLAGS = -ansi -Wno-deprecated-non-prototype

all: irc

clean:
	rm -f irc *.core test.asm

run: all
	./irc < test.ir | tee test.asm
	nasm -fbin -o /dev/null test.asm


