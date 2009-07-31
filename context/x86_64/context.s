	.text
	.align  4,0x90
	.globl  ___ctx_gctx
___ctx_gctx:
	xorq %rax, %rax
	movq %rdi, %rcx
	movq %rbx, 8(%rcx)
	movq %rbp, 16(%rcx)
	movq %rsi, 24(%rcx)
	movq %rsp, 32(%rcx)
	addq $8, 32(%rcx)
	movq (%rsp), %rdx
	movq %rdx, 0(%rcx)
	ret

	.align  4,0x90
	.globl ___ctx_sctx
___ctx_sctx:
	movq $1, %rax
	movq %rdi, %rcx
	movq 8(%rcx), %rbx
	movq 16(%rcx), %rbp
	movq 24(%rcx), %rsi
	movq 32(%rcx), %rsp
	pushq 0(%rcx)
	ret

	.align 4,0x90
	.globl ___ctx_start
___ctx_start:
	popq %rcx
	movq 40(%rdi), %rdi
	call *%rcx
	int $5
	.subsections_via_symbols
	
     