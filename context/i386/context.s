	.text
	.align  4,0x90
	.globl  ___ctx_gctx
___ctx_gctx:
	xorl %eax, %eax
	movl 4(%esp), %ecx
	movl %ebx, 4(%ecx)
	movl %ebp, 8(%ecx)
	movl %esi, 12(%ecx)
	movl %edi, 16(%ecx)
	movl %esp, 20(%ecx)
	addl $4, 20(%ecx)
	movl (%esp), %edx
	movl %edx, 0(%ecx)
	ret

	.align  4,0x90
	.globl ___ctx_sctx
___ctx_sctx:
	movl $1, %eax
	movl 4(%esp), %ecx
	movl 4(%ecx), %ebx
	movl 8(%ecx), %ebp
	movl 12(%ecx), %esi
	movl 16(%ecx), %edi
	movl 20(%ecx), %esp
	pushl 0(%ecx)
	ret

	.align 4,0x90
	.globl ___ctx_start
___ctx_start:
	popl %eax
	call *%eax
	mov %esi, %esp
	int $5
	.subsections_via_symbols
	
     