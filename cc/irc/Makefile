CFLAGS = -ansi -Wall -Wextra -Wno-deprecated-non-prototype -Wno-implicit-int -Wno-comment -O2

all: irc

clean:
	rm -f irc *.core test.asm

run: all
	./irc < test.ir > test.asm
	cat test.asm
	nasm -fbin -o /dev/null test.asm


