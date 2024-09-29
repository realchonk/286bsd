#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <err.h>

// use an arena to speed up memory allocations
#define USE_ARENA 1

// MEMORY

#if USE_ARENA
char arena[32768];
size_t alen = 0;

void *
alloc (num)
size_t num;
{
	void *ptr;

	// TODO: consider returning heap memory
	if (alen + num > sizeof (arena))
		errx (1, "out of memory");

	ptr = (void *)((size_t)arena + alen);
	alen += num;
	memset (ptr, 0, num);
	return ptr;
}

reset (void)
{
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
	TK_REG,
	TK_IDENT,
	TK_EOF,
};

union {
	long i;
	char *s;
} lval;

int linenum = 1, peekd = EOF;

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

isname (ch)
{
	return isalnum (ch) || ch == '_';
}

error (msg)
char *msg;
{
	errx (1, "%d: error: %s", linenum, msg);
	return -1;
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
		parse_int ();
		return TK_INT;
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
	DT_BYTE,
	DT_WORD,
	DT_DWORD,
	DT_PTR,
	DT_FPTR,
	DT_LABEL,
	DT_QWORD,
};

struct dtype {
	enum dtype_type type;
	struct dtype *inner;
} default_dt = {
	DT_WORD,
	NULL	
};


struct regimm {
	int is_imm;
	struct dtype *dt;
	union {
		int reg;
		long imm;
	};
};

enum expr_type {
	EX_INT,
	EX_ALLOCA,
	EX_ADD,
	EX_READ,
	EX_CALL,
	EX_PHI,
	EX_PTRADD,
};

struct expr {
	enum expr_type type;
	union {
		int reg;
		long i;
		struct {
			int num;
			int off;
		} alloca;
		struct {
			int l;
			struct regimm r;
		} bin;
		struct {
			char *fn;
			int nargs;
			struct regimm args[8];
		} call;
		struct {
			int from[8], val[8], num;
		} phi;
	};
};

enum ir_type {
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

#define NREGS 128
struct func {
	char *name;
	struct dtype dt;
	struct reg regs[NREGS];
	struct ir *body;
	int stoff;
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
	case DT_PTR:
	case DT_LABEL:
		return 2;
	case DT_DWORD:
	case DT_FPTR:
		return 4;
	case DT_QWORD:
		return 8;
	}
}

assert_dt_eq (a, b)
struct dtype *a, *b;
{
	if (a->type != b->type)
		return error ("type mismatch");

	if (a->type == DT_PTR)
		return assert_dt_eq (a->inner, b->inner);

	return 0;
}

