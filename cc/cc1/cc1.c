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
	case '*':
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
	DT_FUNC,
	DT_PTR,
};

struct dtype {
	enum dtype_type type;
	union {
		struct dtype *base;
	};
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
	struct dtype *base;

	switch (peek ()) {
	case KW_INT:
		next ();
		dt->type = DT_INT;
		break;
	default:
		return 0;
	}

	while (peek () == '*') {
		next ();
		base = new (struct dtype);
		*base = *dt;
		dt->type = DT_PTR;
		dt->base = base;
	}

	return 1;
}

enum sc_class
sclass (void)
{
	switch (peek ()) {
	case KW_AUTO:
		return next(), SC_AUTO;
	case KW_STATIC:
		return next(), SC_STATIC;
	case KW_EXTERN:
		return next(), SC_EXTERN;
	case KW_TYPEDEF:
		return next(), SC_TYPEDEF;
	default:
		return SC_NONE;
	}
}

parse_func (sc, dt, ident)
enum sc_class sc;
struct dtype *dt;
char *ident;
{

	return 0;
}

parse_decl (void)
{
	extern gen_decl (), gen_func ();
	enum sc_class sc;
	struct dtype dt, *base;
	char ident[IDENT_LEN + 1];
	int ii = 0, first = 1;

	sc = sclass ();
	if (sc == SC_AUTO)
		error ("storage class `auto` not permitted in global scope");

	if (!dtype (&dt)) {
		dt.type = DT_INT;
		ii = 1;
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

			if (!match ('{')) {
				expect (';');
				base = new (struct dtype);
				*base = dt;
				dt.type = DT_FUNC;
				dt.base = base;
				gen_decl (sc, &dt, ident);
				return 1;
			}

			if (!first)
				error ("syntax error");

			parse_func (sc, &dt, ident);

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
			gen_decl (sc, &dt, ident);
			return 1;
		case ',':
			break;
		default:
			return error ("expected declaration");
		}
		first = 0;
	} while (match (','));
	error ("unreachable reached");
	return 0;
}

parse (void)
{
	while (peek () != TK_EOF) {
		if (!parse_decl ())
			return error ("expected declaration");
	}

	return 0;
}

// GEN

gen_decl (sc, dt, ident)
enum sc_class sc;
struct dtype *dt;
char *ident;
{
	return 0;
}

// MAIN

main (void)
{
	parse ();
	return 0;
}
