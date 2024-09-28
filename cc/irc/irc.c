#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>


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

int linenum = 1;

peekch (void)
{
	int ch;
	ch = getchar ();
	ungetc (ch, stdin);
	return ch;
}

nextch (void)
{
	int ch;
	ch = getchar ();
	if (ch == '\n')
		++linenum;
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

error (line, msg)
char *msg;
{
	fprintf (stderr, "%d: error: %s\n", line, msg);
	exit (1);
	return -1;
}

lex (void)
{
	size_t cap, len;
	char *s;
	long i;
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
		s = malloc (cap + 1);

		while (isname (peekch ())) {
			if (len == cap) {
				cap *= 2;
				s = realloc (s, cap + 1);
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
		return error (linenum, "invalid input");
	}
}

// AST DATA TYPES

enum dtype_type {
	DT_NONE,
	DT_BYTE,
	DT_WORD,
	DT_PTR,
	DT_LABEL,
};

struct dtype {
	enum dtype_type type;
	struct dtype *inner;
};

enum expr_type {
	EX_INT,
	EX_ALLOCA,
	EX_ADD,
	EX_READ,
	EX_CALL,
};

struct expr {
	enum expr_type type;
	union {
		int i;
		struct {
			int num;
			int off;
		} alloca;
		struct {
			int l, r;
		} bin;
		struct {
			char *fn;
			int nargs;
			int args[8];
		} call;
	};
};

enum ir_type {
	IR_LET,
	IR_WRITE,
	IR_RET,
	IR_LABEL,
	IR_GOTO,
};

struct ir {
	struct ir *next, *prev;		// next/prev IR in current block
	struct ir *bnext, *bprev;	// next/prev block of IR
	enum ir_type type;
	int line;

	union {
		int reg;
		struct {
			int dest;
			struct dtype dt;
			struct expr val;
		} let;
		struct {
			int dest, src;
		} write;
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
		return error (linenum, "sizeof_dt(DT_NONE)");
	case DT_BYTE:
		return 1;
	case DT_WORD:
	case DT_PTR:
	case DT_LABEL:
		return 2;
	}
}

assert_dt_eq (a, b)
struct dtype *a, *b;
{
	if (a->type != b->type)
		return error (linenum, "type mismatch");

	if (a->type == DT_PTR)
		return assert_dt_eq (a->inner, b->inner);

	return 0;
}

assert_dt_ptr (ptr, to)
struct dtype *ptr, *to;
{
	if (ptr->type != DT_PTR)
		return error (linenum, "not a pointer");

	return assert_dt_eq (ptr->inner, to);
}


// PARSER

#define new(T) ((T *)calloc (1, sizeof (T)))
#define expect(exp) (lex () != (exp) ? error (linenum, "expected " #exp) : 0)
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
	}

	if (tk != TK_IDENT)
		goto err;

	if (strcmp (lval.s, "byte") == 0) {
		dt->type = DT_BYTE;
	} else if (strcmp (lval.s, "word") == 0) {
		dt->type = DT_WORD;
	} else {
	err:
		error (linenum, "expected dtype");
	}
	return 0;
}

expr (fn, dt, e)
struct func *fn;
struct dtype *dt;
struct expr *e;
{
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
			error (linenum, "the result of alloca must be stored in a pointer");

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
		expect (TK_REG);
		e->bin.r = lval.i;
		expect (';');

