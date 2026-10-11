#include "insn.h"

const struct reg registers[] = {
	"ax", REG_AX,
	"cx", REG_CX,
	"dx", REG_DX,
	"bx", REG_BX,
	"sp", REG_SP,
	"bp", REG_BP,
	"si", REG_SI,
	"di", REG_DI,

	"al", REG_AL,
	"cl", REG_CL,
	"dl", REG_DL,
	"bl", REG_BL,
	"ah", REG_AH,
	"ch", REG_CH,
	"dh", REG_DH,
	"bh", REG_BH,

	"es", REG_ES,
	"cs", REG_CS,
	"ss", REG_SS,
	"ds", REG_DS,

	"flags", REG_FLAGS,
	"ip", REG_IP,
	"gdt", REG_GDT,
	"ldt", REG_LDT,
	"idt", REG_IDT,
	"msw", REG_MSW,
	"tr", REG_TR,

	"st0", REG_ST0,
	"st1", REG_ST1,
	"st2", REG_ST2,
	"st3", REG_ST3,
	"st4", REG_ST4,
	"st5", REG_ST5,
	"st6", REG_ST6,
	"st7", REG_ST7,
};
