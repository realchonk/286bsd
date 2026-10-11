#include <stdint.h>
#include <stdio.h>

#include "lex.h"

static char *argv0;

main(argc, argv)
	char *argv[];
{
	int tok;

	argv0 = argv[0];

	while (tok = yylex(), tok != 0)
		printf("%3d %s\n", tok, yytext);
}

void
die(reason)
	char *reason;
{
	fprintf(stderr, "%s: %s\n", argv0, reason);
	exit(EXIT_FAILURE);
}