		assert_dt_eq (dt, &fn->regs[e->bin.l].dt);
		assert_dt_eq (dt, &fn->regs[e->bin.r].dt);
	} else if (strcmp (lval.s, "read") == 0) {
		e->type = EX_READ;
		expect (TK_REG);
		e->i = lval.i;
		expect (';');

		assert_dt_ptr (&fn->regs[e->i].dt, dt);
	} else if (strcmp (lval.s, "call") == 0) {
		e->type = EX_CALL;
		expect (TK_IDENT);
		e->call.fn = lval.s;
		e->call.nargs = 0;

		while ((tk = lex ()) == ',') {
			expect (TK_REG);
			e->call.args[e->call.nargs++] = lval.i;
		}

		if (tk != ';')
			error (linenum, "invalid call");

		// TODO: assert_dt_*
	} else {
	err:
		error (linenum, "expected expr");
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

	ir->line = linenum;

	if (tk == TK_INT) {
		ir->type = IR_LABEL;
		ir->reg = lval.i;
		expect (':');

		r = &fn->regs[ir->reg];
		r->type = R_LABEL;
		r->dt.type = DT_LABEL;
		r->off = ir->reg;
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
		r->type = R_LOCAL;
		r->dt = ir->let.dt;
		stalloc (fn, sizeof_dt (&r->dt));
		r->off = fn->stoff;
		return 0;
	} else if (strcmp (lval.s, "write") == 0) {
		ir->type = IR_WRITE;
		expect (TK_REG);
		ir->write.dest = lval.i;
		expect (',');
		expect (TK_REG);
		ir->write.src = lval.i;
		expect (';');

		assert_dt_ptr (&fn->regs[ir->write.dest].dt, &fn->regs[ir->write.src].dt);
		return 0;
	} else if (strcmp (lval.s, "ret") == 0) {
		ir->type = IR_RET;
		expect (TK_REG);
		ir->reg = lval.i;
		expect (';');

		assert_dt_eq (&fn->dt, &fn->regs[ir->reg].dt);
		return 0;
	} else if (strcmp (lval.s, "goto") == 0) {
		ir->type = IR_GOTO;
		expect (TK_REG);
		ir->reg = lval.i;
		expect (';');
		return 2;
	} else {
	err:
		printf ("%d\n", tk);
		return error (linenum, "expected IR stmt");
	}
}

func (fn)
struct func *fn;
{
	struct ir *ir = NULL;
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
			error (linenum, "invalid parameter list");
		}
	}

rt:
	expect (':');
	dtype (&fn->dt);
	expect ('{');

	fn->stoff = 0;

	while ((tk = lex ()) != '}') {
		if (ir == NULL) {
			ir = new (struct ir);
			fn->body = ir;
		} else {
			ir->next = new (struct ir);
			ir->next->prev = ir;
			ir = ir->next;
		}

		stmt (fn, ir, tk);
	}

	return 0;
}

