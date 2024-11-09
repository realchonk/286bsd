#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cc1.h"

int reg;

eval_expr (e)
struct expr *e;
{
	switch (e->type) {
	case EX_INT:
		return e->i;
	}
}

print_dt (dt)
struct dtype *dt;
{
	switch (dt->type) {
	case DT_CHAR:
	case DT_SCHAR:
	case DT_UCHAR:
		printf ("byte");
		break;
	case DT_SHORT:
	case DT_USHORT:
	case DT_INT:
	case DT_UINT:
		printf ("word");
		break;
	case DT_LONG:
	case DT_ULONG:
		printf ("dword");
		break;
	case DT_PTR:
		printf ("*");
		print_dt (dt->inner);
		break;
	case DT_FUNC:
		// TODO
		abort ();
		break;
	}
	return 0;
}

gen_decl (sym)
struct symbol *sym;
{
	int pub, ext = 0;

	switch (sym->sc) {
	case SC_EXTERN:
		ext = 1;
		// fallthrough
	case SC_NONE:
		pub = 1;
		break;
	case SC_STATIC:
		pub = 0;
		break;
	case SC_AUTO:
	case SC_TYPEDEF:
		abort ();
	}

	if (pub)
		printf ("pub ");

	if (ext)
		printf ("extern ");

	if (sym->dt->type == DT_FUNC) {
		printf ("fn %s (): ", sym->ident);
		print_dt (sym->dt->inner);
		printf (";\n");
	} else {
		printf ("var %s: ", sym->ident);
		print_dt (sym->dt);

		switch (sym->init.type) {
		case IN_EX:
			printf (" = %d", eval_expr (sym->init.ex));
			// fallthrough
		case IN_NONE:
			printf (";\n");
			break;
		}
	}

	return 0;
}

struct symbol *
get_var (name)
char *name;
{
	struct symbol *sym;

	for (sym = fscope.vars; sym != NULL; sym = sym->next) {
		if (strcmp (name, sym->ident) == 0)
			return sym;
	}

	return NULL;
}

gen_expr (dt, e)
struct dtype *dt;
struct expr *e;
{
	int r;

	switch (e->type) {
	case EX_INT:
		r = reg++;
		printf ("\tlet $%d: ", r);
		print_dt (dt);
		printf (" = %d;\n", e->i);
		break;
	}

	return r;
}

gen_fn (fn)
struct function *fn;
{
	struct symbol *sym;
	int r;

	for (sym = fn->ahead; sym != NULL; sym = sym->xnext)
		printf ("# $%d: %s\n", sym->reg, sym->ident);

	if (fn->pub)
		printf ("pub ");

	printf ("fn %s (", fn->ident);

	sym = fn->ahead;
	if (sym != NULL) {
		printf ("$%d: ", sym->reg);
		print_dt (sym->dt);
		for (sym = sym->xnext; sym != NULL; sym = sym->xnext) {
			printf (", $%d: ", sym->reg);
			print_dt (sym->dt);
		}
	}

	printf ("): ");
	print_dt (fn->dt);
	printf (" {\n");

	for (sym = fn->ahead; sym != NULL; sym = sym->xnext) {
		r = reg++;
		printf ("\t# $%d: &%s\n", r, sym->ident);
		printf ("\tlet $%d: ^", r);
		print_dt (sym->dt);
		printf (" = alloca 1;\n");
		printf ("\twrite $%d, $%d;\n\n", r, sym->reg);
		sym->reg = r;
	}

	for (sym = fn->vhead; sym != NULL; sym = sym->xnext) {
		printf ("\t# $%d: &%s\n", sym->reg, sym->ident);
		printf ("\tlet $%d: ^", sym->reg);
		print_dt (sym->dt);
		printf (" = ");

		switch (sym->sc) {
		case SC_AUTO:
			printf ("alloca 1");
			break;
		case SC_EXTERN:
			printf ("lookup %s", sym->ident);
			break;
		case SC_STATIC:
			printf ("lookup __fn_%s_%s", fn->ident, sym->ident);
			break;
		case SC_NONE:
		case SC_TYPEDEF:
			abort ();
		}
		printf (";\n");

		if (sym->sc == SC_AUTO && sym->init.type != IN_NONE) {
			r = gen_expr (sym->dt, sym->init.ex);
			printf ("\twrite $%d, $%d;\n", sym->reg, r);
		}
		printf ("\n");
	}

	// TODO: define static variables

	printf ("}\n\n");

	return 0;
}
