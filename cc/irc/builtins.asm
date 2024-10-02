; These functions may not follow the usual ABI.
; It might be necessary to save all modified non-parameter registers.

section .text
global __shld
; dx:ax data, cl: shamt
__shld:
	ret

global __shrd
__shrd:
	ret

global __sard
__sard:
	ret

global __shlq
; cx:bx:dx:ax data, stack: shamt
__shlq:
	ret

global __shrq
__shrq:
	ret

global __sarq
__sarq:
	ret
