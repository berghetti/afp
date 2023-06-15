
.file "trap.S"
.text

/*
 * See the "System V Application Binary Interface" for a full explation of
 * calling and argument passing conventions.
 */

/* arguments registers (can be clobbered) */
#define RDI	(0)
#define RSI	(8)
#define RDX	(16)
#define RCX	(24)
#define R8	(32)
#define R9	(40)

/* temporary registers (can be clobbered) */
#define R10	(48)
#define R11	(56)

/* callee-saved registers (can not be clobbered) */
#define RBX	(64)
#define RBP	(72)
#define R12	(80)
#define R13	(88)
#define R14	(96)
#define R15	(104)

/* special-purpose registers */
#define RAX	(112)	/* return code */
#define RIP	(120)	/* instruction pointer */
#define RSP	(128)	/* stack pointer */


/* On kernel module RSI is modified to value from current PC (EIP)
 * of running thread. After call jmp_for_me we go back to original thread code
 * in user space
 */

.align 16
.globl trap_entry
.type trap_entry, @function
trap_entry:
  // allocate space on stack to save 15 registers
  subq $RAX+8, %rsp

  // save registers
  movq %rdi, RDI(%rsp)
  movq %rsi, RSI(%rsp)
  movq %rdx, RDX(%rsp)
  movq %rcx, RCX(%rsp)
  movq %rax, RAX(%rsp)
  movq %r8, R8(%rsp)
  movq %r9, R9(%rsp)

  movq %r10, R10(%rsp)
  movq %r11, R11(%rsp)

  movq %rbx, RBX(%rsp)
  movq %rbp, RBP(%rsp) /* need? */

  movq %r12, R12(%rsp)
  movq %r13, R13(%rsp)
  movq %r14, R14(%rsp)
  movq %r15, R15(%rsp)

  // call user trap handler
  call jmp_for_me

  // restore regs
  movq RDI(%rsp), %rdi
  movq RSI(%rsp), %rsi
  movq RDX(%rsp), %rdx
  movq RCX(%rsp), %rcx
  movq RAX(%rsp), %rax
  movq R8(%rsp), %r8
  movq R9(%rsp), %r9

  movq	R10(%rsp), %r10
  movq	R11(%rsp), %r11

  movq RBX(%rsp), %rbx
  movq RBP(%rsp), %rbp /* need? */

  movq R12(%rsp), %r12
  movq R13(%rsp), %r13
  movq R14(%rsp), %r14
  movq R15(%rsp), %r15

  //leave
  addq $RAX+8, %rsp
  // in kernel module we put return address to thread app on stack
  ret
