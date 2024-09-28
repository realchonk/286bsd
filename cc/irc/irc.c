#include <assert.h>
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
	EX_PHI,
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
		struct {
			int from[8], val[8], num;
		} phi;
	};
};

enum ir_type {
	IR_LET,
	IR_WRITE,
	IR_RET,
	IR_LABEL,
	IR_GOTO,
	IR_BR,
};

struct ir {
	struct ir *next, *prev;		// next/prev IR in current block
	struct ir *bnext, *bprev;	// next/prev block of IR
	struct ir *bhead;		// head of current IR block
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

assert_dt_label (line, dt)
struct dtype *dt;
{
	if (dt->type != DT_LABEL)
		return error (line, "not a label");
	return 0;
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
			error (linenum, "invalid phi");

		// TODO: assert_dt_*
		// TODO: check that there are no instructions (except IR_LABEL) before this
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
		if (r->type != R_NONE)
			error (linenum, "cannot reassign to register");
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
			error (linenum, "cannot reassign to register");
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
		return error (linenum, "expected IR stmt");
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
			error (linenum, "invalid parameter list");
		}
	}

rt:
	expect (':');
	dtype (&fn->dt);
	expect ('{');

	fn->stoff = 0;

	while ((tk = lex ()) != '}') {
		ir = new (struct ir);

		if (fn->body == NULL) {
			fn->body = ir;
			bhead = ir;
		}

		switch (stmt (fn, ir, tk)) {
		case 0: // normal IR
			if (bhead == NULL)
				error (linenum, "dead instruction");

			ir->prev = prev;
			ir->bprev = bprev;
			ir->bhead = bhead;

			if (prev != NULL)
				prev->next = ir;

			prev = ir;
			break;
		case 1: // end of basic block
			if (prev == NULL || bhead == NULL)
				error (linenum, "invalid end of a basic block");

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
				error (linenum, "invalid start of a basic block");

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
	case IR_BR:
		printf ("br $%d, $%d, $%d;\n", ir->br.cond, ir->br.yes, ir->br.no);
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
		return error (0, "store(R_LABEL)");
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
	assert_dt_label (from->line, &to->dt);

	// check if this jump can be referenced in a phi
	if (from->bhead->type != IR_LABEL) {
		if (from->bhead->bprev != NULL) {
			// this should never happen
			error (from->line, "broken basic block");
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
			load (AX, &fn->regs[e->phi.val[i]]);
			return 0;
		}
	}

	return error (from->line, "invalid jump to phi");
}

gen_ir (fn, ir)
struct func *fn;
struct ir *ir;
{
	struct dtype *dt;
	char *reg;

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
		assert (ir->next == NULL);
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
		load (AX, &fn->regs[ir->br.cond]);
		reg = dt_reg (AX, &fn->regs[ir->br.cond].dt);
		printf ("\ttest %s, %s\n", reg, reg);
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
	parse ();
	return 0;
}
