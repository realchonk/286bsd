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

global __muld
; dx:ax val1, stack: val2
__muld:
	ret

global __mulq
; cx:bx:dx:ax: val1, stack: val2
__mulq:
	ret

global __divd
; same ABI as __muld
__divd:
	ret

global __divq
; same ABI as __mulq
__divq:
	ret

global __idivd
; same ABI as __muld
__idivd:
	ret

global __idivq
; same ABI as __mulq
__idivq:
	ret
