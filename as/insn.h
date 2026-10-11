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

/* register/operand groups */
#define REGGR_WORD	0
#define REGGR_BYTE	1
#define REGGR_SEGMENT	2
#define REGGR_SPECIAL	3
#define REGGR_ST	4

#define REGGR_MEMORY	5	/* memory operand */
#define REGGR_IMMED	6	/* immediate operand */
#define REGGR_INDIR	7	/* indirect operand */

/* REGGR_WORD */
#define REG_AX		000
#define REG_CX		001
#define REG_DX		002
#define REG_BX		003
#define	REG_SP		004
#define REG_BP		005
#define REG_SI		006
#define REG_DI		007

/* REGGR_BYTE */
#define REG_AL		010
#define REG_CL		011
#define REG_DL		012
#define REG_BL		013
#define REG_AH		014
#define REG_CH		015
#define REG_DH		016
#define REG_BH		017

/* REGGR_SEGMENT */
#define REG_ES		020
#define REG_CS		021
#define REG_SS		022
#define REG_DS		023

/* REGGR_SPECIAL */
#define REG_FLAGS	030
#define	REG_IP		031
#define REG_GDT		032
#define REG_LDT		033
#define REG_IDT		034
#define	REG_MSW		035
#define REG_TR		036

/* REGGR_ST */
#define REG_ST0		040
#define REG_ST1		041
#define	REG_ST2		042
#define REG_ST3		043
#define REG_ST4		044
#define REG_ST5		045
#define REG_ST6		046
#define REG_ST7		047

/* operand types */
#define OPR_WORD	0x0001	/* a word register */
#define OPR_BYTE	0x0002	/* a byte register */
#define OPR_SEG		0x0004	/* a segment register */
#define OPR_SPECIAL	0x0008	/* a special register */
#define OPR_ST		0x0010	/* any floating-point register */
#define OPR_MEM		0x0020	/* a memory operand */
#define	OPR_IMM		0x0040	/* an immediate operand */
#define OPR_IND		0x0080	/* an indirect operand */
#define OPR_SEGWORD	0x0100	/* a seg:reg pair */
#define OPR_DXAX	0x0200	/* the pair DX:AX */
#define	OPR_AHAL	0x0400	/* the pair AH:AL */

/* instruction types */
#define INSN_PLUSR	0	/* op +reg	op reg */
#define INSN_PLUSS	1	/* op +seg	op seg */
#define INSN_RM		1	/* op /r	op reg, r/m */
#define INSN_MR		2	/* op /r	op r/m, reg */
#define INSN_M		3	/* op /?	op r/m */
#define INSN_MI8	5	/* op /? imm8	op r/m, imm8 */
#define INSN_MI16	6	/* op /? imm16	op r/m, imm16 */
#define INSN_M16	7	/* op mem16	op mem16 */
#define INSN_J8		7	/* op rel8	op label */
#define	INSN_J16	8	/* op rel16	op label */

/*
struct operand {
	struct sym *base;
	unsigned long disp;
	unsigned short index;
} */

struct reg {
	const char	*name;
	const char	 reg;
}

struct insn {
	const char	*name
	unsigned short	 op;
	unsigned char	 opext;
	unsigned char	 type;
	unsigned short	 arg1, arg2, arg3;
}
