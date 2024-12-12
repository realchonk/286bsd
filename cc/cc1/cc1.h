#ifndef FILE_CC1_H
#define FILE_CC1_H

#define IDENT_LEN 8
#define new(T) ((T *)calloc (1, sizeof (T)))
#define cpident(dst, src) (memcpy ((dst), (src), IDENT_LEN + 1))

enum dtype_type {
	DT_CHAR,
	DT_SCHAR,
	DT_UCHAR,
	DT_SHORT,
	DT_USHORT,
	DT_INT,
	DT_UINT,
	DT_LONG,
	DT_ULONG,
	DT_PTR,
	DT_FUNC,
};

struct dtype {
	enum dtype_type type;
	union {
		struct dtype *inner;
	};
};

enum initializer_type {
	IN_NONE,
	IN_EX,
};

struct initializer {
	enum initializer_type type;

	union {
		struct expr *ex;
	};
};

struct symbol {
	struct symbol *next, *xnext;
	struct dtype *dt;
	char ident[IDENT_LEN + 1];

	int sc, reg;
	struct initializer init;
};

struct scope {
	struct symbol *typedefs, *vars;
};

extern struct token {
	char ident[IDENT_LEN + 1];
	struct symbol *sym;
	int i;
} lval;

enum {
	SC_NONE,
	SC_AUTO,
	SC_EXTERN,
	SC_STATIC,
	SC_TYPEDEF,
};

enum {
	BT_CHAR		= 0x01,
	BT_SHORT	= 0x02,
	BT_INT		= 0x04,
	BT_LONG		= 0x08,
	BT_SIGNED	= 0x10,
	BT_UNSIGNED	= 0x20,
};

enum type_spec_type {
	TS_NONE,
	TS_BASIC,
	TS_TYPEIDENT,
};

struct type_spec {
	enum type_spec_type type;
	union {
		int bt;
		struct symbol *sym;
	};
};

struct decl_spec {
	int sc;
	struct type_spec ts;
};

enum expr_type {
	EX_PAR,
	EX_INT,
	EX_CAST,
};

struct expr {
	enum expr_type type;
	struct dtype *dt;

	union {
		struct expr *e;
		int i;

		struct {
			struct dtype *dt;
			struct expr *e;
		} cast;
	};
};

enum stmt_type {
	ST_NULL,
	ST_EXPR,
	ST_LABEL,
};

struct stmt {
	enum stmt_type type;

	union {
		struct expr *ex;
		char ident[IDENT_LEN + 1];
	};
};

struct ident_list {
	struct ident_list *next;
	char ident[IDENT_LEN + 1];
};

enum declarator_type {
	DL_IDENT,
	DL_FUNC,
	DL_PTR,
	DL_FUNCDEF,
};

struct declarator {
	enum declarator_type type;

	union {
		char ident[IDENT_LEN + 1];
		struct declarator *dl;
		struct {
			struct declarator *dl;
			struct ident_list *il;
		} def;
	};
};

struct decl_init {
	struct decl_init *next;
	struct declarator dl;
	struct initializer in;
};

struct declaration {
	int sc;
	struct type_spec ts;
	struct decl_init *head;
};

struct decls_list {
	struct decls_list *next;
	struct declaration d;
};

struct typename {
	struct type_spec ts;
	struct declarator dl;
};

struct function {
	int pub;
	struct dtype *dt;
	char ident[IDENT_LEN + 1];
	struct symbol *ahead, *vhead;
	struct stmt **body;
};

enum irc_type {
	IT_BYTE,	/* char */
	IT_WORD,	/* short, int */
	IT_DWORD,	/* long */
	IT_QWORD,	/* (unused for now) */
};

extern struct scope gscope, fscope;
extern yyerror (), yylex ();
extern reg, linenum;
extern struct expr *cast ();

#endif /* FILE_CC1_H */
