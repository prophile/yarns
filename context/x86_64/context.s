	.text
	.align  4,0x90
	.globl  ___ctx_gctx
___ctx_gctx:
	leaq 0(%rip), %rax
	movq %rax, (%rdi)
	movq %rbx, 8(%rdi)
	movq %rbp, 16(%rdi)
	movq %r12, 24(%rdi)
	movq %r13, 32(%rdi)
	movq %r14, 40(%rdi)
	movq %r15, 48(%rdi)
	xorq %rax, %rax
	ret

	.align  4,0x90
	.globl ___ctx_sctx
___ctx_sctx:
	movq (%rdi), %rcx
	movq 8(%rdi), %rbx
	movq 16(%rdi), %rbp
	movq 24(%rdi), %r12
	movq 32(%rdi), %r13
	movq 40(%rdi), %r14
	movq 48(%rdi), %r15
	movq %rcx, 8(%rbp)
	movq $1, %rax
	ret

	.align 4,0x90
	.globl ___ctx_start
___ctx_start:
	movq %rbp, %rsp
	movq %r12, %rdi
	movq %r13, %rax
	call *%rax
	ud2
	.subsections_via_symbols
	
     
