#define INDEX_BX	0x0001
#define INDEX_BP	0x0002
#define INDEX_SI	0x0004
#define INDEX_DI	0x0008
#define INDEX_ES	0x0010
#define INDEX_CS	0x0020
#define INDEX_SS	0x0040
#define INDEX_DS	0x0080
#define INDEX_INDIR	0x0100 /* * operand */
#define INDEX_IMMED	0x0200 /* $ operand */

/* register/opernad groups */
#define REGGR_WORD	0
#define REGGR_BYTE	1
#define REGGR_SEGMENT	2
#define REGGR_FP	3
#define REGGR_SPECIAL	4
#define REGGR_MEMORY	5	/* memory operand */

/* REGGR_WORD */
#define REG_AX		0
#define REG_CX		1
#define REG_DX		2
#define REG_BX		3
#define	REG_SP		4
#define REG_BP		5
#define REG_SI		6
#define REG_DI		7

/* REGGR_BYTE */
#define REG_AL		0
#define REG_CL		1
#define REG_DL		2
#define REG_BL		3
#define REG_AH		4
#define REG_CH		5
#define REG_DH		6
#define REG_BH		7

/* REGGR_SEGMENT */
#define REG_ES		0
#define REG_CS		1
#define REG_SS		2
#define REG_DS		3

/* REGGR_SPECIAL */
#define REG_FLAGS	0
#define	REG_IP		1
#define REG_GDT		2
#define REG_LDT		3
#define REG_IDT		4
#define	REG_MSW		5
#define REG_TR		6

struct operand {
	struct sym *base;
	unsigned long disp;
	unsigned short index;
}

struct insn {
	struct insn *next;
	char mnemo[8];
	
