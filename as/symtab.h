/* special segments */
#define SEG_ABSOLUTE	(-1)
#define SEG_UNDEF	(-2)	/* undefined symbol */
#define SEG_REGISTER	(-3)	/* register */
#define SEG_COMMON	(-4)	/* common block */

struct sym {
	struct sym	*next;	/* next symbol of same name */
	char		*name;
	unsigned long	 value;
	short		 segment;
	unsigned short	 length;
};

unsigned djbhash();
struct sym *lookup();
