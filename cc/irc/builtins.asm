	bits	16

; These functions may not follow the usual ABI.
; It might be necessary to save all modified non-parameter registers.

section .text
global __shld
; dx:ax data, cl: shamt
__shld:	and	cl, 31		; mask shift amount to 32
	je	.ret		; no shift?
	cmp	cl, 16
	jbe	.lo

	; shift by 16 < CL <= 32
	xchg	ax, dx
	sub	cl, 16		; adjust shift amount to 0 < CL <= 16
	shl	dx, cl		; hi << cl - 16
	xor	ax, ax
	ret

	; shift by 0 < CL <= 16
.lo:	push	bx
	mov	bx, ax
	shl	ax, cl		; lo << cl
	shl	dx, cl		; hi << cl
	dec	cx
	xor	cx, 15		; cl = 16 - cl
	shr	bx, cl		; lo >> 16 - cl
	or	dx, bx		; dx:ax = dx:ax << cl
	pop	cx
.ret:	ret

global __shrd
__shrd:	and	cl, 31		; mask shift amount to 32
	je	.ret		; no shift?
	cmp	cl, 16
	jbe	.lo

	; shift by 16 < CL <= 32
	xchg	ax, dx
	sub	cl, 16		; adjust shift amount to 0 < CL <= 16
	shr	ax, cl		; lo >> cl - 16
	xor	dx, dx
	ret

	; shift by 0 < CL <= 16
.lo:	push	bx
	mov	bx, dx
	shr	ax, cl		; lo >> cl
	shr	dx, cl		; hi >> cl
	dec	cx
	xor	cx, 15		; cl = 16 - cl
	shl	bx, cl		; hi << 16 - cl
	or	ax, bx		; dx:ax = dx:ax >> cl
	pop	cx
.ret:	ret

global __sard
__sard:	and	cl, 31		; mask shift amount to 32
	je	.ret		; no shift?
	cmp	cl, 16
	jbe	.lo

	; shift by 16 < CL <= 32
	xchg	ax, dx
	sub	cl, 16		; adjust shift amount to 0 < CL <= 16
	sar	ax, cl		; lo >> cl - 16
	cwd
	ret

	; shift by 0 < CL <= 16
.lo:	push	bx
	mov	bx, dx
	shr	ax, cl		; lo >> cl
	sar	dx, cl		; hi >> cl
	dec	cx
	xor	cx, 15		; cl = 16 - cl
	shl	bx, cl		; hi << 16 - cl
	or	ax, bx		; dx:ax = dx:ax >> cl
	pop	cx
.ret:	ret

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
__muld:	push	bp
	mov	bp, sp
	push	cx
	push	ax
	mov	ax, dx
	mul	word [bp+4]	; hi(A)*lo(B)
	mov	cx, ax
	mov	ax, [bp-4]
	mul	word [bp+6]	; lo(A)*hi(B)
	add	cx, ax		; hi(A)*lo(B)+lo(A)*hi(B)
	pop	ax
	mul	word [bp+4]	; lo(A)*lo(B)
	add	dx, cx		; hi(lo(A)*lo(B))+hi(A)*lo(B)+lo(A)*hi(B)
	pop	cx
	pop	bp
	ret

global __mulq
; cx:bx:dx:ax: val1, stack: val2
__mulq:	push	bp
	mov	bp, sp
	push	cx
	push	bx
	push	dx
	mov	bx, ax
	mul	word [bp+8]	; A[0]*B[2]
	push	dx
	push	ax
	mov	ax, bx
	mul	word [bp+4]	; A[0]*B[0]
	push	dx
	push	ax
	mov	ax, bx
	mul	word [bp+6]	; A[0]*B[3]
	xchg	ax, bx
	mul	word [bp+2]	; A[0]*B[1]
	add	[bp-12], ax
	adc	[bp-10], dx
	adc	[bp-8], bx
	mov	ax, [bp-6]
	mul	word [bp+8]	; A[1]*B[2]
	mov	bx, ax
	mov	ax, [bp-6]
	mul	word [bp+4]	; A[1]*B[0]
	add	[bp-12], ax
	adc	[bp-10], dx
	adc	[bp-8], bx
	mov	ax, [bp-6]
	mul	word [bp+6]	; A[1]*B[1]
	add	[bp-10], ax
	adc	[bp-8], dx
	mov	ax, [bp-4]
	mul	word [bp+6]	; A[2]*B[1]
	mov	cx, ax
	mov	ax, [bp-4]
	mul	word [bp+4]	; A[2]*B[0]
	add	cx, dx
	add	[bp-10], ax
	adc	cx, [bp-8]
	mov	ax, [bp-2]
	mul	word [bp+4]	; A[3]*B[0]
	add	cx, ax
	pop	ax
	pop	dx
	pop	bx
	leave
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
