
#define _GNU_SOURCE  // tgkill
#include <stdint.h>

#include <rte_malloc.h>

#include "afp_internal.h"
#include "debug.h"
#include "../kmod/trap.h"

static int workers_core[MAX_WORKERS];

void
interrupt_send ( uint16_t worker_id )
{
  int core = workers_core[worker_id];
  trap_send_interrupt ( core );
}

void
interrupt_register_worker ( uint16_t worker_id, int core )
{
  workers_core[worker_id] = core;
}

void
interrupt_init ( user_handler handler )
{
  if ( trap_init ( handler ) )
    FATAL ( "%s\n", "Error to open kmod_ipi\n" );
}
