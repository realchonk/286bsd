%{
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cc1.h"
%}

%union {
	struct function fn;
	struct declaration d;
	struct decl_spec ds;
	struct type_spec ts;
	struct declarator dl;
	struct initializer in;
	struct ident_list *il;
	struct decl_init *di;
	struct decls_list *dd;
	struct expr *ex;
	struct stmt *st;
	int i;
	
	struct {
		struct decls_list *dd;
		struct stmt **st;
	} fb;
	struct list {
		void **ptr;
		size_t cap, len;
	} lst;
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
%type <st> statement
%type <il> ident_list
%type <dd> decls_list decls_list_opt
%type <fb> fbody
%type <fn> function
%type <lst> stmts

%%

file		: file declaration		{ declare (&$2); }
      		| file function			{ define (&$2); }
      		|
		;

function	: decl_specs declarator decls_list fbody	{ fn (&$$, &$1, &$2, $3, $4.dd, $4.st); }
	 	| declarator decls_list fbody			{ fn (&$$, NULL, &$1, $2, $3.dd, $3.st); }
	 	;

ident_list	: ident_list ',' IDENT		{ $$ = il_append ($1, lval.ident); }
	   	| IDENT				{ $$ = il_append (NULL, lval.ident); }
		;

fbody		: '{' decls_list_opt stmts '}'	{ $$.dd = $2; $$.st = lst_fin (&$3); }
       		;

stmts		: stmts statement		{ $$ = $1; lst_append (&$$, $2); }
       		|				{ lst_new (&$$); }
		;

decls_list_opt	: decls_list			{ $$ = $1; }
	       	|				{ $$ = NULL; }
		;

decls_list	: decls_list declaration	{ $$ = dd_append ($1, &$2); }
	   	| declaration			{ $$ = dd_append (NULL, &$1); }
		;

declaration	: decl_specs decl_list ';'	{ decl (&$$, &$1, $2); }
	    	| decl_specs ';'		{ decl (&$$, &$1, NULL); }
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
	   	| SHORT				{ $$ = BT_SHORT; }
	   	| INT				{ $$ = BT_INT; }
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

statement	: expression ';'		{ $$ = new (struct stmt); $$->type = ST_EXPR; $$->ex = $1; }
	  	| IDENT ':'			{ $$ = new (struct stmt); $$->type = ST_LABEL; cpident ($$->ident, lval.ident); }
	  	| ';'				{ $$ = new (struct stmt); $$->type = ST_NULL; }
	  	;

expression	: '(' expression ')'		{ $$ = new_expr (EX_PAR, $2->dt); $$->e = $2; }
	   	| INTEGER			{ $$ = new_expr (EX_INT, &default_int); $$->i = lval.i; }
	   	;


%%

static struct dtype default_int = { .type = DT_INT };

struct expr *
new_expr (type, dt)
struct dtype *dt;
{
	struct expr *e;

	e = new (struct expr);
	e->type = type;
	e->dt = dt;
	
	return e;
}

lst_new (lst)
struct list *lst;
{
	lst->len = 0;
	lst->cap = 2;
	lst->ptr = calloc (lst->cap + 1, sizeof (void *));
	return 0;
}

lst_append (lst, x)
struct list *lst;
void *x;
{
	if (lst->len == lst->cap) {
		lst->cap *= 2;
		lst->ptr = reallocarray (lst->ptr, lst->cap + 1, sizeof (void *));
	}
	lst->ptr[lst->len++] = x;
	return 0;
}

void *
lst_fin (lst)
struct list *lst;
{
	lst->ptr[lst->len] = NULL;
	return lst->ptr;
}

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
		dt = &default_int;
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
		} else if (bt & BT_LONG) {
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
		dt = NULL;
		break;
	}

	return dt;
}

decl (d, ds, di)
struct declaration *d;
struct decl_spec *ds;
struct decl_init *di;
{
	d->sc = ds->sc;
	d->ts = ds->ts;
	d->head = di;
	return 0;
}

struct symbol *
find_sym (head, ident)
struct symbol *head;
char *ident;
{
	struct symbol *sym;

	for (sym = head; sym != NULL; sym = sym->next) {
		if (strcmp (ident, sym->ident) == 0)
			return sym;
	}
	
	return NULL;
}

fn (fn, ds, dl, args, vars, stmts)
struct function *fn;
struct decl_spec *ds;
struct declarator *dl;
struct decls_list *args, *vars;
struct stmt **stmts;
{
	struct symbol *sym, *atail, *vtail;
	struct decl_init *di;
	struct decls_list *dd;
	struct ident_list *il;
	struct dtype *base, *dt;
	char *ident;

