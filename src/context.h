#ifndef CONTEXT_H
#define CONTEXT_H

#include <rte_malloc.h>
#include <sys/ucontext.h>
#include <ucontext.h>

#define STACK_SIZE 16 * 1024

static inline ucontext_t *
context_alloc ( void )
{
  // TODO: create a mempool to this allocations
  ucontext_t *ctx;
  ctx = rte_malloc ( NULL, sizeof ( ucontext_t ), 0 );
  if ( !ctx )
    return NULL;

  void *stack = rte_malloc ( NULL, STACK_SIZE, 0 );
  if ( !stack )
    {
      rte_free ( ctx );
      return NULL;
    }

  ctx->uc_stack.ss_sp = stack;
  ctx->uc_stack.ss_size = STACK_SIZE;
  ctx->uc_stack.ss_flags = 0;

  return ctx;
}

static inline void
context_free ( ucontext_t *u )
{
  rte_free ( u->uc_stack.ss_sp );
  rte_free ( u );
}

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
