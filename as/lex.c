#include <ctype.h>
#include <stdio.h>

#include "y.tab.h"
#include "lex.h"

#define TOKMAX 100

int lineno = 1, yyleng;
unsigned long tokval;
char yytext[TOKMAX];

int
input()
{
	int c;

	c = getchar();

	if (c == EOF)
		c = 0;

	return (c);
}

void
unput(c)
{
	ungetc(c, stdin);
}

int
yylex()
{
	int c;

	yyleng = 0;

again:	while (c = input(), isblank(c))
		;

	yytext[yyleng++] = c;

	/* name? */
	if (c == '.' || c == '_' || isalpha(c)) {
		do {
			c = input();
			yytext[yyleng++] = c;
		} while (c == '.' || c == '_' || isalnum(c));

		unput(c);
		yytext[--yyleng] = '\0';

		return (NAME);
	}

	/* hexadecimal or octal number? */
	if (c == '0') {
		tokval = 0;
		c = input();
		yytext[yyleng++] = c;

		/* hexadecimal number? */
		if (c == 'x' || c == 'X') {
			int shift = 0;

			while (c = input(), isxdigit(c)) {
				yytext[yyleng++] = c;
				c = tolower(c);

				if (c > '9')
					tokval += (c - 'a' + 10) << shift;
				else
					tokval += (c - '0') << shift;

				shift += 4;
			}
		} else if (isdigit(c)) {
			/* octal number */
			int shift = 0;

			do {
				tokval += (c - '0') << shift;
				shift += 3;

				c = input();
				yytext[yyleng++] = c;
			} while (isdigit(c));
		}

		unput(c);
		yytext[--yyleng] = '\0';

		return (CONSTANT);
	}	

	/* decimal number? */
	if (isdigit(c)) {
		long val = 0;
		long exp = 1;

		do {
			val += (c - '0') * exp;
			exp *= 10;

			c = input();
			yytext[yyleng++] = c;
		} while (isdigit(c));

		unput(c);
		yytext[--yyleng] = '\0';

		return (CONSTANT);
	}

	if (c == '\\') {
		c = input();

		/* escaped newline? */
		if (c == '\n') {
			lineno++;
			goto again;
		}

		unput(c);
	}

	if (c == '\n')
		lineno++;

	/* other */
	yytext[yyleng] = '\0';

	return (c);
	
}
