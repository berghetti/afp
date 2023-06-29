
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

static int workers_core[MAX_WORKERS];
static int fd;

static struct req_ipi req = { ._trap_entry = _trap_entry };

void
interrupt_send ( uint16_t worker_id )
{
  req.core = workers_core[worker_id];

  ioctl ( fd, KMOD_IPI_SEND, &req );
}

void
interrupt_register_worker ( uint16_t worker_id, int core )
{
  workers_core[worker_id] = core;
}

void
interrupt_init ( void ( *handler ) ( int ) )
{
  fd = open ( KMOD_IPI_PATH, O_RDWR );
  if ( fd < 0 )
    FATAL ( "%s\n", "Error to open kmod_ipi\n" );
}
