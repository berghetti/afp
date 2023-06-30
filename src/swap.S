
/* taken from glibc and shinjuku and modified to afp use.

https://github.com/bminor/glibc/blob/glibc-2.37/sysdeps/unix/sysv/linux/x86_64/setcontext.S
https://github.com/stanford-mast/shinjuku/blob/master/dp/core/context_fast.S
*/


.file "swap.S"

#define oRBX		0x80
#define oRBP		0x78
#define oR12		0x48
#define oR13		0x50
#define oR14		0x58
#define oR15		0x60
#define oRDI		0x68
#define oRSI		0x70
#define oRDX		0x88
#define oRCX		0x98
#define oR8		    0x28
#define oR9		    0x30
#define oRIP		0xa8
#define oRSP		0xa0
#define oFPREGSMEM	0x1a8
#define oFPREGS		0xe0
#define oMXCSR		0x1c0

/* int afp_setcontext(ucontext *u) */
.text
.align 16
.globl afp_setcontext
.type afp_setcontext, @function

afp_setcontext:
  /* disable interruptions */
  cli

  /* Load the new stack pointer, the preserved registers and
     registers used for passing args.  */
  movq oRSP(%rdi), %rsp
  movq oRBX(%rdi), %rbx
  movq oRBP(%rdi), %rbp
  movq oR12(%rdi), %r12
  movq oR13(%rdi), %r13
  movq oR14(%rdi), %r14
  movq oR15(%rdi), %r15

  /* Restore the floating-point context.  Not the registers, only the
     rest.  */
  //movq	oFPREGS(%rdi), %rcx
  //fldenv	(%rcx)
  //ldmxcsr oMXCSR(%rdi)

  /* The following ret should return to the address set with
     getcontext.  Therefore push the address on the stack.  */
  movq oRIP(%rdi), %rcx
  pushq	%rcx

  movq oRSI(%rdi), %rsi
  movq oRDI(%rdi), %rdi
  movq oRCX(%rdi), %rcx
  movq oR8(%rdi), %r8
  movq oR9(%rdi), %r9

  /* Setup finally %rdx.  */
  movq oRDX(%rdi), %rdx

  /* Clear rax to indicate success.  */
  xorl %eax, %eax

  /* enable interruptions */
  sti

  ret

