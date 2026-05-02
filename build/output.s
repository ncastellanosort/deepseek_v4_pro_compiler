	.bss
	.comm	x, 8, 8
	.section .rodata
fmtD:
	.string "%ld\n"
fmtS:
	.string "%s\n"
	.text
	.extern	printf
	.globl	main
main:
	pushq	%rbp
	movq	%rsp, %rbp
	movq	$5, %rax
	movq	%rax, x(%rip)
	movq	x(%rip), %rax
	pushq	%rax
	movq	$3, %rax
	popq	%rcx
	addq	%rcx, %rax
	movq	%rax, %rsi
	leaq	fmtD(%rip), %rdi
	xorl	%eax, %eax
	call	printf@PLT
	movq	x(%rip), %rax
	cmpq	$0,%rax
	je	.L0
	movq	$1, %rax
	movq	%rax, %rsi
	leaq	fmtD(%rip), %rdi
	xorl	%eax, %eax
	call	printf@PLT
	jmp	.L1
.L0:
	movq	$0, %rax
	movq	%rax, %rsi
	leaq	fmtD(%rip), %rdi
	xorl	%eax, %eax
	call	printf@PLT
.L1:
	movq	$0, %rax
	leave
	ret
