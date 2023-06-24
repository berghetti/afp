
.file "trap.S"
.text

/*
 * See the "System V Application Binary Interface" for a full explation of
 * calling and argument passing conventions.
 */

/* clobbered registers */

/* arguments registers */
#define RDI	(0)
#define RSI	(8)
#define RDX	(16)
#define RCX	(24)
#define R8	(32)
#define R9	(40)

/* temporary registers */
#define R10	(48)
#define R11	(56)

/* return code */
#define RAX	(64)

#define FLAGS (72)

/* not considering flag register here */
#define CLOBBERED_COUNT 9
#define RESERVE CLOBBERED_COUNT * 8

/*
callee-saved registers (preserveds)
RBX
RBP
R12
R13
R14
R15

special-purpose registers
RIP		 instruction pointer
RSP		 stack pointer
*/

/*
 * On kernel module (kmod_ipi) we put EIP of app thread on stack.
 * After calling user handling function we go back to app code.
 *
 * When we got here all register has application value state.
 * So we just save, on stack, clobbered register before calling
 * user handler interruption.
 */

.align 16
.globl _trap_entry
.type _trap_entry, @function
_trap_entry:
  pushq %rbp
  movq %rsp, %rbp

  /* save flag register.
     save low 16 bits from flags register on stack */
  pushf

  /* allocate space on stack to save clobbered registers */
  subq $RESERVE, %rsp

  /* save callee-clobbered registers */
  movq %rdi, RDI(%rsp)
  movq %rsi, RSI(%rsp)
  movq %rdx, RDX(%rsp)
  movq %rcx, RCX(%rsp)
  movq %rax, RAX(%rsp)
  movq %r8, R8(%rsp)
  movq %r9, R9(%rsp)
  movq %r10, R10(%rsp)
  movq %r11, R11(%rsp)

  /* call we interrupt handler */
  call jmp_for_me

  /* restore callee-clobbered registers */
  movq RDI(%rsp), %rdi
  movq RSI(%rsp), %rsi
  movq RDX(%rsp), %rdx
  movq RCX(%rsp), %rcx
  /* movq RAX(%rsp), %rax */
  movq R8(%rsp), %r8
  movq R9(%rsp), %r9
  movq R10(%rsp), %r10
  movq R11(%rsp), %r11

  /* restore flag register.
     load flags values from stack, save by pushf, to AH register and
     load AH to flags register, using sahf.
     Could using popf, but popf is look slow
     (https://www.agner.org/optimize/instruction_tables.pdf).*/
  mov FLAGS(%rsp), %ah
  sahf

  /* finally restore rax */
  movq RAX(%rsp), %rax

  /* using leave to release stack frame because leave not affect flags register
     as "addq $RESERVE+8, %rsp" */
  leave

  /* in kernel module we put return address to thread app on stack */
  ret

