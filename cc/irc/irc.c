#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

// use an arena to speed up memory allocations
#define ENABLE_ARENA 1

// enable debugging prints
#define ENABLE_DEBUG 1

// maximum number of registers per function
#define NREGS 128

// maximum number of arguments for a function/call
#define MAXARGS 8

// maximum number of PHI values
#define MAXPHI 8

int ignore_nan = 0;

// ERROR HANDLING

int linenum;

error (msg)
char *msg;
{
	fprintf (stderr, "irc: %d: error: %s\n", linenum, msg);
	return exit (1), -1;
}

// MEMORY

#if ENABLE_ARENA

# if ENABLE_DEBUG
size_t total_alloc = 0;
# endif

char arena[32768];
size_t alen = 0;

void *
alloc (num)
size_t num;
{
	void *ptr;

	// TODO: consider returning heap memory
	if (alen + num > sizeof (arena))
		error ("arena out of memory");

	ptr = (void *)((size_t)arena + alen);
	alen += num;
# if ENABLE_DEBUG
	total_alloc += num;
# endif // ENABLE_DEBUG
	memset (ptr, 0, num);
	return ptr;
}

reset (void)
{
# if ENABLE_DEBUG
	fprintf (stderr, "irc: debug: arena usage: %zu/%zu bytes\n", alen, sizeof (arena));
# endif
	alen = 0;
	return 0;
}
#else
# define alloc(num) calloc (1, (num))
# define reset()
#endif

// LEXER

enum {
	TK_INT = 128,
	TK_FLOAT,
	TK_REG,
	TK_IDENT,
	TK_STRING,
	TK_EOF,
};

union {
	long i;
	char *s;
	double f;
} lval;

int peekd = EOF;

readch (void)
{
	int ch;
	ch = getchar ();
	if (ch == '\n')
		++linenum;
	return ch;
}
peekch (void)
{
	if (peekd == EOF)
		peekd = readch ();
	return peekd;
}

nextch (void)
{
	int ch;
	if (peekd != EOF) {
		ch = peekd;
		peekd = EOF;
	} else {
		ch = readch ();
	}
	return ch;
}

parse_int (void)
{
	long i = 0;

	while (isdigit (peekch ()))
		i = i * 10 + (nextch () - '0');

	lval.i = i;
	return 0;
}

parse_int_or_float (int neg)
{
	long i = 0;
	double f, s;

	while (isdigit (peekch ()))
		i = i * 10 + (nextch () - '0');

	if (peekch () != '.') {
		lval.i = neg ? -i : i;
		return TK_INT;
	}

	f = i;
	s = 0.1;
	nextch ();

	while (isdigit (peekch ())) {
		f = f + s * (nextch () - '0');
		s /= 10.0;
	}

	lval.f = neg ? -f : f;

	return TK_FLOAT;
}

isname (ch)
{
	return isalnum (ch) || ch == '_';
}

lex (void)
{
	size_t cap, len;
	char *s, *t;
	int ch;

begin:
	while (isspace (peekch ()))
		nextch ();

	ch = peekch ();

	if (isdigit (ch)) {
		return parse_int_or_float (0);
	} else if (isalpha (ch) || ch == '_') {
		cap = 10;
		len = 0;
		s = alloc (cap + 1);

		while (isname (peekch ())) {
			if (len == cap) {
				cap *= 2;
				t = alloc (cap + 1);
				memcpy (t, s, len);
			}
			s[len++] = nextch ();
		}
		s[len] = '\0';
		lval.s = s;
		return TK_IDENT;
	} else switch (ch) {
	case '$':
		nextch ();
		parse_int ();
		return TK_REG;
	case '-':
		nextch ();
		return parse_int_or_float (1);

	case '"':
		nextch ();
		cap = 10;
		len = 0;
		s = alloc (cap + 1);

		// TODO: implement escape sequences
		while ((ch = nextch ()) != '"') {
			if (len == cap) {
				cap *= 2;
				t = alloc (cap + 1);
				memcpy (t, s, len);
			}
			s[len++] = ch;
		}

		s[len] = '\0';
		lval.s = s;
		return TK_STRING;
	case '(':
	case ')':
	case '{':
	case '}':
	case ':':
	case ';':
	case ',':
	case '=':
	case '*':
	case '!':
	case '^':
		return nextch ();
	case '#':
		while (1) {
			ch = peekch ();
			if (ch == EOF || ch == '\n')
				break;
			nextch ();
		}
		goto begin;
	case EOF:
		return TK_EOF;
	default:
		return error ("invalid input");
	}
}

// AST DATA TYPES

enum dtype_type {
	DT_NONE,
	DT_DPTR,	// pointer to data memory (ds)
	DT_FPTR,	// segment+pointer
	DT_SPTR,	// pointer to stack memory (ss)
	DT_LABEL,
	DT_BYTE,
	DT_WORD,
	DT_DWORD,
	DT_QWORD,
	DT_FLOAT,
	DT_DOUBLE,
	DT_LDOUBLE,
};

struct dtype {
	enum dtype_type type;
	struct dtype *inner;
} default_dt = {
	DT_WORD,
	NULL	
};

struct dtype dt_byte = { DT_BYTE, NULL };

struct regimm {
	int is_imm;
	struct dtype *dt;
	union {
		int reg;
		long imm;
		double f;
	};
};

enum expr_type {
	EX_REG,		// .reg
	EX_INT,		// .i
	EX_FLOAT,	// .f
	EX_ALLOCA,	// .alloca
	EX_ADD,		// .bin
	EX_SUB,		// .bin
	EX_AND,		// .bin
	EX_OR,		// .bin
	EX_XOR,		// .bin
	EX_LSL,		// .bin
	EX_LSR,		// .bin
	EX_ASR,		// .bin
	EX_MUL,		// .bin
	EX_UDIV,	// .bin
	EX_SDIV,	// .bin
	EX_EQ,		// .bin
	EX_NE,		// .bin
	EX_ULT,		// .bin
	EX_UGT,		// .bin
	EX_ULE,		// .bin
	EX_UGE,		// .bin
	EX_SLT,		// .bin
	EX_SGT,		// .bin
	EX_SLE,		// .bin
	EX_SGE,		// .bin
	EX_FEQ,		// .bin
	EX_FNE,		// .bin
	EX_FLT,		// .bin
	EX_FGT,		// .bin
	EX_FLE,		// .bin
	EX_FGE,		// .bin
	EX_PTRADD,	// .bin
	EX_READ,	// .read
	EX_CALL,	// .call
	EX_PHI,		// .phi
	EX_PEXT,	// .reg
	EX_ZEXT,	// .reg
	EX_SEXT,	// .reg
	EX_TRUNC,	// .reg
	EX_PCAST,	// .reg
	EX_LOOKUP,	// .s
};

struct call {
	char *fn;
	int nargs;
	struct regimm args[MAXARGS];
};

struct phi {
	int from[MAXPHI], val[MAXPHI], num;
};

struct expr {
	enum expr_type type;
	union {
		int reg;
		long i;
		double f;
		char *s;
		struct {
			struct regimm num;
			int off;	// only valid if ri.is_imm
		} alloca;
		struct {
			int l;
			struct regimm r;
		} bin;
		struct call *call;
		struct phi *phi;
		struct {
			int src, off;
		} read;
	};
};

enum ir_type {
	IR_NOP,
	IR_LET,		// .let
	IR_WRITE,	// .write
	IR_RET,		// .ri
	IR_LABEL,	// .reg
	IR_GOTO,	// .reg
	IR_BR,		// .br
};

struct ir {
	struct ir *next, *prev;		// next/prev IR in current block
	struct ir *bnext, *bprev;	// next/prev block of IR
	struct ir *bhead;		// head of current IR block
	enum ir_type type;
	int line;

	union {
		int reg;
		struct regimm ri;
		struct {
			int dest;
			struct dtype dt;
			struct expr val;
		} let;
		struct {
			int dest;
			struct regimm src;
			int off;
		} write;
		struct {
			int cond, yes, no;
		} br;
	};
};

enum reg_type {
	R_NONE,
	R_PARAM,
	R_LOCAL,
	R_LABEL,
};

struct reg {
	enum reg_type type;
	struct dtype dt;
	struct ir *ir;

	// R_LOCAL/R_PARAM: stack offset
	// R_LABEL: self-reference
	int off;
};

struct func {
	char *name;
	struct dtype dt;
	struct reg regs[NREGS];
	struct ir *body;
	char *section;
	int stoff;
	int is_pub;
};

// MISC

sizeof_dt (dt)
struct dtype *dt;
{
	switch (dt->type) {
	case DT_NONE:
		return error ("sizeof_dt(DT_NONE)");
	case DT_BYTE:
		return 1;
	case DT_WORD:
	case DT_DPTR:
	case DT_SPTR:
	case DT_LABEL:
		return 2;
	case DT_DWORD:
	case DT_FPTR:
	case DT_FLOAT:
		return 4;
	case DT_QWORD:
	case DT_DOUBLE:
		return 8;
	case DT_LDOUBLE:
		return 10;
	}
}

assert_dt_eq (a, b)
struct dtype *a, *b;
{
	if (a->type != b->type)
		return error ("type mismatch");

	switch (a->type) {
	case DT_DPTR:
	case DT_FPTR:
	case DT_SPTR:
		return assert_dt_eq (a->inner, b->inner);
	default:
		return 0;
	}
}

assert_dt_is_ptr (ptr)
struct dtype *ptr;
{
	if (ptr->type != DT_DPTR && ptr->type != DT_FPTR && ptr->type != DT_SPTR)
		return error ("not a pointer");
	return 0;
}

