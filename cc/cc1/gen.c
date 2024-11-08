#include <stdlib.h>
#include <stdio.h>
#include "cc1.h"

gen_decl (d, pub)
struct declaration *d;
{
	return 0;
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

print_arg (a)
struct arg *a;
{
	// TODO: use registers, like $0, $1, etc
	printf ("%s: ", a->ident);
	print_dt (a->dt);
	return 0;
}

gen_fn (fn)
struct function *fn;
{
	size_t i;

	if (fn->pub)
		printf ("pub ");

	printf ("fn %s (", fn->ident);

	if (fn->args[0].ident[0] != '\0') {
		print_arg (&fn->args[0]);

		for (i = 1; fn->args[i].ident[0] != '\0'; ++i) {
			printf (", ");
			print_arg (&fn->args[i]);
		}
	}

	printf ("): ");
	print_dt (fn->dt);
	printf (" {\n");

	// TODO: allocate vars

	printf ("}\n");

	return 0;
}
