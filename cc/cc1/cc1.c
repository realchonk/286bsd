#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define IDENT_LEN 8

// ERROR HANDLING

int line;

error (msg)
char *msg;
{
	fprintf (stderr, "cc1: %d: error: %s\n", line, msg);
	return exit (1), -1;
}

// LEXER

enum {
	TK_IDENT = 128,
	TK_EOF,
};

union {
	char ident[IDENT_LEN + 1];
} lval;

int peekdch = EOF;

readch (void)
{
	int ch = getchar ();

	if (ch == '\n')
		++line;
	
	return ch;
}

peekch (void)
{
	if (peekdch != EOF)
		peekdch = readch ();

	return peekdch;
}

nextch (void)
{
	int ch;

	if (peekdch != EOF) {
		ch = peekdch;
		peekdch = EOF;
	} else {
		ch = readch ();
	}

	return ch;
}

isname (ch)
{
	return isalnum (ch) || ch == '_';
}

lex (void)
{
	size_t i;
	int ch;

	while (isspace (peekch ()))
		nextch ();

	ch = peekch ();

	if (isdigit (ch)) {
		return error ("TODO: integers");
	} else if (isname (ch)) {
		for (i = 0; isname (peekch ());) {
			ch = nextch ();
			if (i < IDENT_LEN)
				lval.ident[i++] = ch;
		}
		lval.ident[i] = '\0';
		return TK_IDENT;
	} else switch (ch) {
	case '(':
	case ')':
	case '{':
	case '}':
		return ch;
	default:
		return error ("invalid input");
	}
}

int peekd = TK_EOF;

peek (void)
{
	if (peekd == TK_EOF)
		peekd = lex ();

	return peekd;
}

next (void)
{
	int tk;

	if (peekd != TK_EOF) {
		tk = peekd;
		peekd = TK_EOF;
	} else {
		tk = lex ();
	}

	return tk;
}

main (void)
{
	return 0;
}
