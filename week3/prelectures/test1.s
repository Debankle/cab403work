	.file	"test1.c"
	.intel_syntax noprefix
	.text
	.globl	x
	.data
	.align 4
	.type	x, @object
	.size	x, 4
x:
	.long	1
	.text
	.globl	add
	.type	add, @function
add:
.LFB0:
	.cfi_startproc
	endbr64
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	mov	rbp, rsp
	.cfi_def_cfa_register 6
	mov	DWORD PTR -4[rbp], edi
	mov	DWORD PTR -8[rbp], esi
	mov	edx, DWORD PTR -4[rbp]
	mov	eax, DWORD PTR -8[rbp]
	add	eax, edx
	pop	rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE0:
	.size	add, .-add
	.globl	cringe
	.type	cringe, @function
cringe:
.LFB1:
	.cfi_startproc
	endbr64
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	mov	rbp, rsp
	.cfi_def_cfa_register 6
	mov	edx, DWORD PTR x[rip]
	mov	eax, DWORD PTR y.0[rip]
	add	eax, edx
	mov	DWORD PTR x[rip], eax
	mov	eax, DWORD PTR y.0[rip]
	add	eax, 1
	mov	DWORD PTR y.0[rip], eax
	nop
	pop	rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE1:
	.size	cringe, .-cringe
	.section	.rodata
.LC0:
	.string	"%d\n"
	.text
	.globl	main
	.type	main, @function
main:
.LFB2:
	.cfi_startproc
	endbr64
	push	rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	mov	rbp, rsp
	.cfi_def_cfa_register 6
	sub	rsp, 16
	mov	eax, 0
	call	cringe
	mov	DWORD PTR -16[rbp], 1
	mov	DWORD PTR -12[rbp], 2
	mov	edx, DWORD PTR -12[rbp]
	mov	eax, DWORD PTR -16[rbp]
	mov	esi, edx
	mov	edi, eax
	call	add
	mov	DWORD PTR -8[rbp], eax
	mov	eax, DWORD PTR -8[rbp]
	mov	esi, eax
	lea	rax, .LC0[rip]
	mov	rdi, rax
	mov	eax, 0
	call	printf@PLT
	mov	eax, 0
	call	cringe
	mov	eax, DWORD PTR x[rip]
	mov	edx, DWORD PTR -16[rbp]
	mov	esi, edx
	mov	edi, eax
	call	add
	mov	DWORD PTR -4[rbp], eax
	mov	eax, DWORD PTR -4[rbp]
	mov	esi, eax
	lea	rax, .LC0[rip]
	mov	rdi, rax
	mov	eax, 0
	call	printf@PLT
	mov	eax, 0
	call	cringe
	mov	eax, DWORD PTR x[rip]
	mov	edx, DWORD PTR -16[rbp]
	mov	esi, edx
	mov	edi, eax
	call	add
	mov	DWORD PTR -4[rbp], eax
	mov	eax, DWORD PTR -4[rbp]
	mov	esi, eax
	lea	rax, .LC0[rip]
	mov	rdi, rax
	mov	eax, 0
	call	printf@PLT
	mov	eax, 0
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE2:
	.size	main, .-main
	.local	y.0
	.comm	y.0,4,4
	.ident	"GCC: (Ubuntu 13.2.0-23ubuntu4) 13.2.0"
	.section	.note.GNU-stack,"",@progbits
	.section	.note.gnu.property,"a"
	.align 8
	.long	1f - 0f
	.long	4f - 1f
	.long	5
0:
	.string	"GNU"
1:
	.align 8
	.long	0xc0000002
	.long	3f - 2f
2:
	.long	0x3
3:
	.align 8
4:
