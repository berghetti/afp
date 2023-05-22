#ifndef CONTEXT_H
#define CONTEXT_H

#include <ucontext.h>

ucontext_t *
context_alloc ( void );

// https://elixir.bootlin.com/glibc/glibc-2.37/source/sysdeps/unix/sysv/linux/x86_64/makecontext.c
// https://github.com/stanford-mast/shinjuku/blob/master/inc/ix/context.h
static inline void
context_setlink ( ucontext_t *c, ucontext_t *uc_link )
{
  uintptr_t *sp;

  /* Set up the sp pointer so that we save uc_link in the correct address. */
  sp = ( ( uintptr_t * ) c->uc_stack.ss_sp + c->uc_stack.ss_size );
  sp -= 1;

  /* We assume that we have less than 6 arguments here.
   * Align stack to multuple of 16, clearning last four bits and reserve  8
   * bytes space to return address */
  sp = ( uintptr_t * ) ( ( ( ( uintptr_t ) sp ) & -16L ) - 8 );

  c->uc_link = uc_link;

  /* push return address on stack.
     sp[1] because stack grow to "below" */
  sp[1] = ( uintptr_t ) uc_link;
}

#endif
