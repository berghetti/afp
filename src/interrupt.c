
#define _GNU_SOURCE  // tgkill
#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <signal.h>
#include <sys/ioctl.h>

#include <rte_malloc.h>

#include "afp_internal.h"
#include "debug.h"
#include "../kmod/kmod_ipi.h"
#include "../kmod/trap.h"

static pid_t workers_tid[MAX_WORKERS];
// static pid_t pid;

static int workers_core[MAX_WORKERS];
static int fd;

static __thread sigset_t sig_block;

static struct req_ipi req = { ._trap_entry = _trap_entry };

void
interrupt_send ( uint16_t worker_id )
{
  // if ( syscall ( SYS_tgkill, pid, workers_tid[worker_id], SIGUSR1 ) )
  //  FATAL ( "Error send signal to worker %u\n", worker_id );

  req.core = workers_core[worker_id];

  ioctl ( fd, KMOD_IPI_SEND, &req );
}

void
interrupt_register_work_tid ( uint16_t worker_id, pid_t tid )
{
  workers_tid[worker_id] = tid;

  sigemptyset ( &sig_block );
  sigaddset ( &sig_block, SIGUSR1 );
}

void
interrupt_register_worker ( uint16_t worker_id, int core )
{
  workers_core[worker_id] = core;
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
  fd = open ( KMOD_IPI_PATH, O_RDWR );
  if ( fd < 0 )
    FATAL ( "%s\n", "Error to open kmod_ipi\n" );
  // pid = getpid ();

  // stack_t ss = { .ss_sp = rte_malloc ( NULL, 4 * 1024, 0 ),
  //               .ss_size = 4 * 1024 };

  // sigaltstack ( &ss, NULL );

  // struct sigaction sa = { .sa_handler = handler, .sa_flags = SA_NODEFER };

  // sigemptyset ( &sa.sa_mask );
  // sigaddset ( &sa.sa_mask, SIGUSR1 );

  // if ( sigaction ( SIGUSR1, &sa, NULL ) )
  //  FATAL ( "%s\n", "Error setup interrupt" );
}
