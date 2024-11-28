#include <stdlib.h>
#include <assert.h>
#include <err.h>
#include "cc1.h"

dt_is_eq (a, b)
struct dtype *a, *b;
{
	if (a->type != b->type)
		return 0;

	switch (a->type) {
	case DT_PTR:
	case DT_FUNC:
		return dt_is_eq (a->inner, b->inner);
	default:
		return 1;
	}
}

is_integral (type)
enum dtype_type type;
{
	switch (type) {
	case DT_CHAR:
	case DT_SCHAR:
	case DT_UCHAR:
	case DT_SHORT:
	case DT_USHORT:
	case DT_INT:
	case DT_UINT:
	case DT_LONG:
	case DT_ULONG:
		return 1;
	case DT_PTR:
	case DT_FUNC:
		return 0;
	}
}

as_base (type)
enum dtype_type type;
{
	switch (type) {
	case DT_CHAR:
	case DT_SCHAR:
	case DT_UCHAR:
		return DT_CHAR;
	case DT_SHORT:
	case DT_USHORT:
		return DT_SHORT;
	case DT_INT:
	case DT_UINT:
		return DT_INT;
	case DT_LONG:
	case DT_ULONG:
		return DT_LONG;
	case DT_PTR:
	case DT_FUNC:
		return type;
	}
}

enum irc_type
as_irc_type (type)
enum dtype_type type;
{
	switch (type) {
	case DT_CHAR:
	case DT_SCHAR:
	case DT_UCHAR:
		return IT_BYTE;
	case DT_SHORT:
	case DT_USHORT:
	case DT_INT:
	case DT_UINT:
		return IT_WORD;
	case DT_LONG:
	case DT_ULONG:
		return IT_DWORD;
	default:
		abort ();
	}
}

irc_size (type)
enum irc_type type;
{
	switch (type) {
	case IT_BYTE:
		return 1;
	case IT_WORD:
		return 2;
	case IT_DWORD:
		return 4;
	case IT_QWORD:
		return 8;
	}
}

is_signed (type)
enum dtype_type type;
{
	switch (type) {
	case DT_CHAR:
	case DT_SCHAR:
	case DT_SHORT:
	case DT_INT:
	case DT_LONG:
		return 1;
	case DT_UCHAR:
	case DT_USHORT:
	case DT_UINT:
	case DT_ULONG:
		return 0;
	default:
		abort ();
	}
}

/* checks if a is a bigger type than b */
cmp_size (a, b)
enum dtype_type a, b;
{
	int sa, sb;

	assert (is_integral (a));
	assert (is_integral (b));
	sa = irc_size (as_irc_type (a));
	sb = irc_size (as_irc_type (b));

	if (sa < sb) {
		return -1;
	} else if (sa > sb) {
		return 1;
	} else {
		return 0;
	}
}

struct expr *
cast (e, dt, imp)
struct expr *e;
struct dtype *dt;
{
	struct expr *n;

	if (dt_is_eq (e->dt, dt))
		return e;

	n = new (struct expr);
	n->type = EX_CAST;
	n->cast.dt = dt;
	n->cast.e = e;
	return n;
}
