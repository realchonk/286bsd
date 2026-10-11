#define NAME_INSN	001	/* instruction */
#define	NAME_REG	002	/* register */
#define	NAME_SYM	004	/* symbol table entry */
#define	NAME_LBL	010	/* function-local label */
#define NAME_SYMLBL	(NAME_SYM|NAME_LBL)
#define NAME_ANY	(NAME_INSN|NAME_REG|NAME_SYM|NAME_LBL)

struct name {
	struct name	 *next;	/* next symbol of same hash */
	char		**sym;	/* symbol structure (first member is name pointer) */
	unsigned char	  type;
};

struct name	 *name_lookup();
char		**name_next();
void		  name_put();

extern int name_fill;