assert_dt_ptr (ptr, to)
struct dtype *ptr, *to;
{
	assert_dt_is_ptr (ptr);
	return assert_dt_eq (ptr->inner, to);
}

assert_dt_label (dt)
struct dtype *dt;
{
	if (dt->type != DT_LABEL)
		return error ("not a label");
	return 0;
}

assert_dt_is_num (dt)
struct dtype *dt;
{
	switch (dt->type) {
	case DT_BYTE:
	case DT_WORD:
	case DT_DWORD:
	case DT_QWORD:
	case DT_FLOAT:
	case DT_DOUBLE:
	case DT_LDOUBLE:
		return 0;
	case DT_NONE:
	case DT_LABEL:
	case DT_DPTR:
	case DT_FPTR:
	case DT_SPTR:
		return error ("this operation is only supported for numbers (byte, word, dword, qword, float)");
	}
}

assert_dt_is_int (dt)
struct dtype *dt;
{
	switch (dt->type) {
	case DT_BYTE:
	case DT_WORD:
	case DT_DWORD:
	case DT_QWORD:
		return 0;
	case DT_FLOAT:
	case DT_DOUBLE:
	case DT_LDOUBLE:
	case DT_NONE:
	case DT_LABEL:
	case DT_DPTR:
	case DT_FPTR:
	case DT_SPTR:
		return error ("this operation is only supported for integers (byte, word, dword, qword)");
	}
}

assert_dt_is_float (dt)
struct dtype *dt;
{
	switch (dt->type) {
	case DT_FLOAT:
	case DT_DOUBLE:
	case DT_LDOUBLE:
		return 0;
	default:
		return error ("this operation is only supported for floats (float, double, ldouble)");
	}
}

// PARSER

#define new(T) ((T *)alloc (sizeof (T)))
#define expect(exp) (lex () != (exp) ? error ("expected " #exp) : 0)
#define stalloc(fn, nbytes) ((fn)->stoff -= (((nbytes) + 1) & ~1))

dtype (dt)
struct dtype *dt;
{
	int tk;

	tk = lex ();

	if (tk == '*') {
		dt->type = DT_DPTR;
		dt->inner = new (struct dtype);
		return dtype (dt->inner);
	} else if (tk == '!') {
		dt->type = DT_FPTR;
		dt->inner = new (struct dtype);
		return dtype (dt->inner);
	} else if (tk == '^') {
		dt->type = DT_SPTR;
		dt->inner = new (struct dtype);
		return dtype (dt->inner);
	}

	if (tk != TK_IDENT)
		goto err;

	if (strcmp (lval.s, "byte") == 0) {
		dt->type = DT_BYTE;
	} else if (strcmp (lval.s, "word") == 0) {
		dt->type = DT_WORD;
	} else if (strcmp (lval.s, "dword") == 0) {
		dt->type = DT_DWORD;
	} else if (strcmp (lval.s, "qword") == 0) {
		dt->type = DT_QWORD;
	} else if (strcmp (lval.s, "float") == 0) {
		dt->type = DT_FLOAT;
	} else if (strcmp (lval.s, "double") == 0) {
		dt->type = DT_DOUBLE;
	} else if (strcmp (lval.s, "ldouble") == 0) {
		dt->type = DT_LDOUBLE;
	} else {
	err:
		error ("expected dtype");
	}
	return 0;
}

regimm (ri, dt)
struct regimm *ri;
struct dtype *dt;
{
	int tk;

	ri->dt = dt;

	tk = lex ();
	switch (tk) {
	case TK_REG:
		ri->is_imm = 0;
		ri->reg = lval.i;
		break;
	case TK_INT:
		ri->is_imm = 1;
		ri->imm = lval.i;
		break;
	default:
		error ("expected register or immediate");
	}
	return 0;
}

expr_bin (fn, dt, e, t, allow_float)
struct func *fn;
struct dtype *dt;
struct expr *e;
{
	e->type = t;
	expect (TK_REG);
	e->bin.l = lval.i;
	expect (',');
	regimm (&e->bin.r, &fn->regs[e->bin.l].dt);
	expect (';');

	assert_dt_eq (dt, &fn->regs[e->bin.l].dt);
	assert_dt_eq (dt, e->bin.r.dt);

	if (allow_float) {
		assert_dt_is_num (dt);
	} else {
		assert_dt_is_int (dt);
	}
	return 0;
}

expr_shift (fn, dt, e, t)
struct func *fn;
struct dtype *dt;
struct expr *e;
enum expr_type t;
{
	e->type = t;
	expect (TK_REG);
	e->bin.l = lval.i;
	expect (',');
	regimm (&e->bin.r, &dt_byte);
	expect (';');

	assert_dt_eq (dt, &fn->regs[e->bin.l].dt);
	assert_dt_is_int (dt);
	if (e->bin.r.dt->type != DT_BYTE)
		error ("second argument of a shift must be of type `byte`");
	return 0;
}

expr_icast (fn, dt, e, t)
struct func *fn;
struct dtype *dt;
struct expr *e;
enum expr_type t;
{
	e->type = t;
	expect (TK_REG);
	e->reg = lval.i;
	expect (';');

	assert_dt_is_int (dt);
	assert_dt_is_int (&fn->regs[e->reg].dt);
	return 0;
}

expr_cmp (fn, dt, e, t, allow_fp)
struct func *fn;
struct dtype *dt;
struct expr *e;
enum expr_type t;
{
	e->type = t;
	expect (TK_REG);
	e->bin.l = lval.i;
	expect (',');
	regimm (&e->bin.r, &fn->regs[e->bin.l].dt);
	expect (';');

	if (allow_fp) {
		assert_dt_is_num (&fn->regs[e->bin.l].dt);
	} else {
		assert_dt_is_int (&fn->regs[e->bin.l].dt);
	}
	assert_dt_eq (&fn->regs[e->bin.l].dt, e->bin.r.dt);
	if (dt->type != DT_WORD)
		error ("result of comparison must be of type word");
	return 0;
}

expr_fcmp (fn, dt, e, t)
struct func *fn;
struct dtype *dt;
struct expr *e;
enum expr_type t;
{
	e->type = t;
	expect (TK_REG);
	e->bin.l = lval.i;
	expect (',');
	regimm (&e->bin.r, &fn->regs[e->bin.l].dt);
	expect (';');

	assert_dt_is_float (&fn->regs[e->bin.l].dt);
	assert_dt_eq (&fn->regs[e->bin.l].dt, e->bin.r.dt);
	if (dt->type != DT_WORD)
		error ("result of comparison must be of type word");
	if (e->bin.r.is_imm)
		error ("immediate arguments not supported");
	return 0;
}

expr (fn, dt, e)
struct func *fn;
struct dtype *dt;
struct expr *e;
{
	struct regimm *ri;
	struct reg *r;
	int tk;

	tk = lex ();

	if (tk == TK_INT) {
		e->type = EX_INT;
		e->i = lval.i;
		expect (';');
		return 0;
	} else if (tk == TK_REG) {
		e->type = EX_REG;
		e->reg = lval.i;
		expect (';');
		assert_dt_eq (dt, &fn->regs[e->reg].dt);
		return 0;
	} else if (tk == TK_FLOAT) {
		e->type = EX_FLOAT;
		e->f = lval.f;
		expect (';');
		return 0;
	}

	if (tk != TK_IDENT)
		goto err;