	if (dl->type != DL_FUNCDEF || dl->def.dl->type != DL_IDENT)
		yyerror ("invalid function definition");
	cpident (fn->ident, dl->def.dl->ident);

	if (ds != NULL) {
		fn->dt = resolve_type (&ds->ts);
		switch (ds->sc) {
		case SC_NONE:
		case SC_EXTERN:
			fn->pub = 1;
			break;
		case SC_STATIC:
			fn->pub = 0;
			break;
		default:
			yyerror ("invalid storage class for function");
		}
	} else {
		fn->dt = &default_int;
		fn->pub = 1;
	}

	reg = 0;
	fscope.typedefs = fscope.vars = NULL;
	fn->ahead = atail = NULL;
	fn->vhead = vtail = NULL;
	fn->body = stmts;

	// declare arguments as implicit int
	for (il = dl->def.il; il != NULL; il = il->next) {
		if (find_sym (fscope.vars, il->ident) != NULL)
			yyerror ("redeclaration of argument");
		sym = new (struct symbol);
		sym->next = fscope.vars;
		sym->xnext = NULL;
		sym->dt = &default_int;
		cpident (sym->ident, il->ident);
		sym->sc = SC_NONE;
		sym->reg = reg++;
		sym->init.type = IN_NONE;

		fscope.vars = sym;
	
		if (fn->ahead == NULL) {
			fn->ahead = sym;
		} else {
			atail->xnext = sym;
		}
		atail = sym;
	}

	// set the correct data types for arguments
	for (dd = args; dd != NULL; dd = dd->next) {
		if (dd->d.sc != SC_NONE)
			yyerror ("invalid storage class for function argument");
		base = resolve_type (&dd->d.ts);
		for (di = dd->d.head; di != NULL; di = di->next) {
			dt = resolve_dl (base, &di->dl, &ident);
			sym = find_sym (fscope.vars, ident);
			if (sym == NULL)
				yyerror ("defining type for undeclared argument");
			if (sym->dt != &default_int)
				yyerror ("redefinition of argument type");
			sym->dt = dt;
			sym->init = di->in;
		}
	}

	// declare variables
	for (dd = vars; dd != NULL; dd = dd->next) {
		base = resolve_type (&dd->d.ts);
		for (di = dd->d.head; di != NULL; di = di->next) {
			dt = resolve_dl (base, &di->dl, &ident);
			if (find_sym (fscope.vars, ident) != NULL || find_sym (fscope.typedefs, ident) != NULL)
				yyerror ("redeclaration of variable or typedef");

			sym = new (struct symbol);
			sym->next = dd->d.sc == SC_TYPEDEF ? fscope.typedefs : fscope.vars;
			sym->xnext = NULL;
			sym->dt = resolve_dl (base, &di->dl, &ident);
			cpident (sym->ident, ident);
			sym->sc = dd->d.sc;
			sym->init = di->in;
			sym->reg = reg++;
			
			switch (sym->sc) {
			case SC_NONE:
				sym->sc = SC_AUTO;
			case SC_AUTO:
			case SC_EXTERN:
			case SC_STATIC:
				if (fn->vhead == NULL) {
					fn->vhead = sym;
				} else {
					vtail->xnext = sym;
				}
				fscope.vars = sym;
				vtail = sym;
				break;
			case SC_TYPEDEF:
				if (sym->init.type != IN_NONE)
					yyerror ("invalid assignment to typedef");
				fscope.typedefs = sym;
				break;
			}
			
		}
	}

	return 0;
}

declare (d)
struct declaration *d;
{
	extern gen_decl ();
	struct decl_init *di;
	struct dtype *base;
	struct symbol *sym;
	char *ident;

	base = new (struct dtype);
	base = resolve_type (&d->ts);

	for (di = d->head; di != NULL; di = di->next) {
		sym = new (struct symbol);
		sym->sc = d->sc;
		sym->dt = resolve_dl (base, &di->dl, &ident);
		sym->init = di->in;
		cpident (sym->ident, ident);

		switch (d->sc) {
		case SC_AUTO:
			yyerror ("invalid storage class at top level: auto");
			break;
		case SC_EXTERN:
			if (sym->init.type != IN_NONE)
				yyerror ("extern variable with initializer");
		case SC_NONE:
		case SC_STATIC:
			sym->next = gscope.vars;
			gscope.vars = sym;
			gen_decl (sym);
			break;
		case SC_TYPEDEF:
			if (di->in.type != IN_NONE)
				yyerror ("cannot assign to typedef");
			sym->next = gscope.typedefs;
			gscope.typedefs = sym;
			break;
		}
	}

	return 0;
}

define (fn)
struct function *fn;
{
	extern gen_fn ();
	gen_fn (fn);
	return 0;
}
