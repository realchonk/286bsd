#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include "cc1.h"
#include "y.tab.h"

int linenum = 1, peekdch = EOF;
struct token lval;
struct scope *scope = NULL;

// MISC

yyerror (msg)
char *msg;
{
	fprintf (stderr, "cc1: %d: error: %s\n", linenum, msg);
	return exit (1), -1;
}

readch ()
{
	int ch = getchar ();

	if (ch == '\n')
		++linenum;

	return ch;
}

peekch ()
{
	if (peekdch == EOF)
		peekdch = readch ();
	return peekdch;
}

nextch ()
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

// LEXER

yylex ()
{
	struct symbol *sym;
	size_t i;
	char *s;
	int ch;

begin:
	while (isspace (peekch ()))
		nextch ();

	ch = peekch ();

	if (isdigit (ch)) {
		lval.i = 0;

		while (isname (peekch ()))
			lval.i = lval.i * 10 + (nextch () - '0');

		return INTEGER;
	} else if (isname (ch)) {
		s = lval.ident;

		for (i = 0; isname (peekch ());) {
			ch = nextch ();
			if (i < IDENT_LEN)
				s[i++] = ch;
		}
		s[i] = '\0';

		if (strcmp (s, "char") == 0) {
			return CHAR;
		} else if (strcmp (s, "short") == 0) {
			return SHORT;
		} else if (strcmp (s, "int") == 0) {
			return INT;
		} else if (strcmp (s, "long") == 0) {
			return LONG;
		} else if (strcmp (s, "static") == 0) {
			return STATIC;
		} else if (strcmp (s, "extern") == 0) {
			return EXTERN;
		} else if (strcmp (s, "typedef") == 0) {
			return TYPEDEF;
		}

		for (sym = scope->typedefs; sym != NULL; sym = sym->next) {
			if (strcmp (s, sym->ident) == 0) {
				lval.sym = sym;
				return TYPEIDENT;
			}
		}

		return IDENT;
	} else switch (ch) {
	case '(':
	case ')':
	case '{':
	case '}':
	case '=':
	case ';':
	case ',':
	case '*':
	case EOF:
		return nextch ();
	case '/':
		nextch ();
		ch = peekch ();
		if (ch == '/') {
			nextch ();
			while (nextch () != '\n');
			goto begin;
		} else if (ch == '*') {
			while ((ch = nextch ()) != EOF) {
				while ((ch = nextch ()) == '*') {
					if (nextch () == '/')
						goto begin;
				}
			}
		}
		// fallthrough
	default:
		return yyerror ("invalid input");
	}
}

yyinit ()
{
	scope = new (struct scope);
	scope->parent = NULL;
	scope->typedefs = NULL;
	return 0;
}

