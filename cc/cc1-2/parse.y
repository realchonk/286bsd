%{
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cc1.h"
%}

%union {
	struct declaration d;
	struct decl_spec ds;
	struct type_spec ts;
	struct declarator dl;
	struct initializer in;
	struct ident_list *il;
	struct decl_init *di;
	struct decls_list *dd;
	struct expr *ex;
	int i;
};

%token INTEGER
%token IDENT
%token TYPEIDENT

%token SIGNED
%token UNSIGNED
%token CHAR
%token SHORT
%token INT
%token LONG
%token STATIC
%token EXTERN
%token AUTO
%token TYPEDEF

%type <d> declaration
%type <i> sc_spec basic_type
%type <ts> type_spec
%type <ds> decl_spec decl_specs
%type <dl> declarator declarator1
%type <in> initializer
%type <di> init_decl decl_list
%type <ex> expression
%type <il> ident_list
%type <dd> decls_list

%%

file		: file declaration		{ declare (&$2); }
      		| file function
      		|
		;

function	: decl_specs declarator decls_list fbody
	 	| declarator decls_list fbody
	 	;

ident_list	: ident_list ',' IDENT		{ $$ = il_append ($1, lval.ident); }
	   	| IDENT				{ $$ = il_append (NULL, lval.ident); }
		;

fbody		: '{' '}'
       		;

decls_list	: decls_list declaration	{ $$ = dd_append ($1, &$2); }
	   	| declaration			{ $$ = dd_append (NULL, &$1); }
		;

declaration	: decl_specs decl_list ';'	{ $$.sc = $1.sc; $$.ts = $1.ts; $$.head = $2; }
	    	| decl_specs ';'		{ $$.sc = $1.sc; $$.ts = $1.ts; $$.head = NULL; }
		;

decl_specs	: decl_specs decl_spec		{ ds_comb (&$$, &$1, &$2); }
	   	| decl_spec			{ $$ = $1; }
		;

decl_spec	: type_spec			{ $$.sc = SC_NONE; $$.ts = $1; }
	  	| sc_spec			{ $$.sc = $1; $$.ts.type = TS_NONE; }
		;

sc_spec		: AUTO				{ $$ = SC_AUTO; }
       		| EXTERN			{ $$ = SC_EXTERN; }
		| STATIC			{ $$ = SC_STATIC; }
		| TYPEDEF			{ $$ = SC_TYPEDEF; }
		;

type_spec	: basic_type			{ $$.type = TS_BASIC; $$.bt = $1; }
	  	| TYPEIDENT			{ $$.type = TS_TYPEIDENT; $$.sym = lval.sym; }
	  	;
       	
basic_type	: CHAR				{ $$ = BT_CHAR; }
	   	| INT				{ $$ = BT_SHORT; }
	   	| SHORT				{ $$ = BT_INT; }
		| LONG				{ $$ = BT_LONG; }
		| SIGNED			{ $$ = BT_SIGNED; }
		| UNSIGNED			{ $$ = BT_UNSIGNED; }
	   	;

decl_list	: init_decl ',' decl_list	{ $$ = $1; $$->next = $3; }
	  	| init_decl			{ $$ = $1; }
		;

init_decl	: declarator			{ $$ = new_decl (&$1); $$->in.type = IN_NONE; }
	  	| declarator initializer	{ $$ = new_decl (&$1); $$->in = $2; }
		;

declarator	: declarator '(' ')'		{ $$.type = DL_FUNC; $$.dl = new (struct declarator); *$$.dl = $1; }
	   	| declarator '(' ident_list ')'	{ $$.type = DL_FUNCDEF; $$.def.dl = new (struct declarator); *$$.def.dl = $1; $$.def.il = $3; }
	   	//| declarator '[' const_expr ']'
	   	| declarator1			{ $$ = $1; }
		;

declarator1	: IDENT				{ $$.type = DL_IDENT; cpident ($$.ident, lval.ident); }
	    	| '*' declarator1		{ $$.type = DL_PTR; $$.dl = new (struct declarator); *$$.dl = $2; }
		| '(' declarator ')'		{ $$ = $2; }
		;

initializer	: '=' expression		{ $$.type = IN_EX; $$.ex = $2; }
	    	;

expression	: INTEGER			{ $$ = new (struct expr); $$->type = EX_INT; $$->i = lval.i; }
	   	;


%%

struct ident_list *
il_append (head, ident)
struct ident_list *head;
char *ident;
{
	struct ident_list *n, *i;

	n = new (struct ident_list);
	n->next = NULL;
	cpident (n->ident, ident);

	if (head == NULL) {
		head = n;
	} else {
		for (i = head; i->next != NULL; i = i->next);
		i->next = n;
	}

	return head;
}

