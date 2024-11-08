

main ()
{
	extern yyparse (), yyinit ();
	yyinit ();
	return yyparse ();
}
