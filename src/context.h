#ifndef CONTEXT_H
#define CONTEXT_H

#include <ucontext.h>

ucontext_t *
context_alloc ( void );

void
context_setlink ( ucontext_t *c, ucontext_t *link );

#endif