struct decls_list *
dd_append (head, d)
struct decls_list *head;
struct declaration *d;
{
	struct decls_list *n, *i;

	n = new (struct decls_list);
	n->next = NULL;
	n->d = *d;

	if (head == NULL) {
		head = n;
	} else {
		for (i = head; i->next != NULL; i = i->next);
		i->next = n;
	}

	return head;
}

struct decl_init *
new_decl (dl)
struct declarator *dl;
{
	struct decl_init *di = new (struct decl_init);
	di->next = NULL;
	di->dl = *dl;
	return di;
}

ts_comb (out, ts)
struct type_spec *out, *ts;
{
	if (ts->type == TS_NONE) {
	} else if (out->type == TS_NONE) {
		*out = *ts;
	} else	if (out->type == TS_BASIC && ts->type == TS_BASIC) {
		out->bt |= ts->bt;
	} else {
		yyerror ("invalid type combination");
	}

	return 0;
}

ds_comb (out, ds1, ds2)
struct decl_spec *out, *ds1, *ds2;
{
	out->sc = ds1->sc;
	if (out->sc == SC_NONE) {
		out->sc = ds2->sc;
	} else if (ds2->sc != SC_NONE && ds1->sc != ds2->sc) {
		yyerror ("cannot combine storage class specifiers");
	}
	
	out->ts.type = TS_NONE;

	ts_comb (&out->ts, &ds1->ts);
	ts_comb (&out->ts, &ds2->ts);

	return 0;
}

struct dtype *
resolve_type (ts)
struct type_spec *ts;
{
	struct dtype *dt;
	int us, bt;

	switch (ts->type) {
	case TS_NONE:
		dt = new (struct dtype);
		dt->type = DT_INT;
		break;
	case TS_BASIC:
		dt = new (struct dtype);
		bt = ts->bt;
		if ((bt & (BT_SIGNED | BT_UNSIGNED)) == (BT_SIGNED | BT_UNSIGNED))
			yyerror ("cannot combine signed and unsigned");
		us = (bt & BT_UNSIGNED) == BT_UNSIGNED;

		if (!!(bt & BT_CHAR) + !!(bt & BT_SHORT) + !!(bt & BT_LONG) > 1)
			yyerror ("cannot combine char, short and long");

		if (bt & BT_CHAR) {
			if (bt & BT_SIGNED) {
				dt->type = DT_SCHAR;
			} else if (bt & BT_UNSIGNED) {
				dt->type = DT_UCHAR;
			} else {
				dt->type = DT_CHAR;
			}
		} else if (bt & BT_SHORT) {
			dt->type = us ? DT_USHORT : DT_SHORT;
		} else if (bt & DT_LONG) {
			dt->type = us ? DT_ULONG : DT_LONG;
		} else {
			dt->type = us ? DT_UINT : DT_INT;
		}
		break;
	case TS_TYPEIDENT:
		dt = ts->sym->dt;
		break;
	}

	return dt;
}

struct dtype *
resolve_dl (base, dl, ident)
struct dtype *base;
struct declarator *dl;
char **ident;
{
	struct dtype *dt;

	switch (dl->type) {
	case DL_IDENT:
		*ident = dl->ident;
		dt = base;
		break;
	case DL_FUNC:
		dt = new (struct dtype);
		dt->type = DT_FUNC;
		dt->inner = resolve_dl (base, dl->dl, ident);
		break;
	case DL_PTR:
		dt = new (struct dtype);
		dt->type = DT_PTR;
		dt->inner = resolve_dl (base, dl->dl, ident);
		break;
	case DL_FUNCDEF:
		yyerror ("invalid function definition");
	}

	return dt;
}

declare (d)
struct declaration *d;
{
	struct decl_init *di;
	struct dtype *base, *dt;
	struct symbol *sym;
	char *ident;
	int pub;

	base = new (struct dtype);
	base = resolve_type (&d->ts);

	for (di = d->head; di != NULL; di = di->next) {
		dt = resolve_dl (base, &di->dl, &ident);

		switch (d->sc) {
		case SC_AUTO:
			yyerror ("invalid storage class at top level: auto");
			break;
		case SC_NONE:
			pub = 1;
			goto common;
		case SC_STATIC:
			pub = 0;
			goto common;
		case SC_EXTERN:
			pub = 1;
		common:
			printf ("T %s;\n", ident);
			// TODO
			break;
		case SC_TYPEDEF:
			if (di->in.type != IN_NONE)
				yyerror ("cannot assign to typedef");
			sym = new (struct symbol);
			sym->next = scope->typedefs;
			sym->dt = dt;
			cpident (sym->ident, ident);
			scope->typedefs = sym;
			break;
		}
	}

	return 0;
}