	if (strcmp (lval.s, "add") == 0) {
		expr_bin (fn, dt, e, EX_ADD, 1);
	} else if (strcmp (lval.s, "sub") == 0) {
		expr_bin (fn, dt, e, EX_SUB, 1);
	} else if (strcmp (lval.s, "and") == 0) {
		expr_bin (fn, dt, e, EX_AND, 0);
	} else if (strcmp (lval.s, "or") == 0) {
		expr_bin (fn, dt, e, EX_OR, 0);
	} else if (strcmp (lval.s, "xor") == 0) {
		expr_bin (fn, dt, e, EX_XOR, 0);
	} else if (strcmp (lval.s, "mul") == 0) {
		expr_bin (fn, dt, e, EX_MUL, 1);
	} else if (strcmp (lval.s, "udiv") == 0) {
		expr_bin (fn, dt, e, EX_UDIV, 0);
	} else if (strcmp (lval.s, "sdiv") == 0) {
		expr_bin (fn, dt, e, EX_SDIV, 1);
	} else if (strcmp (lval.s, "eq") == 0) {
		expr_cmp (fn, dt, e, EX_EQ, 1);
	} else if (strcmp (lval.s, "ne") == 0) {
		expr_cmp (fn, dt, e, EX_NE, 1);
	} else if (strcmp (lval.s, "ult") == 0) {
		expr_cmp (fn, dt, e, EX_ULT, 0);
	} else if (strcmp (lval.s, "ugt") == 0) {
		expr_cmp (fn, dt, e, EX_UGT, 0);
	} else if (strcmp (lval.s, "ule") == 0) {
		expr_cmp (fn, dt, e, EX_ULE, 0);
	} else if (strcmp (lval.s, "uge") == 0) {
		expr_cmp (fn, dt, e, EX_UGE, 0);
	} else if (strcmp (lval.s, "slt") == 0) {
		expr_cmp (fn, dt, e, EX_SLT, 0);
	} else if (strcmp (lval.s, "sgt") == 0) {
		expr_cmp (fn, dt, e, EX_SGT, 0);
	} else if (strcmp (lval.s, "sle") == 0) {
		expr_cmp (fn, dt, e, EX_SLE, 0);
	} else if (strcmp (lval.s, "sge") == 0) {
		expr_cmp (fn, dt, e, EX_SGE, 0);
	} else if (strcmp (lval.s, "feq") == 0) {
		expr_fcmp (fn, dt, e, EX_FEQ);
	} else if (strcmp (lval.s, "fne") == 0) {
		expr_fcmp (fn, dt, e, EX_FNE);
	} else if (strcmp (lval.s, "flt") == 0) {
		expr_fcmp (fn, dt, e, EX_FLT);
	} else if (strcmp (lval.s, "fgt") == 0) {
		expr_fcmp (fn, dt, e, EX_FGT);
	} else if (strcmp (lval.s, "fle") == 0) {
		expr_fcmp (fn, dt, e, EX_FLE);
	} else if (strcmp (lval.s, "fge") == 0) {
		expr_fcmp (fn, dt, e, EX_FGE);
	} else if (strcmp (lval.s, "lsl") == 0) {
		expr_shift (fn, dt, e, EX_LSL);
	} else if (strcmp (lval.s, "lsr") == 0) {
		expr_shift (fn, dt, e, EX_LSR);
	} else if (strcmp (lval.s, "asr") == 0) {
		expr_shift (fn, dt, e, EX_ASR);
	} else if (strcmp (lval.s, "zext") == 0) {
		expr_icast (fn, dt, e, EX_ZEXT);
		if (dt->type <= fn->regs[e->reg].dt.type)
			error ("invalid zext");
	} else if (strcmp (lval.s, "sext") == 0) {
		expr_icast (fn, dt, e, EX_SEXT);
		if (dt->type <= fn->regs[e->reg].dt.type)
			error ("invalid sext");
	} else if (strcmp (lval.s, "trunc") == 0) {
		expr_icast (fn, dt, e, EX_TRUNC);
		if (dt->type >= fn->regs[e->reg].dt.type)
			error ("invalid trunc");
	} else if (strcmp (lval.s, "pext") == 0) {
		e->type = EX_PEXT;
		expect (TK_REG);
		e->reg = lval.i;
		expect (';');

		if (dt->type != DT_FPTR)
			error ("the result of `pext` must be a far pointer");
		r = &fn->regs[e->reg];
		if (r->dt.type != DT_DPTR && r->dt.type != DT_SPTR)
			error ("the source of `pext` must be either a data pointer or stack pointer");
		assert_dt_eq (dt->inner, r->dt.inner);
	} else if (strcmp (lval.s, "alloca") == 0) {
		if (dt->type != DT_SPTR && dt->type != DT_FPTR)
			error ("the result of alloca must be stored in a far or stack pointer");

		e->type = EX_ALLOCA;
		regimm (&e->alloca.num, &default_dt);
		if (e->alloca.num.is_imm) {
			stalloc (fn, sizeof_dt (dt->inner) * e->alloca.num.imm);
			e->alloca.off = fn->stoff;
		}
		expect (';');
	} else if (strcmp (lval.s, "ptradd") == 0) {
		e->type = EX_PTRADD;
		expect (TK_REG);
		e->bin.l = lval.i;
		expect (',');
		regimm (&e->bin.r, &default_dt);
		expect (';');

		assert_dt_is_ptr (dt);
		if (e->bin.r.dt->type != DT_WORD)
			error ("ptradd requires word as second argument");
	} else if (strcmp (lval.s, "read") == 0) {
		e->type = EX_READ;
		expect (TK_REG);
		e->read.src = lval.i;
		switch (lex ()) {
		case ';':
			e->read.off = 0;
			break;
		case ',':
			expect (TK_INT);
			e->read.off = lval.i;
			expect (';');
			break;
		default:
			error ("syntax error");
			break;
		}

		assert_dt_ptr (&fn->regs[e->read.src].dt, dt);
	} else if (strcmp (lval.s, "call") == 0) {
		e->type = EX_CALL;
		expect (TK_IDENT);
		e->call = new (struct call);
		e->call->fn = lval.s;
		e->call->nargs = 0;

		while ((tk = lex ()) == ',') {
			ri = &e->call->args[e->call->nargs++];
			regimm (ri, &default_dt);
			if (!ri->is_imm)
				ri->dt = &fn->regs[ri->reg].dt;
		}

		if (tk != ';')
			error ("invalid call");

		// TODO: assert_dt_*
	} else if (strcmp (lval.s, "phi") == 0) {
		e->type = EX_PHI;
		expect ('(');
		expect (TK_REG);
		e->phi = new (struct phi);
		e->phi->from[0] = lval.i;
		expect (':');
		expect (TK_REG);
		e->phi->val[0] = lval.i;
		expect (')');
		e->phi->num = 1;

		while ((tk = lex ()) == ',') {
			expect ('(');
			expect (TK_REG);
			e->phi->from[e->phi->num] = lval.i;
			expect (':');
			expect (TK_REG);
			e->phi->val[e->phi->num] = lval.i;
			expect (')');
			++e->phi->num;
		}
		
		if (tk != ';')
			error ("invalid phi");

		// TODO: assert_dt_*
		// TODO: check that there are no instructions (except IR_LABEL) before this
	} else if (strcmp (lval.s, "pcast") == 0) {
		e->type = EX_PCAST;
		expect (TK_REG);
		e->reg = lval.i;
		expect (';');

		assert_dt_is_ptr (dt);
		assert_dt_is_ptr (&fn->regs[e->reg].dt);
		if (dt->type != fn->regs[e->reg].dt.type)
			error ("pcast cannot be used to cast between different types of pointers (eg. SPTR -> FPTR)");
	} else if (strcmp (lval.s, "lookup") == 0) {
		e->type = EX_LOOKUP;
		expect (TK_IDENT);
		e->s = lval.s;
		expect (';');

		assert_dt_is_ptr (dt);
		// TODO: symbol table
	} else {
	err:
		error ("expected expr");
	}
	return 0;
}

// return:
// 0 = normal IR
// 1 = end of basic block (e.g. goto)
// 2 = start of basic block (label)
stmt (fn, ir, tk)
struct func *fn;
struct ir *ir;
{
	struct reg *r;

	if (tk == TK_INT) {
		ir->type = IR_LABEL;
		ir->reg = lval.i;
		expect (':');

		r = &fn->regs[ir->reg];
		if (r->type != R_NONE)
			error ("cannot reassign to register");
		r->type = R_LABEL;
		r->dt.type = DT_LABEL;
		r->off = ir->reg;
		r->ir = ir;
		return 2;
	}

	if (tk != TK_IDENT)
		goto err;

	if (strcmp (lval.s, "nop") == 0) {
		ir->type = IR_NOP;
		expect (';');
		return 0;
	} else if (strcmp (lval.s, "let") == 0) {
		ir->type = IR_LET;
		expect (TK_REG);
		ir->let.dest = lval.i;
		expect (':');
		dtype (&ir->let.dt);
		expect ('=');
		expr (fn, &ir->let.dt, &ir->let.val);

		r = &fn->regs[ir->let.dest];
		if (r->type != R_NONE)
			error ("cannot reassign to register");
		r->type = R_LOCAL;
		r->dt = ir->let.dt;
		stalloc (fn, sizeof_dt (&r->dt));
		r->off = fn->stoff;
		r->ir = ir;
		return 0;
	} else if (strcmp (lval.s, "write") == 0) {
		ir->type = IR_WRITE;
		expect (TK_REG);
		ir->write.dest = lval.i;
		expect (',');
		assert_dt_is_ptr (&fn->regs[ir->write.dest].dt);
		regimm (&ir->write.src, fn->regs[ir->write.dest].dt.inner);
		switch (lex ()) {
		case ';':
			ir->write.off = 0;
			break;
		case ',':
			expect (TK_INT);
			ir->write.off = lval.i;
			expect (';');
			break;
		default:
			error ("syntax error");
		}

		assert_dt_ptr (&fn->regs[ir->write.dest].dt, ir->write.src.dt);
		return 0;
	} else if (strcmp (lval.s, "ret") == 0) {
		ir->type = IR_RET;
		regimm (&ir->ri, &fn->dt);
		expect (';');

		assert_dt_eq (&fn->dt, ir->ri.dt);
		return 1;
	} else if (strcmp (lval.s, "goto") == 0) {
		ir->type = IR_GOTO;
		expect (TK_REG);
		ir->reg = lval.i;
		expect (';');
		return 1;
	} else if (strcmp (lval.s, "br") == 0) {
		ir->type = IR_BR;
		expect (TK_REG);
		ir->br.cond = lval.i;
		expect (',');
		expect (TK_REG);
		ir->br.yes = lval.i;
		expect (',');
		expect (TK_REG);
		ir->br.no = lval.i;
		expect (';');
		return 1;
	} else {
	err:
		return error ("expected IR stmt");
	}
}

func (fn)
struct func *fn;
{
	struct ir *bprev = NULL, *bhead = NULL, *prev = NULL, *ir, *tmp;
	int tk, i, stoff;

	expect (TK_IDENT);
	fn->name = lval.s;

	expect ('(');
	stoff = 4;

	// parse parameter list
	while (1) {
		tk = lex ();
		switch (tk) {
		case ',':
			break;
		case ')':
			goto rt;
		case TK_REG:
			i = lval.i;
			expect (':');
			fn->regs[i].type = R_PARAM;
			fn->regs[i].off = stoff;
			dtype (&fn->regs[i].dt);
			stoff += (sizeof_dt (&fn->regs[i].dt) + 1) & ~1;
			break;
		default:
			printf ("%d\n", tk);
			error ("invalid parameter list");
		}
	}

rt:
	expect (':');
	dtype (&fn->dt);
	expect ('{');

	fn->stoff = 0;

