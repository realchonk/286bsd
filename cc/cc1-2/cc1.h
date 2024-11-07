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

struct symbol {
	struct symbol *next;
	struct dtype *dt;
	char ident[IDENT_LEN + 1];
};

struct scope {
	struct scope *parent;
	struct symbol *typedefs;
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
	EX_INT,
};

struct expr {
	enum expr_type type;

	union {
		int i;
	};
};

enum declarator_type {
	DL_IDENT,
	DL_FUNC,
	DL_PTR,
};

struct declarator {
	enum declarator_type type;

	union {
		char ident[IDENT_LEN + 1];
		struct declarator *dl;
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

extern struct scope *scope;
extern yyerror (), yylex ();

#endif // FILE_CC1_H
