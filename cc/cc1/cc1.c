#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define IDENT_LEN 8

// ERROR HANDLING

int line = 1;

error (msg)
char *msg;
{
	fprintf (stderr, "cc1: %d: error: %s\n", line, msg);
	return exit (1), -1;
}

// LEXER

enum {
	TK_IDENT = 128,
	KW_INT,
	KW_STATIC,
	KW_EXTERN,
	KW_AUTO,
	KW_TYPEDEF,
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
	if (peekdch == EOF)
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
	char *s;
	int ch;

begin:
	while (isspace (peekch ()))
		nextch ();

	ch = peekch ();

	if (isdigit (ch)) {
		return error ("TODO: integers");
	} else if (isname (ch)) {
		s = lval.ident;

		for (i = 0; isname (peekch ());) {
			ch = nextch ();
			if (i < IDENT_LEN)
				s[i++] = ch;
		}
		s[i] = '\0';

		if (strcmp (s, "int") == 0) {
			return KW_INT;
		} else if (strcmp (s, "auto") == 0) {
			return KW_AUTO;
		} else if (strcmp (s, "static") == 0) {
			return KW_STATIC;
		} else if (strcmp (s, "extern") == 0) {
			return KW_EXTERN;
		} else if (strcmp (s, "typedef") == 0) {
			return KW_TYPEDEF;
		} else {
			return TK_IDENT;
		}
	} else switch (ch) {
	case '(':
	case ')':
	case '{':
	case '}':
	case '=':
	case ';':
	case ',':
		return nextch ();
	case EOF:
		return TK_EOF;
	case '/':
		nextch ();
		ch = peekch ();
		if (ch == '/') {
			nextch ();
			while (nextch () != '\n');
			goto begin;
		}
		// fallthrough
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

expect (exp)
{
	if (next () != exp)
		error ("syntax error");
	return 0;
}

match (tk)
{
	return peek () == tk ? next (), 1 : 0;
}

// AST DATA TYPES

enum sc_class {
	SC_NONE,
	SC_AUTO,
	SC_STATIC,
	SC_EXTERN,
	SC_TYPEDEF,
};

enum dtype_type {
	DT_INT,
};

struct dtype {
	enum dtype_type type;
};

enum decl_type {
	DL_VAR,
	DL_FUNC,
};

struct decl {
	struct decl *next;
	enum sc_class sc;
	struct dtype *dt;
	char *name;
};

// PARSER

#define new(T) ((T *)calloc (1, sizeof (T)))
#define pnew(T) (new (T))

dtype (dt)
struct dtype *dt;
{
	switch (peek ()) {
	case KW_INT:
		next ();
		dt->type = DT_INT;
		return 1;
	default:
		return 0;
	}
}

body ();

decl (global)
{
	enum sc_class sc;
	struct dtype dt;
	char ident[IDENT_LEN + 1];
	int ii = 0, first = 1;

	switch (peek ()) {
	case KW_AUTO:
		next ();
		if (global)
			error ("storage class `auto` not permitted in global scope");
		sc = SC_AUTO;
		break;
	case KW_STATIC:
		next ();
		sc = SC_STATIC;
		break;
	case KW_EXTERN:
		next ();
		sc = SC_EXTERN;
		break;
	case KW_TYPEDEF:
		next ();
		sc = SC_TYPEDEF;
		break;
	default:
		sc = SC_NONE;
		break;
	}

	if (!dtype (&dt)) {
		if (global || sc != SC_NONE) {
			dt.type = DT_INT;	// implicit int
			ii = 1;
		} else {
			return 0;
		}
	}

	do {
		if (!match (TK_IDENT))
			return first && ii ? 0 : error ("expected identifier");

		memcpy (ident, lval.ident, IDENT_LEN + 1);

		switch (peek ()) {
		case '(':
			next ();
			// TODO: params
			expect (')');

			if (!match ('{'))
				break;

			if (!first || !global)
				error ("syntax error");

			body ();

			expect ('}');
			return 1;
		case '[':
			return error ("TODO: arrays");
		case '=':
			next ();
			error ("TODO: init expr");
			break;
		case ';':
			next ();
			return 1;
		case ',':
			break;
		default:
			return error ("expected declaration");
		}
		first = 0;
	} while (match (','));

	expect (';');

	return 1;
}

body (void)
{
	while (decl (0));

}

parse (void)
{
	while (peek () != TK_EOF) {
		if (!decl (1))
			return error ("expected declaration");
	}

	return 0;
}

// MAIN

main (void)
{
	parse ();
	return 0;
}