parse (void)
{
	struct func fn;
	int tk;

	while ((tk = lex ()) != TK_EOF) {
		if (tk != TK_IDENT)
			goto err;

		if (strcmp (lval.s, "fn") == 0) {
			memset (&fn, 0, sizeof (fn));
			func (&fn);
			gen (&fn);
		} else {
		err:
			error (linenum, "expected fn");
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
		error (0, "print_dt(DT_NONE)");
		break;
	case DT_BYTE:
		printf ("byte");
		break;
	case DT_WORD:
		printf ("word");
		break;
	case DT_PTR:
		putchar ('*');
		print_dt (dt->inner);
		break;
	case DT_LABEL:
		printf ("label");
		break;
	}
	return 0;
}

print_ex (e)
struct expr *e;
{
	int i;

	switch (e->type) {
	case EX_INT:
		printf ("%d", e->i);
		break;
	case EX_ALLOCA:
		printf ("alloca %d", e->alloca.num);
		break;
	case EX_ADD:
		printf ("add $%d, $%d", e->bin.l, e->bin.r);
		break;
	case EX_READ:
		printf ("read $%d", e->i);
		break;
	case EX_CALL:
		printf ("call %s", e->call.fn);
		for (i = 0; i < e->call.nargs; ++i)
			printf (", $%d", e->call.args[i]);
		break;
	}
	return 0;
}

print_ir (ir)
struct ir *ir;
{
	switch (ir->type) {
	case IR_LET:
		printf ("let $%d: ", ir->let.dest);
		print_dt (&ir->let.dt);
		printf (" = ");
		print_ex (&ir->let.val);
		puts (";");
		break;
	case IR_WRITE:
		printf ("write $%d, $%d;\n", ir->write.dest, ir->write.src);
		break;
	case IR_RET:
		printf ("ret $%d;\n", ir->reg);
		break;
	case IR_LABEL:
		printf ("%d:\n", ir->reg);
		break;
	case IR_GOTO:
		printf ("goto $%d;\n", ir->reg);
		break;
	}
	return 0;
}

// ASM MISC

enum x86_reg {
	AX,
	BX,
	CX,
	DX,
};

char *
dt_reg (reg, dt)
enum x86_reg reg;
struct dtype *dt;
{
	switch (dt->type) {
	case DT_NONE:
		return error (0, "dt_reg(DT_NONE)"), NULL;
	case DT_BYTE:
		switch (reg) {
		case AX:
			return "al";
		case BX:
			return "bl";
		case CX:
			return "cl";
		case DX:
			return "dl";
		}
		break;
	case DT_WORD:
	case DT_PTR:
	case DT_LABEL:
		switch (reg) {
		case AX:
			return "ax";
		case BX:
			return "bx";
		case CX:
			return "cx";
		case DX:
			return "dx";
		}
		break;
	}
}

char *
dt_size (dt)
struct dtype *dt;
{
	switch (dt->type) {
	case DT_NONE:
		return error (0, "dt_size(DT_NONE)"), NULL;
	case DT_BYTE:
		return "byte";
	case DT_WORD:
	case DT_PTR:
	case DT_LABEL:
		return "word";
	}
}

// ASM GEN

load (dest, src)
enum x86_reg dest;
struct reg *src;
{
	char *reg, *size;

	reg = dt_reg (dest, &src->dt);

	switch (src->type) {
	case R_NONE:
		return error (0, "load(R_NONE)");
	case R_LOCAL:
	case R_PARAM:
		size = dt_size (&src->dt);
		printf ("\tmov %s, %s [bp + %d]\n",
			reg, size, src->off);
		break;
	case R_LABEL:
		printf ("\tlea %s, [.L%d]\n", reg, src->off);
		break;
	}
	return 0;
}

store (dest, src)
struct reg *dest;
enum x86_reg src;
{
	char *reg, *size;

	switch (dest->type) {
	case R_NONE:
		return error (0, "store(R_NONE)");
	case R_LOCAL:
		reg = dt_reg (src, &dest->dt);
		size = dt_size (&dest->dt);
		printf ("\tmov %s [bp + %d], %s\n",
			size, dest->off, reg);
		break;
	case R_PARAM:
		return error (0, "store(R_PARAM)");
	case R_LABEL:
		return error (0, "store(R_PARAM)");
	}
	return 0;
}

gen_expr (fn, dt, e)
struct func *fn;
struct dtype *dt;
struct expr *e;
{
	struct reg *r;
	int i, cnt;

	switch (e->type) {
	case EX_INT:
		printf ("\tmov ax, %d\n", e->i);
		break;
	case EX_ALLOCA:
		printf ("\tlea ax, [bp + %d]\n", e->alloca.off);
		break;
	case EX_ADD:
		load (AX, &fn->regs[e->bin.l]);
		load (DX, &fn->regs[e->bin.l]);
		puts ("\tadd ax, dx");
		break;
	case EX_READ:
		load (BX, &fn->regs[e->i]);
		printf ("\tmov %s, %s [bx]\n", dt_reg (AX, dt), dt_size (dt));
		break;
	case EX_CALL:
		cnt = 0;
		for (i = e->call.nargs - 1; i >= 0; --i) {
			r = &fn->regs[e->call.args[i]];
			
			switch (r->dt.type) {
			case DT_NONE:
				error (0, "gen_expr(): call with DT_NONE");
				break;
			case DT_BYTE:
			case DT_WORD:
			case DT_PTR:
			case DT_LABEL:
				load (AX, r);
				puts ("\tpush ax");
				break;
			}

			cnt += sizeof_dt (&r->dt);
		}
		printf ("\tcall %s\n", e->call.fn);
		printf ("\tadd sp, %d\n", cnt);
		break;
	}
	return 0;
}

gen_ir (fn, ir)
struct func *fn;
struct ir *ir;
{
	struct dtype *dt;

	printf ("\t; ");
	print_ir (ir);

	switch (ir->type) {
	case IR_LET:
		gen_expr (fn, &fn->regs[ir->let.dest].dt, &ir->let.val);
		store (&fn->regs[ir->let.dest], AX);
		break;
	case IR_WRITE:
		load (AX, &fn->regs[ir->write.src]);
		load (BX, &fn->regs[ir->write.dest]);
		dt = &fn->regs[ir->write.src].dt;
		printf ("\tmov %s [bx], %s\n", dt_size (dt), dt_reg (AX, dt));
		break;
	case IR_RET:
		load (AX, &fn->regs[ir->reg]);
		if (ir->next != NULL)
			puts ("\tjmp .ret");
		break;
	case IR_LABEL:
		printf (".L%d:\n", ir->reg);
		break;
	case IR_GOTO:
		printf ("\tjmp .L%d\n", ir->reg);
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

	for (ir = fn->body; ir != NULL; ir = ir->next)
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
	parse ();
	return 0;
}
