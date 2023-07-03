
#define _GNU_SOURCE  // tgkill

#include <stdint.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "afp_internal.h"
#include "globals.h"
#include "debug.h"
#include "../kmod/trap.h"

#ifdef SIGNAL

static pid_t workers_tids[MAX_WORKERS];
static pid_t pid;

void
interrupt_send ( uint16_t worker_id )
{
  pid_t tid = workers_tids[worker_id];
  syscall ( SYS_tgkill, pid, tid, SIGUSR1 );
}

void
interrupt_register_worker ( uint16_t worker_id, pid_t tid )
{
  workers_tids[worker_id] = tid;
}

void
interrupt_init ( void ( *handler ) ( int ) )
{
  const struct sigaction sa = { .sa_handler = handler, .sa_flags = SA_NODEFER };

  sigaction ( SIGUSR1, &sa, NULL );

  pid = getpid ();
}

#else

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

#endif
