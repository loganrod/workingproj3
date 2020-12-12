.file "stdin"
.text
.globl main
.type main, @function
main:
	# stack space for argc and argv
	# emit main's prologue
	push	%rbp
	mov	%rsp, %rbp
	sub	$16, %rsp
	push	%rbx
	# move argc and argv from parameter registers to the stack
	mov	%rdi, -8(%rbp)
	mov	%rsi, -16(%rbp)
	# generate code for the body
	# generate code for the return expression
	# push the integer
	mov	$1, %rax
	push	%rax
	# save the return expression into %rax per the abi
	pop	%rax
	# emit main's epilogue
	pop	%rbx
	mov	%rbp, %rsp
	pop	%rbp
	ret