	while ((tk = lex ()) != '}') {
		ir = new (struct ir);
		ir->line = linenum;

		if (fn->body == NULL) {
			fn->body = ir;
			bhead = ir;
		}

		switch (stmt (fn, ir, tk)) {
		case 0: // normal IR
			if (bhead == NULL)
				error ("dead instruction");

			ir->prev = prev;
			ir->bprev = bprev;
			ir->bhead = bhead;

			if (prev != NULL)
				prev->next = ir;

			prev = ir;
			break;
		case 1: // end of basic block
			if (bhead == NULL)
				error ("invalid end of a basic block");

			if (prev != NULL)
				prev->next = ir;

			ir->prev = prev;
			ir->bprev = bprev;
			ir->bhead = bhead;
			bprev = bhead;
			bhead = NULL;
			prev = NULL;
			break;
		case 2: // start of basic block
			if (prev != NULL || bhead != NULL)
				error ("invalid start of a basic block");

			ir->prev = NULL;
			ir->bprev = bprev;
			ir->bhead = ir;

			for (tmp = bprev; tmp != NULL; tmp = tmp->next)
				tmp->bnext = ir;

			bhead = ir;
			prev = ir;
			break;
		}
	}

	return 0;
}

parse (void)
{
	extern gen ();
	struct func fn;
	int tk, is_pub;
	char *section;

	while ((tk = lex ()) != TK_EOF) {
		if (tk != TK_IDENT)
			goto err;
		
		is_pub = 0;
		section = NULL;

		if (strcmp (lval.s, "pub") == 0) {
			is_pub = 1;
			expect (TK_IDENT);
		}

		if (strcmp (lval.s, "section") == 0) {
			expect (TK_STRING);
			section = lval.s;
			expect (TK_IDENT);
		}

		if (strcmp (lval.s, "fn") == 0) {
			if (section == NULL)
				section = ".text";

			memset (&fn, 0, sizeof (fn));
			fn.is_pub = is_pub;
			fn.section = section;
			func (&fn);
			gen (&fn);
			reset ();
		} else {
		err:
			error ("expected fn");
		}
	}

	return 0;
}

// IR PRINTING

print_dt (dt)
struct dtype *dt;
{
	switch (dt->type) {
	case DT_NONE:
		error ("print_dt(DT_NONE)");
		break;
	case DT_BYTE:
		printf ("byte");
		break;
	case DT_WORD:
		printf ("word");
		break;
	case DT_DWORD:
		printf ("dword");
		break;
	case DT_QWORD:
		printf ("qword");
		break;
	case DT_FLOAT:
		printf ("float");
		break;
	case DT_DOUBLE:
		printf ("double");
		break;
	case DT_LDOUBLE:
		printf ("ldouble");
		break;
	case DT_DPTR:
		putchar ('*');
		print_dt (dt->inner);
		break;
	case DT_FPTR:
		putchar ('!');
		print_dt (dt->inner);
		break;
	case DT_SPTR:
		putchar ('^');
		print_dt (dt->inner);
		break;
	case DT_LABEL:
		printf ("label");
		break;
	}
	return 0;
}

print_ri (ri)
struct regimm *ri;
{
	if (ri->is_imm) {
		printf ("%ld", ri->imm);
	} else {
		printf ("$%d", ri->reg);
	}
	return 0;
}

print_ex (e)
struct expr *e;
{
	int i;

