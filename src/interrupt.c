
#define _GNU_SOURCE  // tgkill
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>

#include "afp_internal.h"
#include "debug.h"

static pid_t workers_tid[MAX_WORKERS];
static pid_t pid;

static __thread sigset_t sig_block;

void
interrupt_send ( uint16_t worker_id )
{
  // if ( 0 != syscall ( SYS_tgkill, SIGUSR1, pid, workers_tid[worker_id] ) )
  if ( tgkill ( pid, workers_tid[worker_id], SIGUSR1 ) != 0 )
    FATAL ( "Error send signal to worker %u\n", worker_id );
}

void
interrupt_register_work_tid ( uint16_t worker_id, pid_t tid )
{
  workers_tid[worker_id] = tid;

  sigemptyset ( &sig_block );
  sigaddset ( &sig_block, SIGUSR1 );
}

void
interrupt_enable ( void )
{
  sigprocmask ( SIG_UNBLOCK, &sig_block, NULL );
}

void
interrupt_disable ( void )
{
  sigprocmask ( SIG_BLOCK, &sig_block, NULL );
}

void
interrupt_init ( void ( *handler ) ( int ) )
{
  pid = getpid ();

  // stack_t ss = { .ss_sp = rte_malloc ( NULL, 2 * 1024, 0 ),
  //               .ss_size = 2 * 1024 };

  // sigaltstack ( &ss, NULL );

  struct sigaction sa;
  sa.sa_handler = handler;
  sa.sa_flags = SA_NODEFER | SA_RESTART | SA_ONSTACK;

  if ( sigemptyset ( &sa.sa_mask ) )
    FATAL ( "%s\n", "Error setup interrupt" );

  if ( sigaction ( SIGUSR1, &sa, NULL ) )
    FATAL ( "%s\n", "Error setup interrupt" );
}