assert_dt_is_ptr (ptr)
struct dtype *ptr;
{
	if (ptr->type != DT_PTR && ptr->type != DT_FPTR)
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
		dt->type = DT_PTR;
		dt->inner = new (struct dtype);
		return dtype (dt->inner);
	} else if (tk == '!') {
		dt->type = DT_FPTR;
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

expr (fn, dt, e)
struct func *fn;
struct dtype *dt;
struct expr *e;
{
	struct regimm *ri;
	int tk;

	tk = lex ();

	if (tk == TK_INT) {
		e->type = EX_INT;
		e->i = lval.i;
		expect (';');
		return 0;
	}

	if (tk != TK_IDENT)
		goto err;

	if (strcmp (lval.s, "alloca") == 0) {
		if (dt->type != DT_PTR)
			error ("the result of alloca must be stored in a pointer");

		e->type = EX_ALLOCA;
		expect (TK_INT);
		e->alloca.num = lval.i;
		stalloc (fn, sizeof_dt (dt->inner) * e->alloca.num);
		e->alloca.off = fn->stoff;
		expect (';');
	} else if (strcmp (lval.s, "add") == 0) {
		e->type = EX_ADD;
		expect (TK_REG);
		e->bin.l = lval.i;
		expect (',');
		regimm (&e->bin.r, &fn->regs[e->bin.l].dt);
		expect (';');

		assert_dt_eq (dt, &fn->regs[e->bin.l].dt);
		assert_dt_eq (dt, e->bin.r.dt);
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
		e->reg = lval.i;
		expect (';');

		assert_dt_ptr (&fn->regs[e->i].dt, dt);
	} else if (strcmp (lval.s, "call") == 0) {
		e->type = EX_CALL;
		expect (TK_IDENT);
		e->call.fn = lval.s;
		e->call.nargs = 0;

		while ((tk = lex ()) == ',') {
			ri = &e->call.args[e->call.nargs++];
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
		e->phi.from[0] = lval.i;
		expect (':');
		expect (TK_REG);
		e->phi.val[0] = lval.i;
		expect (')');
		e->phi.num = 1;

		while ((tk = lex ()) == ',') {
			expect ('(');
			expect (TK_REG);
			e->phi.from[e->phi.num] = lval.i;
			expect (':');
			expect (TK_REG);
			e->phi.val[e->phi.num] = lval.i;
			expect (')');
			++e->phi.num;
		}
		
		if (tk != ';')
			error ("invalid phi");

		// TODO: assert_dt_*
		// TODO: check that there are no instructions (except IR_LABEL) before this
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

	if (strcmp (lval.s, "let") == 0) {
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
		expect (';');

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
	int tk;

	while ((tk = lex ()) != TK_EOF) {
		if (tk != TK_IDENT)
			goto err;

		if (strcmp (lval.s, "fn") == 0) {
			memset (&fn, 0, sizeof (fn));
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
	case DT_PTR:
		putchar ('*');
		print_dt (dt->inner);
		break;
	case DT_FPTR:
		putchar ('!');
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
	case EX_ALLOCA:
		printf ("alloca %d", e->alloca.num);
		break;
	case EX_ADD:
		printf ("add $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_PTRADD:
		printf ("ptradd $%d, ", e->bin.l);
		print_ri (&e->bin.r);
		break;
	case EX_READ:
		printf ("read $%d", e->reg);
		break;
	case EX_CALL:
		printf ("call %s", e->call.fn);
		for (i = 0; i < e->call.nargs; ++i) {
			printf (", ");
			print_ri (&e->call.args[i]);
		}
		break;
	case EX_PHI:
		printf ("phi ($%d: $%d)", e->phi.from[0], e->phi.val[0]);
		for (i = 1; i < e->phi.num; ++i)
			printf (", ($%d: $%d)", e->phi.from[i], e->phi.val[i]);
		break;
	}
	return 0;
}

print_ir (ir)
struct ir *ir;
{
	printf ("%d: ", ir->line);
	switch (ir->type) {
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
		puts (";");
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
};

char *reg8[] = { "al", "bl", "cl", "dl" };
char *reg16[] = { "ax", "bx", "cx", "dx" };

// ASM GEN

loadb (instr, dest, src)
char *instr;
enum x86_reg8 dest;
struct reg *src;
{
	assert (sizeof_dt (&src->dt) == 1);
	switch (src->type) {
	case R_NONE:
	case R_LABEL:
		abort ();
		break;
	case R_LOCAL:
	case R_PARAM:
		printf ("\t%s %s, byte [bp + %d]\n",
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
	switch (src->type) {
	case R_NONE:
		abort ();
		break;
	case R_LOCAL:
	case R_PARAM:
		printf ("\t%s %s, word [bp + %d]\n",
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
	switch (src->type) {
	case R_NONE:
	case R_LABEL:
		abort ();
		break;
	case R_LOCAL:
	case R_PARAM:
		printf ("\t%s %s, word [bp + %d + %d]\n",
			instr, reg16[dest], src->off, off);
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
	case DT_PTR:
	case DT_LABEL:
		return loadw (instr, AX, src);
	case DT_DWORD:
	case DT_FPTR:
		return loaddw (instr, src);
	case DT_QWORD:
		return loadqw (instr, src);
	}
}

char *
loadptr (ptr)
struct reg *ptr;
{
	switch (ptr->dt.type) {
	case DT_PTR:
		loadw ("mov", BX, ptr);
		return "bx";
	case DT_FPTR:
		loadpw ("mov", BX, ptr, 0);
		loadpw ("mov", DX, ptr, 2);
		puts ("\tmov es, dx");
		return "es:bx";
	default:
		return abort (), NULL;
	}
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
		printf ("\t%s al, %d\n", instr, (int)imm);
		break;
	case DT_WORD:
	case DT_PTR:
		printf ("\t%s ax, %d\n", instr, (int)imm);
		break;
	case DT_DWORD:
	case DT_FPTR:
		printf ("\t%s ax, %d\n", instr, (int)(imm & 0xffff));
		printf ("\t%s dx, %d\n", instr, (int)(imm >> 16));
		break;
	case DT_QWORD:
		printf ("\t%s ax, %d\n", instr, (int)(imm & 0xffff));
		printf ("\t%s dx, %d\n", instr, (int)((imm >> 16) & 0xffff));
		printf ("\t%s bx, %d\n", instr, (int)((imm >> 32) & 0xffff));
		printf ("\t%s cx, %d\n", instr, (int)(imm >> 48));
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
enum x86_reg16 reg;
{
	char *r;
	int x;

	r = reg16[reg];

	x = ffs (i);
	if (x == 0) {
		printf ("\txor %s, %s\n", r, r);
		return 0;
	}

	if (i == (1 << (x - 1))) {
		printf ("\tshr %s, %d\n", r, (x - 1));
		return 0;
	}

	error ("cmul() not implemented for arbitrary constants");
	return 0;
}

gen_expr (fn, dt, e)
struct func *fn;
struct dtype *dt;
struct expr *e;
{
	struct regimm *ri;
	char *s;
	int i, cnt, sz;

	switch (e->type) {
	case EX_INT:
		loadimm ("mov", dt, e->i);
		break;
	case EX_ALLOCA:
		assert (dt->type == DT_PTR);
		printf ("\tlea ax, [bp + %d]\n", e->alloca.off);
		break;
	case EX_ADD:
		load ("mov", &fn->regs[e->bin.l]);
		switch (dt->type) {
		case DT_NONE:
		case DT_LABEL:
			abort ();
			break;
		case DT_BYTE:
		case DT_WORD:
			loadri (fn, "add", &e->bin.r);
			break;
		case DT_DWORD:
			loadpwri (fn, "add", AX, &e->bin.r, 0);
			loadpwri (fn, "adc", DX, &e->bin.r, 2);
			break;
		case DT_QWORD:
			loadpwri (fn, "add", AX, &e->bin.r, 0);
			loadpwri (fn, "adc", DX, &e->bin.r, 2);
			loadpwri (fn, "adc", BX, &e->bin.r, 4);
			loadpwri (fn, "adc", CX, &e->bin.r, 6);
			break;
		case DT_PTR:
		case DT_FPTR:
			error ("cannot add pointers");
			break;
		}
		break;
	case EX_PTRADD:
		load ("mov", &fn->regs[e->bin.l]);

		sz = sizeof_dt (fn->regs[e->bin.l].dt.inner);

		if (e->bin.r.is_imm) {
			if (e->bin.r.imm != 0)
				printf ("\tadd ax, %d\n", (int)(e->bin.r.imm & 0xffff) * sz);
		} else {
			loadw ("mov", CX, &fn->regs[e->bin.r.reg]);
			cmul (CX, sz);
			puts ("\tadd ax, cx");
		}
		break;
	case EX_READ:
		s = loadptr (&fn->regs[e->reg]);
		switch (dt->type) {
		case DT_NONE:
		case DT_LABEL:
			abort ();
			break;
		case DT_BYTE:
			printf ("\tmov al, byte [%s]\n", s);
			break;
		case DT_WORD:
		case DT_PTR:
			printf ("\tmov ax, word [%s]\n", s);
			break;
		case DT_DWORD:
		case DT_FPTR:
			printf ("\tmov ax, word [%s + 0]\n", s);
			printf ("\tmov dx, word [%s + 2]\n", s);
			break;
		case DT_QWORD:
			printf ("\tmov ax, word [%s + 0]\n", s);
			printf ("\tmov dx, word [%s + 2]\n", s);
			printf ("\tmov bx, word [%s + 4]\n", s);
			printf ("\tmov cx, word [%s + 6]\n", s);
			break;
		}
		break;
	case EX_CALL:
		cnt = 0;
		for (i = e->call.nargs - 1; i >= 0; --i) {
			ri = &e->call.args[i];
			loadri (fn, "mov", ri);
			
			switch (ri->dt->type) {
			case DT_NONE:
				abort ();
				break;
			case DT_BYTE:
			case DT_WORD:
			case DT_PTR:
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
			}

			cnt += sizeof_dt (ri->dt);
		}
		printf ("\tcall %s\n", e->call.fn);
		printf ("\tadd sp, %d\n", cnt);
		break;
	case EX_PHI:
		// TODO: check that all sources actually jump to this phi.
		// It's correct that this is empty,
		// since the value is set in the jumping instruction.
		break;
	}
	return 0;
}

// phi's are crazy. I'm surprised this actually works.
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

	for (i = 0; i < e->phi.num; ++i) {
		if (e->phi.from[i] == mylbl) {
			loadw ("mov", AX, &fn->regs[e->phi.val[i]]);
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
			printf ("\tmov byte [bp + %d], al\n", r->off);
			break;
		case DT_WORD:
		case DT_PTR:
			printf ("\tmov word [bp + %d], ax\n", r->off);
			break;
		case DT_DWORD:
		case DT_FPTR:
			printf ("\tmov word [bp + %d + 0], ax\n", r->off);
			printf ("\tmov word [bp + %d + 2], dx\n", r->off);
			break;
		case DT_QWORD:
			printf ("\tmov word [bp + %d + 0], ax\n", r->off);
			printf ("\tmov word [bp + %d + 2], dx\n", r->off);
			printf ("\tmov word [bp + %d + 4], bx\n", r->off);
			printf ("\tmov word [bp + %d + 6], cx\n", r->off);
			break;
		}
		break;
	case IR_WRITE:
		reg = loadptr (&fn->regs[ir->write.dest]);

		loadri (fn, "mov", &ir->write.src);
		switch (ir->write.src.dt->type) {
		case DT_NONE:
		case DT_LABEL:
			abort ();
			break;
		case DT_BYTE:
			printf ("\tmov byte [%s], al\n", reg);
			break;
		case DT_WORD:
		case DT_PTR:
			printf ("\tmov word [%s], ax\n", reg);
			break;
		case DT_DWORD:
		case DT_FPTR:
			printf ("\tmov word [%s + 0], ax\n", reg);
			printf ("\tmov word [%s + 2], dx\n", reg);
			break;
		case DT_QWORD:
			printf ("\tmov word [%s + 0], ax\n", reg);
			printf ("\tmov word [%s + 2], dx\n", reg);
			printf ("\tmov word [%s + 4], bx\n", reg);
			printf ("\tmov word [%s + 6], cx\n", reg);
			break;
		}
		break;
	case IR_RET:
		assert (ir->next == NULL);

		loadri (fn, "mov", &ir->ri);
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
		load ("mov", r);

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
		case DT_PTR:
			puts ("\ttest ax, ax");
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

	printf ("%s:\n", fn->name);
	puts ("\tpush bp");
	puts ("\tmov bp, sp");
	printf ("\tsub sp, %d\n", -fn->stoff);
	puts ("");

	for (ir = fn->body; ir != NULL; ir = ir->next ? ir->next : ir->bnext)
		gen_ir (fn, ir);

	puts (".ret:");
	printf ("\tadd sp, %d\n", -fn->stoff);
	puts ("\tpop bp");
	puts ("\tret");
	puts ("");

	return 0;
}

// MAIN

main (void)
{
	puts ("[bits 16]");
	puts ("[cpu 286]");
	puts ("");
	parse ();
	return 0;
}