	switch (e->type) {
	case EX_INT:
		printf ("%ld", e->i);
		break;
	case EX_FLOAT:
		printf ("%f", e->f);
		break;
	case EX_REG:
		printf ("$%d", e->reg);
		break;
	case EX_ALLOCA:
		printf ("alloca ");
		print_ri (&e->alloca.num);
		break;
	case EX_ADD:
		printf ("add $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_SUB:
		printf ("sub $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_AND:
		printf ("and $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_OR:
		printf ("or $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_XOR:
		printf ("xor $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_MUL:
		printf ("mul $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_UDIV:
		printf ("udiv $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_SDIV:
		printf ("sdiv $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_EQ:
		printf ("eq $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_NE:
		printf ("ne $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_ULT:
		printf ("ult $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_UGT:
		printf ("ugt $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_ULE:
		printf ("ule $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_UGE:
		printf ("uge $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_SLT:
		printf ("slt $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_SGT:
		printf ("sgt $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_SLE:
		printf ("sle $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_SGE:
		printf ("sge $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_FEQ:
		printf ("feq $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_FNE:
		printf ("fne $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_FLT:
		printf ("flt $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_FGT:
		printf ("fgt $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_FLE:
		printf ("fle $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_FGE:
		printf ("fge $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_PTRADD:
		printf ("ptradd $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_READ:
		printf ("read $%d, %d", e->read.src, e->read.off);
		break;
	case EX_CALL:
		printf ("call %s", e->call->fn);
		for (i = 0; i < e->call->nargs; ++i) {
			printf (", ");
			print_ri (&e->call->args[i]);
		}
		break;
	case EX_PHI:
		printf ("phi ($%d: $%d)", e->phi->from[0], e->phi->val[0]);
		for (i = 1; i < e->phi->num; ++i)
			printf (", ($%d: $%d)", e->phi->from[i], e->phi->val[i]);
		break;
	case EX_PEXT:
		printf ("pext $%d", e->reg);
		break;
	case EX_ZEXT:
		printf ("zext $%d", e->reg);
		break;
	case EX_SEXT:
		printf ("sext $%d", e->reg);
		break;
	case EX_TRUNC:
		printf ("trunc $%d", e->reg);
		break;
	case EX_PCAST:
		printf ("pcast $%d", e->reg);
		break;
	case EX_LSL:
		printf ("lsl $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_LSR:
		printf ("lsr $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_ASR:
		printf ("asr $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_LOOKUP:
		printf ("lookup %s", e->s);
		break;
	}
	return 0;
}

print_ir (ir)
struct ir *ir;
{
	printf ("%d: ", ir->line);
	switch (ir->type) {
	case IR_NOP:
		puts ("nop;");
		break;
	case IR_LET:
		printf ("let $%d: ", ir->let.dest);
		print_dt (&ir->let.dt);
		printf (" = ");
		print_ex (&ir->let.val);
		puts (";");
		break;
	case IR_WRITE:
		printf ("write $%d, ", ir->write.dest);
		print_ri (&ir->write.src);
		printf (", %d;\n", ir->write.off);
		break;
	case IR_RET:
		printf ("ret ");
		print_ri (&ir->ri);
		puts (";");
		break;
	case IR_LABEL:
		printf ("%d:\n", ir->reg);
		break;
	case IR_GOTO:
		printf ("goto $%d;\n", ir->reg);
		break;
	case IR_BR:
		printf ("br $%d, $%d, $%d;\n", ir->br.cond, ir->br.yes, ir->br.no);
		break;
	}
	return 0;
}

// ASM MISC

int nlbl;

enum x86_reg8 {
	AL,
	BL,
	CL,
	DL,
};

enum x86_reg16 {
	AX,
	BX,
	CX,
	DX,
	ES,
};

char *reg8[] = { "al", "bl", "cl", "dl" };
char *reg16[] = { "ax", "bx", "cx", "dx", "es" };

// ASM GEN

loadb (instr, dest, src)
char *instr;
enum x86_reg8 dest;
struct reg *src;
{
	assert (sizeof_dt (&src->dt) == 1);

	if (instr == NULL)
		instr = "mov";

	switch (src->type) {
	case R_NONE:
	case R_LABEL:
		abort ();
		break;
	case R_LOCAL:
	case R_PARAM:
		printf ("\t%s %s, byte [ss:bp + %d]\n",
			instr, reg8[dest], src->off);
		break;
	}
	return 0;
}

loadw (instr, dest, src)
char *instr;
enum x86_reg16 dest;
struct reg *src;
{
	assert (sizeof_dt (&src->dt) == 2);

	if (instr == NULL)
		instr = "mov";

	switch (src->type) {
	case R_NONE:
		abort ();
		break;
	case R_LOCAL:
	case R_PARAM:
		printf ("\t%s %s, word [ss:bp + %d]\n",
			instr, reg16[dest], src->off);
		break;
	case R_LABEL:
		printf ("\tlea %s, [.L%d]\n",
			reg16[dest], src->off);
		break;
	}
	return 0;
}

loadpw (instr, dest, src, off)
char *instr;
enum x86_reg16 dest;
struct reg *src;
{
	if (instr == NULL)
		instr = "mov";

	switch (src->type) {
	case R_NONE:
	case R_LABEL:
		abort ();
		break;
	case R_LOCAL:
	case R_PARAM:
		printf ("\t%s %s, word [ss:bp + %d + %d]\n",
			instr, reg16[dest], src->off, off);
		break;
	}
	return 0;
}

loadpb (instr, dest, src, off)
char *instr;
enum x86_reg8 dest;
struct reg *src;
{
	if (instr == NULL)
		instr = "mov";

	switch (src->type) {
	case R_NONE:
	case R_LABEL:
		abort ();
		break;
	case R_LOCAL:
	case R_PARAM:
		printf ("\t%s %s, byte [ss:bp + %d + %d]\n",
			instr, reg8[dest], src->off, off);
		break;
	}
	return 0;
}

loaddw (instr, src)
char *instr;
struct reg *src;
{
	assert (sizeof_dt (&src->dt) == 4);
	loadpw (instr, AX, src, 0);
	loadpw (instr, DX, src, 2);
	return 0;
}

loadqw (instr, src)
char *instr;
struct reg *src;
{
	assert (sizeof_dt (&src->dt) == 8);
	loadpw (instr, AX, src, 0);
	loadpw (instr, DX, src, 2);
	loadpw (instr, BX, src, 4);
	loadpw (instr, CX, src, 6);
	return 0;
}

loadf (instr, src)
char *instr;
struct reg *src;
{
	char *size;

	if (instr == NULL)
		instr = "fld";

	switch (src->dt.type) {
	case DT_FLOAT:
		size = "dword";
		break;
	case DT_DOUBLE:
		size = "qword";
		break;
	case DT_LDOUBLE:
		size = "tword";
		break;
	default:
		abort ();
		break;
	}

	printf ("\t%s %s [ss:bp + %d]\n", instr, size, src->off);
	return 0;
}

load (instr, src)
char *instr;
struct reg *src;
{
	switch (src->dt.type) {
	case DT_NONE:
		abort ();
		return 0;
	case DT_BYTE:
		return loadb (instr, AL, src);
	case DT_WORD:
	case DT_DPTR:
	case DT_SPTR:
	case DT_LABEL:
		return loadw (instr, AX, src);
	case DT_DWORD:
	case DT_FPTR:
		return loaddw (instr, src);
	case DT_QWORD:
		return loadqw (instr, src);
	case DT_FLOAT:
	case DT_DOUBLE:
	case DT_LDOUBLE:
		return loadf (instr, src);
	}
}

char *
loadptr (ptr)
struct reg *ptr;
{
	switch (ptr->dt.type) {
	case DT_DPTR:
		loadw (NULL, BX, ptr);
		return "ds:bx";
	case DT_SPTR:
		loadw (NULL, BX, ptr);
		return "ss:bx";
	case DT_FPTR:
		loadpw (NULL, BX, ptr, 0);
		loadpw (NULL, ES, ptr, 2);
		return "es:bx";
	default:
		return abort (), NULL;
	}
}

loadimmr (instr, dest, imm)
char *instr, *dest;
long imm;
{
	if (imm != 0 || instr != NULL) {
		if (instr == NULL)
			instr = "mov";
		printf ("\t%s %s, %ld\n", instr, dest, imm);
	} else {
		printf ("\txor %s, %s\n", dest, dest);
	}
	return 0;
}

loadimm (instr, dt, imm)
char *instr;
struct dtype *dt;
long imm;
{
	switch (dt->type) {
	case DT_NONE:
	case DT_LABEL:
		abort ();
	case DT_BYTE:
		loadimmr (instr, "al", imm & 0xff);
		break;
	case DT_WORD:
	case DT_DPTR:
		loadimmr (instr, "ax", imm & 0xffff);
		break;
	case DT_SPTR:
		if (imm != 0)
			error ("it is illegal initialize a pointer to stack memory with anything else than 0");
		// fallthrough
	case DT_DWORD:
	case DT_FPTR:
		loadimmr (instr, "ax", imm & 0xffff);
		loadimmr (instr, "dx", (imm >> 16) & 0xffff);
		break;
	case DT_QWORD:
		loadimmr (instr, "ax", imm & 0xffff);
		loadimmr (instr, "dx", (imm >> 16) & 0xffff);
		loadimmr (instr, "bx", (imm >> 32) & 0xffff);
		loadimmr (instr, "cx", (imm >> 48) & 0xffff);
		break;
	case DT_FLOAT:
	case DT_DOUBLE:
	case DT_LDOUBLE:
		error ("cannot use immediate values for floating-point operations");
		break;
	}
	return 0;
}

loadri (fn, instr, ri)
struct func *fn;
char *instr;
struct regimm *ri;
{
	if (ri->is_imm) {
		return loadimm (instr, ri->dt, ri->imm);
	} else {
		return load (instr, &fn->regs[ri->reg]);
	}
}

// yes, this function is absolutely ridiculous
loadpwri (fn, instr, dest, src, off)
struct func *fn;
char *instr;
enum x86_reg16 dest;
struct regimm *src;
int off;
{
	if (src->is_imm) {
		printf ("\t%s %s, %d\n", instr, reg16[dest],
			(int)((src->imm >> (off * 8)) & 0xffff));
	} else {
		loadpw (instr, dest, &fn->regs[src->reg], off);
	}
	return 0;
}

// constant multiply
cmul (reg, i)
char *reg;
{
	int x = ffs (i);

	if (x == 0) {
		printf ("\txor %s, %s\n", reg, reg);
		return 0;
	}

	if (i == (1 << (x - 1))) {
		printf ("\tshr %s, %d\n", reg, (x - 1));
		return 0;
	}

	printf ("\timul %s, %d\n", reg, i);
	return 0;
}

addsub (instr1, instrn, instrf, fn, dt, e)
char *instr1, *instrn, *instrf;
struct func *fn;
struct dtype *dt;
struct expr *e;
{
	load (NULL, &fn->regs[e->bin.l]);
	switch (dt->type) {
	case DT_NONE:
	case DT_LABEL:
		abort ();
		break;
	case DT_BYTE:
	case DT_WORD:
		loadri (fn, instr1, &e->bin.r);
		break;
	case DT_DWORD:
		loadpwri (fn, instr1, AX, &e->bin.r, 0);
		loadpwri (fn, instrn, DX, &e->bin.r, 2);
		break;
	case DT_QWORD:
		loadpwri (fn, instr1, AX, &e->bin.r, 0);
		loadpwri (fn, instrn, DX, &e->bin.r, 2);
		loadpwri (fn, instrn, BX, &e->bin.r, 4);
		loadpwri (fn, instrn, CX, &e->bin.r, 6);
		break;
	case DT_FLOAT:
	case DT_DOUBLE:
	case DT_LDOUBLE:
		loadri (fn, instrf, &e->bin.r);
		break;
	case DT_DPTR:
	case DT_FPTR:
	case DT_SPTR:
		error ("cannot add/sub pointers or floats");
		break;
	}
	return 0;
}

alu (instr, fn, dt, e)
char *instr;
struct func *fn;
struct dtype *dt;
struct expr *e;
{
	load (NULL, &fn->regs[e->bin.l]);
	switch (dt->type) {
	case DT_NONE:
	case DT_LABEL:
	case DT_FLOAT:
	case DT_DOUBLE:
	case DT_LDOUBLE:
		abort ();
		break;
	case DT_DPTR:
	case DT_FPTR:
	case DT_SPTR:
		error ("invalid alu operation on pointers");
		break;
	case DT_BYTE:
	case DT_WORD:
	case DT_DWORD:
	case DT_QWORD:
		loadri (fn, instr, &e->bin.r);
		break;
	}
	return 0;
}

shift (fn, instr, reg, shamt)
struct func *fn;
char *instr;
struct reg *reg;
struct regimm *shamt;
{
	// TODO: optimize byte-aligned shift (eg lsl $0, 8)
	struct dtype *dt = &reg->dt;

	assert (shamt->dt->type == DT_BYTE);

	if (shamt->is_imm) {
		loadimmr (NULL, "cl", shamt->imm & 0xff);
	} else {
		loadb (NULL, CL, &fn->regs[shamt->reg]);
	}

	if (dt->type == DT_QWORD)
		puts ("\tpush cx");

	load (NULL, reg);

	switch (dt->type) {
	case DT_NONE:
	case DT_LABEL:
	case DT_DPTR:
	case DT_FPTR:
	case DT_SPTR:
	case DT_FLOAT:
	case DT_DOUBLE:
	case DT_LDOUBLE:
		abort ();
		break;
	case DT_BYTE:
		printf ("\t%s al, cl\n", instr);
		break;
	case DT_WORD:
		printf ("\t%s ax, cl\n", instr);
		break;
	case DT_DWORD:
		printf ("\tcall __%sd\n", instr);
		break;
	case DT_QWORD:
		printf ("\tcall __%sq\n", instr);
		break;
	}

	return 0;
}

call_muldiv (instr, fn, dt, e)
char *instr;
struct func *fn;
struct dtype *dt;
struct expr *e;
{
	struct reg *r = &fn->regs[e->bin.l];
	struct regimm *ri = &e->bin.r;

	switch (dt->type) {
	case DT_DWORD:
		if (ri->is_imm) {
			printf ("\tpush %d\n", (int)((ri->imm >> 16) & 0xffff));
			printf ("\tpush %d\n", (int)(ri->imm & 0xffff));
		} else {
			r = &fn->regs[ri->reg];
			printf ("\tpush word [ss:bp + %d + 2]\n", r->off);
			printf ("\tpush word [ss:bp + %d + 0]\n", r->off);
		}
		load (NULL, &fn->regs[e->bin.l]);
		printf ("\tcall __%sd\n", instr);
		puts ("\tadd sp, 4");
		break;
	case DT_QWORD:
		if (ri->is_imm) {
			printf ("\tpush %d\n", (int)((ri->imm >> 48) & 0xffff));
			printf ("\tpush %d\n", (int)((ri->imm >> 32) & 0xffff));
			printf ("\tpush %d\n", (int)((ri->imm >> 16) & 0xffff));
			printf ("\tpush %d\n", (int)(ri->imm & 0xffff));
		} else {
			r = &fn->regs[ri->reg];
			printf ("\tpush word [ss:bp + %d + 6]\n", r->off);
			printf ("\tpush word [ss:bp + %d + 4]\n", r->off);
			printf ("\tpush word [ss:bp + %d + 2]\n", r->off);
			printf ("\tpush word [ss:bp + %d + 0]\n", r->off);
		}
		load (NULL, &fn->regs[e->bin.l]);
		printf ("\tcall __%sq\n", instr);
		puts ("\tadd sp, 8");
		break;
	default:
		abort ();
	}

	return 0;
}

gen_div (fn, dt, e, sign)
struct func *fn;
struct dtype *dt;
struct expr *e;
{
	struct reg *acc = &fn->regs[e->bin.l];
	struct regimm *ri = &e->bin.r;
	char *instr = sign ? "idiv" : "div";

	switch (dt->type) {
	case DT_NONE:
	case DT_LABEL:
	case DT_DPTR:
	case DT_FPTR:
	case DT_SPTR:
		abort ();
		break;
	case DT_BYTE:
		loadb (NULL, AL, acc);
		if (sign) {
			puts ("\tcbw");
		} else {
			puts ("\txor dl, dl");
		}
		if (ri->is_imm) {
			loadimmr (NULL, "cl", (int)ri->imm);
			printf ("\t%s cl\n", instr);
		} else {
			printf ("\t%s byte [ss:bp + %d]\n", instr, fn->regs[ri->reg].off);
		}
		break;
	case DT_WORD:
		loadw (NULL, AX, acc);
		if (sign) {
			puts ("\tcwd");
		} else {
			puts ("\txor dx, dx");
		}
		if (ri->is_imm) {
			loadimmr (NULL, "cx", (int)ri->imm);
			printf ("\t%s cx\n", instr);
		} else {
			printf ("\t%s word [ss:bp + %d]\n", instr, fn->regs[ri->reg].off);
		}
		break;
	case DT_DWORD:
	case DT_QWORD:
		call_muldiv (instr, fn, dt, e);
		break;
	case DT_FLOAT:
	case DT_DOUBLE:
	case DT_LDOUBLE:
		if (ri->is_imm)
			error ("cannot use immediate values for floating-point operations");
		load (NULL, acc);
		load ("fdiv", &fn->regs[ri->reg]);
		break;
	}
	return 0;
}

gen_cmp (fn, e, cmp, cc)
struct func *fn;
struct expr *e;
char *cmp, *cc;
{
	int l1, l2;

	switch (e->bin.r.dt->type) {
	case DT_NONE:
	case DT_LABEL:
	case DT_DPTR:
	case DT_FPTR:
	case DT_SPTR:
		abort ();
		break;

	case DT_BYTE:
	case DT_WORD:
		load (NULL, &fn->regs[e->bin.l]);
		loadri (fn, cmp, &e->bin.r);
		goto common;
	
	case DT_FLOAT:
	case DT_DOUBLE:
	case DT_LDOUBLE:
		error ("normal comparisons not implemented for floats, use feq, flt, etc.");

	case DT_DWORD:
	case DT_QWORD:
		error ("comparisons not implemented for dword, qword, fptr");
	
	common:
		l1 = nlbl++;
		l2 = nlbl++;
		printf ("\tj%s .Li%d\n", cc, l1);
		printf ("\txor ax, ax\n");
		printf ("\tjmp .Li%d\n\n", l2);
		printf (".Li%d:\n", l1);
		printf ("\tmov ax, 1\n\n");
		printf (".Li%d:\n", l2);
		break;
	}
	return 0;
}

gen_eq (fn, e, inv)
struct func *fn;
struct expr *e;
{
	int l1, l2;

	switch (e->bin.r.dt->type) {
	case DT_NONE:
	case DT_LABEL:
		abort ();
		break;

	case DT_BYTE:
	case DT_WORD:
	case DT_DPTR:
	case DT_SPTR:
		load (NULL, &fn->regs[e->bin.l]);
		loadri (fn, "cmp", &e->bin.r);
		puts ("\tlahf");
		goto common;

	case DT_FPTR:
	case DT_DWORD:
		loaddw (NULL, &fn->regs[e->bin.l]);
		puts ("\t; TODO: adjust loaddw to allow different register pairs");
		puts ("\tmov bx, ax");
		puts ("\tmov cx, dx");
		loadri (fn, NULL, &e->bin.r);

		puts ("\tcmp bx, ax");
		puts ("\tlahf");
		puts ("\tmov bx, ax");
		puts ("\tcmp cx, dx");
		puts ("\tlahf");
		puts ("\tand ah, bh");
		goto common;

	case DT_QWORD:
		error ("TODO: EX_EQ and EX_NE for DT_QWORD");

	case DT_FLOAT:
	case DT_DOUBLE:
	case DT_LDOUBLE:
		error ("TODO: gen_eq(): floats");

		assert (!e->bin.r.is_imm);
		loadf (NULL, &fn->regs[e->bin.l]);
		loadf ("fcomp", &fn->regs[e->bin.r.reg]);
		puts ("\tfstsw ax");
		if (ignore_nan) {
			puts ("\tmov al, ah");
			puts ("\tshr al, 3");
			puts ("\tand al, 1");
		} else {
			l1 = nlbl++;
			l2 = nlbl++;
			puts ("\tand ah, 0b1101");
			puts ("\tcmp ah, 0b1000");
			printf ("\tjne .Li%d\n", l1);
			printf ("\tmov al, 1\n");
			printf ("\tjmp .Li%d\n\n", l2);
			printf (".Li%d:\n", l1);
			printf ("\txor al, al\n\n");
			printf (".Li%d:\n", l2);
		}
		break;

	common:
		puts ("\tshl ah, 1");
		puts ("\trol ax, 1");
		puts ("\tand ax, 1");
		if (inv)
			puts ("\txor ax, 1");
		break;
	}
	return 0;
}

gen_fcmp (fn, e, cc, swap)
struct func *fn;
struct expr *e;
char *cc;
{
	int l1, l2;

	assert (!e->bin.r.is_imm);
	l1 = nlbl++;
	l2 = nlbl++;
	if (swap) {
		loadf (NULL, &fn->regs[e->bin.r.reg]);
		loadf ("fcomp", &fn->regs[e->bin.l]);
	} else {
		loadf (NULL, &fn->regs[e->bin.l]);
		loadf ("fcomp", &fn->regs[e->bin.r.reg]);
	}
	printf ("\tj%s .Li%d\n", cc, l1);
	printf ("\txor ax, ax\n");
	printf ("\tjmp .Li%d\n\n", l2);
	printf (".Li%d:\n", l1);
	printf ("\tmov ax, 1\n\n");
	printf (".Li%d:\n", l2);
	return 0;
}

gen_expr (fn, dt, e)
struct func *fn;
struct dtype *dt;
struct expr *e;
{
	union {
		float f;
		int32_t i;
	} flint;
	union {
		double d;
		int64_t l;
	} dbint;
	struct regimm *ri;
	struct reg *r;
	char *s;
	int i, cnt, sz, l1, l2;

	switch (e->type) {
	case EX_INT:
		switch (dt->type) {
		case DT_NONE:
		case DT_LABEL:
			abort ();
			break;

		case DT_BYTE:
		case DT_WORD:
		case DT_DWORD:
		case DT_QWORD:
		case DT_DPTR:
		case DT_FPTR:
		case DT_SPTR:
			loadimm (NULL, dt, e->i);
			break;

		case DT_FLOAT:
			printf ("\tpush %d\n", (int)((e->i >> 16) & 0xffff));
			printf ("\tpush %d\n", (int)(e->i & 0xffff));
			puts ("\tmov bx, sp");
			puts ("\tfild dword [ss:bx]");
			puts ("\tadd sp, 4");
			break;

		case DT_LDOUBLE:
			fprintf (stderr, "%d: warning: loading an ldouble by immediate will cause the result to be truncated", linenum);
			// fallthrough

		case DT_DOUBLE:
			printf ("\tpush %d\n", (int)((e->i >> 48) & 0xffff));
			printf ("\tpush %d\n", (int)((e->i >> 32) & 0xffff));
			printf ("\tpush %d\n", (int)((e->i >> 16) & 0xffff));
			printf ("\tpush %d\n", (int)(e->i & 0xffff));
			puts ("\tmov bx, sp");
			puts ("\tfild qword [ss:bx]");
			puts ("\tadd sp, 4");
			break;
		}
		break;
	case EX_FLOAT:
		switch (dt->type) {
		case DT_FLOAT:
			flint.f = e->f;
			printf ("\tpush %d\n", (int)((flint.i >> 16) & 0xffff));
			printf ("\tpush %d\n", (int)(flint.i & 0xffff));
			puts ("\tmov bx, sp");
			puts ("\tfld dword [ss:bx]");
			puts ("\tadd sp, 4");
			break;
		case DT_LDOUBLE:
			fprintf (stderr, "%d: warning: loading an ldouble by immediate will cause the result to be truncated", linenum);
			// fallthrough
		case DT_DOUBLE:
			dbint.d = e->f;
			printf ("\tpush %d\n", (int)((dbint.l >> 48) & 0xffff));
			printf ("\tpush %d\n", (int)((dbint.l >> 32) & 0xffff));
			printf ("\tpush %d\n", (int)((dbint.l >> 16) & 0xffff));
			printf ("\tpush %d\n", (int)(dbint.l & 0xffff));
			puts ("\tmov bx, sp");
			puts ("\tfld dword [ss:bx]");
			puts ("\tadd sp, 4");
			break;

		case DT_NONE:
		case DT_LABEL:
		case DT_DPTR:
		case DT_FPTR:
		case DT_SPTR:
		case DT_BYTE:
		case DT_WORD:
		case DT_DWORD:
		case DT_QWORD:
			abort ();
			break;
		}
		break;
	case EX_REG:
		switch (dt->type) {
		case DT_FLOAT:
		case DT_DOUBLE:
		case DT_LDOUBLE:
			loadf (NULL, &fn->regs[e->reg]);
			break;
		default:
			goto pcast;
		}
		break;
	case EX_PCAST:
	pcast:
		load (NULL, &fn->regs[e->reg]);
		break;
	case EX_ALLOCA:
		switch (dt->type) {
		case DT_FPTR:
			puts ("\tmov dx, ss");
			// fallthrough
		case DT_SPTR:
			if (e->alloca.num.is_imm) {
				printf ("\tlea ax, [bp + %d]\n", e->alloca.off);
			} else {
				loadw (NULL, AX, &fn->regs[e->alloca.num.reg]);
				cmul ("ax", sizeof_dt (dt->inner));
				puts ("\tsub sp, ax");
				puts ("\tmov ax, sp");
			}
			break;
		default:
			abort ();
		}
		break;
	case EX_LOOKUP:
		switch (dt->type) {
		case DT_DPTR:
			printf ("\tlea ax, [%s]\n", e->s);
			break;
		case DT_FPTR:
			printf ("\tles ax, [%s]\n", e->s);
			puts ("\tmov dx, es");
			break;
		default:
			abort ();
		}
		break;
	case EX_ADD:
		addsub ("add", "adc", "fadd", fn, dt, e);
		break;
	case EX_SUB:
		addsub ("sub", "sbb", "fsub", fn, dt, e);
		break;
	case EX_AND:
		alu ("and", fn, dt, e);
		break;
	case EX_OR:
		alu ("or", fn, dt, e);
		break;
	case EX_XOR:
		alu ("xor", fn, dt, e);
		break;
	case EX_MUL:
		r = &fn->regs[e->bin.l];
		ri = &e->bin.r;
		switch (dt->type) {
		case DT_NONE:
		case DT_LABEL:
		case DT_DPTR:
		case DT_FPTR:
		case DT_SPTR:
			abort ();
			break;
		case DT_BYTE:
			loadb (NULL, AL, r);
			if (ri->is_imm) {
				cmul ("al", (int)(ri->imm & 0xff));
			} else {
				loadb ("mul", AL, &fn->regs[ri->reg]);
			}
			break;
		case DT_WORD:
			loadw (NULL, AX, &fn->regs[e->bin.l]);
			if (ri->is_imm) {
				cmul ("ax", (int)(ri->imm & 0xffff));
			} else {
				loadw ("mul", AX, &fn->regs[ri->reg]);
			}
			break;
		case DT_DWORD:
		case DT_QWORD:
			// TODO: multiply using FPU
			call_muldiv ("mul", fn, dt, e);
			break;
		case DT_FLOAT:
		case DT_DOUBLE:
		case DT_LDOUBLE:
			load (NULL, r);
			if (ri->is_imm)
				error ("cannot use immediate values for floating-point operations");
			load ("fmul", &fn->regs[ri->reg]);
			break;
		}
		break;
	case EX_UDIV:
		gen_div (fn, dt, e, 0);
		break;
	case EX_SDIV:
		gen_div (fn, dt, e, 1);
		break;
	case EX_EQ:
		gen_eq (fn, e, 0);
		break;
	case EX_NE:
		gen_eq (fn, e, 1);
		break;
	case EX_ULT:
		gen_cmp (fn, e, "cmp", "b");
		break;
	case EX_UGT:
		gen_cmp (fn, e, "cmp", "a");
		break;
	case EX_ULE:
		gen_cmp (fn, e, "cmp", "be");
		break;
	case EX_UGE:
		gen_cmp (fn, e, "cmp", "a");
		break;
	case EX_SLT:
		gen_cmp (fn, e, "cmp", "l");
		break;
	case EX_SGT:
		gen_cmp (fn, e, "cmp", "g");
		break;
	case EX_SLE:
		gen_cmp (fn, e, "cmp", "le");
		break;
	case EX_SGE:
		gen_cmp (fn, e, "cmp", "l");
		break;
	case EX_FEQ:
		assert (!e->bin.r.is_imm);
		l1 = nlbl++;
		l2 = nlbl++;
		loadf (NULL, &fn->regs[e->bin.l]);
		loadf ("fcomp", &fn->regs[e->bin.r.reg]);
		printf ("\tjp .Li%d\n", l1);
		printf ("\tjne .Li%d\n", l1);
		printf ("\tmov ax, 1\n");
		printf ("\tjmp .Li%d\n\n", l2);
		printf (".Li%d:\n", l1);
		printf ("\txor ax, ax\n\n");
		printf (".Li%d:\n", l2);
		break;
	case EX_FNE:
		assert (!e->bin.r.is_imm);
		l1 = nlbl++;
		l2 = nlbl++;
		loadf (NULL, &fn->regs[e->bin.l]);
		loadf ("fcomp", &fn->regs[e->bin.r.reg]);
		printf ("\tjp .Li%d\n", l1);
		printf ("\tjne .Li%d\n", l1);
		printf ("\txor ax, ax\n");
		printf ("\tjmp .Li%d\n\n", l2);
		printf (".Li%d:\n", l1);
		printf ("\tmov ax, 1\n\n");
		printf (".Li%d:\n", l2);
		break;
	case EX_FLT:
		gen_fcmp (fn, e, "a", 1);
		break;
	case EX_FGT:
		gen_fcmp (fn, e, "a", 0);
		break;
	case EX_FLE:
		gen_fcmp (fn, e, "ae", 1);
		break;
	case EX_FGE:
		gen_fcmp (fn, e, "ae", 0);
		break;
	case EX_PTRADD:
		load (NULL, &fn->regs[e->bin.l]);

		sz = sizeof_dt (fn->regs[e->bin.l].dt.inner);

		if (e->bin.r.is_imm) {
			if (e->bin.r.imm != 0)
				printf ("\tadd ax, %d\n", (int)(e->bin.r.imm & 0xffff) * sz);
		} else {
			loadw (NULL, CX, &fn->regs[e->bin.r.reg]);
			cmul ("cx", sz);
			puts ("\tadd ax, cx");
		}
		break;
	case EX_READ:
		s = loadptr (&fn->regs[e->read.src]);
		switch (dt->type) {
		case DT_NONE:
		case DT_LABEL:
			abort ();
			break;
		case DT_BYTE:
			printf ("\tmov al, byte [%s + %d]\n", s, e->read.off);
			break;
		case DT_WORD:
		case DT_DPTR:
		case DT_SPTR:
			printf ("\tmov ax, word [%s + %d * 2]\n", s, e->read.off);
			break;
		case DT_DWORD:
		case DT_FPTR:
			printf ("\tmov ax, word [%s + %d * 4 + 0]\n", s, e->read.off);
			printf ("\tmov dx, word [%s + %d * 4 + 2]\n", s, e->read.off);
			break;
		case DT_QWORD:
			printf ("\tmov ax, word [%s + %d * 8 + 0]\n", s, e->read.off);
			printf ("\tmov dx, word [%s + %d * 8 + 2]\n", s, e->read.off);
			printf ("\tmov bx, word [%s + %d * 8 + 4]\n", s, e->read.off);
			printf ("\tmov cx, word [%s + %d * 8 + 6]\n", s, e->read.off);
			break;
		case DT_FLOAT:
			printf ("\tfld dword [%s + %d * 4]\n", s, e->read.off);
			break;
		case DT_DOUBLE:
			printf ("\tfld qword [%s + %d * 8]\n", s, e->read.off);
			break;
		case DT_LDOUBLE:
			printf ("\tfld tword [%s + %d * 10]\n", s, e->read.off);
			break;
		}
		break;
	case EX_CALL:
		cnt = 0;
		for (i = e->call->nargs - 1; i >= 0; --i) {
			ri = &e->call->args[i];
			loadri (fn, NULL, ri);
			
			switch (ri->dt->type) {
			case DT_NONE:
				abort ();
				break;
			case DT_BYTE:
			case DT_WORD:
			case DT_DPTR:
			case DT_SPTR:
			case DT_LABEL:
				puts ("\tpush ax");
				break;
			case DT_DWORD:
			case DT_FPTR:
				puts ("\tpush dx");
				puts ("\tpush ax");
				break;
			case DT_QWORD:
				puts ("\tpush cx");
				puts ("\tpush bx");
				puts ("\tpush dx");
				puts ("\tpush ax");
				break;
			case DT_FLOAT:
				puts ("\tsub sp, 4");
				puts ("\tmov bx, sp");
				puts ("\tfstp dword [bx]");
				break;
			case DT_DOUBLE:
				puts ("\tsub sp, 8");
				puts ("\tmov bx, sp");
				puts ("\tfstp qword [bx]");
				break;
			case DT_LDOUBLE:
				puts ("\tsub sp, 10");
				puts ("\tmov bx, sp");
				puts ("\tfstp tword [bx]");
				break;
			}

			cnt += sizeof_dt (ri->dt);
		}
		printf ("\tcall %s\n", e->call->fn);
		if (cnt != 0)
			printf ("\tadd sp, %d\n", cnt);
		break;
	case EX_PHI:
		// TODO: check that all sources actually jump to this phi.
		// It's correct that this is empty,
		// since the value is set in the jumping instruction.
		break;
	case EX_PEXT:
		r = &fn->regs[e->reg];
		loadw (NULL, AX, r);
		switch (r->dt.type) {
		case DT_DPTR:
			puts ("\tmov dx, ds");
			break;
		case DT_SPTR:
			puts ("\tmov dx, ss");
			break;
		default:
			abort ();
		}
		break;
	case EX_ZEXT:
		load (NULL, &fn->regs[e->reg]);
		switch (dt->type) {
		case DT_NONE:
		case DT_LABEL:
		case DT_DPTR:
		case DT_FPTR:
		case DT_SPTR:
		case DT_BYTE:
		case DT_FLOAT:
		case DT_DOUBLE:
		case DT_LDOUBLE:
			abort ();
			break;
		case DT_WORD:
			puts ("\txor ah, ah");
			break;
		case DT_DWORD:
			puts ("\txor dx, dx");
			break;
		case DT_QWORD:
			puts ("\txor bx, bx");
			puts ("\txor cx, cx");
			break;
		}
		break;
	case EX_SEXT:
		r = &fn->regs[e->reg];
		load (NULL, r);
		switch (dt->type) {
		case DT_NONE:
		case DT_LABEL:
		case DT_DPTR:
		case DT_FPTR:
		case DT_SPTR:
		case DT_BYTE:
		case DT_FLOAT:
		case DT_DOUBLE:
		case DT_LDOUBLE:
			abort ();
			break;
		case DT_WORD:
			puts ("\tcbw");
			break;
		case DT_DWORD:
			if (r->dt.type == DT_BYTE) {
				puts ("\tcbw");
			}
			puts ("\tcwd");
			break;
		case DT_QWORD:
			if (r->dt.type == DT_BYTE)
				puts ("\tcbw");
			if (r->dt.type <= DT_WORD)
				puts ("\tcwd");
			puts ("\tmov bx, dx");
			puts ("\tsar bx, 15");
			puts ("\tmov cx, bx");
			break;
		}
		break;
	case EX_TRUNC:
		switch (dt->type) {
		case DT_NONE:
		case DT_LABEL:
		case DT_DPTR:
		case DT_FPTR:
		case DT_SPTR:
		case DT_QWORD:
		case DT_FLOAT:
		case DT_DOUBLE:
		case DT_LDOUBLE:
			abort ();
			break;
		case DT_DWORD:
			loadpw (NULL, AX, &fn->regs[e->reg], 0);
			loadpw (NULL, DX, &fn->regs[e->reg], 2);
			break;
		case DT_WORD:
			loadpw (NULL, AX, &fn->regs[e->reg], 0);
			break;
		case DT_BYTE:
			loadpb (NULL, AL, &fn->regs[e->reg], 0);
			break;
		}
		break;
	case EX_LSL:
		shift (fn, "shl", &fn->regs[e->bin.l], &e->bin.r);
		break;
	case EX_LSR:
		shift (fn, "shr", &fn->regs[e->bin.l], &e->bin.r);
		break;
	case EX_ASR:
		shift (fn, "sar", &fn->regs[e->bin.l], &e->bin.r);
		break;
	}
	return 0;
}

// phi's are crazy. I'm surprised this actually works.
// You are not expected to understand this.
check_phi (fn, from, to)
struct func *fn;
struct ir *from;
struct reg *to;
{
	struct ir *phi;
	struct expr *e;
	int i, mylbl;

	// check for corruption of basic block structure
	assert (from->bhead != NULL && from->bhead->prev == NULL);

	// check that the destination is actually a label
	assert_dt_label (&to->dt);

	// check if this jump can be referenced in a phi
	if (from->bhead->type != IR_LABEL) {
		if (from->bhead->bprev != NULL) {
			// this should never happen
			error ("broken basic block");
		} else {
			// first instruction of a function
			return 0;
		}
	}

	// origin basic block label
	mylbl = from->bhead->reg;

	phi = to->ir->next;
	assert (phi != NULL);
	
	// only the first instruction after a label can be a phi
	if (phi->type != IR_LET || phi->let.val.type != EX_PHI)
		return 0;

	e = &phi->let.val;

	for (i = 0; i < e->phi->num; ++i) {
		if (e->phi->from[i] == mylbl) {
			loadw (NULL, AX, &fn->regs[e->phi->val[i]]);
			return 0;
		}
	}

	return error ("invalid jump to phi");
}

gen_ir (fn, ir)
struct func *fn;
struct ir *ir;
{
	struct reg *r;
	char *reg;

	linenum = ir->line;

	printf ("\t; ");
	print_ir (ir);

	switch (ir->type) {
	case IR_NOP:
		puts ("\tnop");
		break;
	case IR_LET:
		r = &fn->regs[ir->let.dest];
		if (r->type != R_LOCAL)
			goto err;
		gen_expr (fn, &r->dt, &ir->let.val);

		switch (r->dt.type) {
		case DT_NONE:
		case DT_LABEL:
		err:
			error ("broken let");
		case DT_BYTE:
			printf ("\tmov byte [ss:bp + %d], al\n", r->off);
			break;
		case DT_WORD:
		case DT_DPTR:
		case DT_SPTR:
			printf ("\tmov word [ss:bp + %d], ax\n", r->off);
			break;
		case DT_DWORD:
		case DT_FPTR:
			printf ("\tmov word [ss:bp + %d + 0], ax\n", r->off);
			printf ("\tmov word [ss:bp + %d + 2], dx\n", r->off);
			break;
		case DT_QWORD:
			printf ("\tmov word [ss:bp + %d + 0], ax\n", r->off);
			printf ("\tmov word [ss:bp + %d + 2], dx\n", r->off);
			printf ("\tmov word [ss:bp + %d + 4], bx\n", r->off);
			printf ("\tmov word [ss:bp + %d + 6], cx\n", r->off);
			break;
		case DT_FLOAT:
		case DT_DOUBLE:
		case DT_LDOUBLE:
			loadf ("fstp", r);
		}
		break;
	case IR_WRITE:
		reg = loadptr (&fn->regs[ir->write.dest]);

		loadri (fn, NULL, &ir->write.src);
		switch (ir->write.src.dt->type) {
		case DT_NONE:
		case DT_LABEL:
			abort ();
			break;
		case DT_BYTE:
			printf ("\tmov byte [%s + %d], al\n", reg, ir->write.off);
			break;
		case DT_WORD:
		case DT_DPTR:
		case DT_SPTR:
			printf ("\tmov word [%s + %d * 2], ax\n", reg, ir->write.off);
			break;
		case DT_DWORD:
		case DT_FPTR:
			printf ("\tmov word [%s + %d * 4 + 0], ax\n", reg, ir->write.off);
			printf ("\tmov word [%s + %d * 4 + 2], dx\n", reg, ir->write.off);
			break;
		case DT_QWORD:
			printf ("\tmov word [%s + %d * 8 + 0], ax\n", reg, ir->write.off);
			printf ("\tmov word [%s + %d * 8 + 2], dx\n", reg, ir->write.off);
			printf ("\tmov word [%s + %d * 8 + 4], bx\n", reg, ir->write.off);
			printf ("\tmov word [%s + %d * 8 + 6], cx\n", reg, ir->write.off);
			break;
		case DT_FLOAT:
			printf ("\tfstp dword [%s + %d * 4]\n", reg, ir->write.off);
			break;
		case DT_DOUBLE:
			printf ("\tfstp qword [%s + %d * 8]\n", reg, ir->write.off);
			break;
		case DT_LDOUBLE:
			printf ("\tfstp tword [%s + %d * 10]\n", reg, ir->write.off);
			break;
		}
		break;
	case IR_RET:
		assert (ir->next == NULL);

		loadri (fn, NULL, &ir->ri);
		if (ir->bnext != NULL)
			puts ("\tjmp .ret");
		break;
	case IR_LABEL:
		printf (".L%d:\n", ir->reg);
		break;
	case IR_GOTO:
		check_phi (fn, ir, &fn->regs[ir->reg]);
		printf ("\tjmp .L%d\n", ir->reg);
		break;
	case IR_BR:
		r = &fn->regs[ir->br.cond];
		load (NULL, r);

		switch (r->dt.type) {
		case DT_NONE:
		case DT_LABEL:
			abort ();
			break;
		case DT_BYTE:
			puts ("\ttest al, al");
			break;
		case DT_QWORD:
			puts ("\tor ax, bx");
			puts ("\tor ax, cx");
			// fallthrough
		case DT_DWORD:
		case DT_FPTR:
			puts ("\tor ax, dx");
			// fallthrough
		case DT_WORD:
		case DT_DPTR:
		case DT_SPTR:
			puts ("\ttest ax, ax");
			break;
		case DT_FLOAT:
		case DT_DOUBLE:
		case DT_LDOUBLE:
			// TODO
			error ("TODO: IR_BR for floats");
			break;
		}

		check_phi (fn, ir, &fn->regs[ir->br.yes]);
		printf ("\tjnz .L%d\n", ir->br.yes);
		check_phi (fn, ir, &fn->regs[ir->br.no]);
		printf ("\tjmp .L%d\n", ir->br.no);
		break;
	}
	puts ("");
	return 0;
}

gen (fn)
struct func *fn;
{
	struct ir *ir;

	nlbl = 1;

	printf ("section %s\n", fn->section);
	if (fn->is_pub)
		printf ("global %s\n", fn->name);

	printf ("%s:\n", fn->name);
	puts ("\tpush bp");
	puts ("\tmov bp, sp");
	if (fn->stoff != 0)
		printf ("\tsub sp, %d\n", -fn->stoff);
	puts ("");

	for (ir = fn->body; ir != NULL; ir = ir->next ? ir->next : ir->bnext)
		gen_ir (fn, ir);

	puts (".ret:");
	puts ("\tleave");
	puts ("\tret");
	puts ("");

	return 0;
}

// MAIN

main (void)
{
#if ENABLE_DEBUG
	fprintf (stderr, "sizeof (struct func) = %zu\n", sizeof (struct func));
	fprintf (stderr, "sizeof (struct ir) = %zu\n", sizeof (struct ir));
	fprintf (stderr, "sizeof (struct expr) = %zu\n", sizeof (struct expr));
	fprintf (stderr, "sizeof (struct call) = %zu\n", sizeof (struct call));
	fprintf (stderr, "sizeof (struct phi) = %zu\n", sizeof (struct phi));
	fprintf (stderr, "sizeof (struct regimm) = %zu\n", sizeof (struct regimm));
	fprintf (stderr, "sizeof (struct dtype) = %zu\n", sizeof (struct dtype));
#endif
	linenum = 1;
	puts ("[bits 16]");
	puts ("[cpu 286]");
	puts ("extern __shld, __shrd, __sard, __shlq, __shrq, __sarq");
	puts ("extern __muld, __mulq, __divd, __divq, __idivd, __idivq");
	puts ("");
	parse ();

#if ENABLE_DEBUG
	fprintf (stderr, "debug: total memory allocated: %zu bytes\n", total_alloc);
#endif
	return 0;
}
