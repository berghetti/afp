
#include <rte_malloc.h>
#include <ucontext.h>

#define STACK_SIZE 16 * 1024

ucontext_t *
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
