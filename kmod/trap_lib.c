
/* high level interface to kernel module kmod_ipi */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "trap.h"
#include "kmod_ipi.h"

/* low level trap functions in trap.S */
extern void
_trap_entry ( void );

static struct req_ipi req = { ._trap_entry = _trap_entry };

static int fd;
static user_handler u_handler;

/* called from low level _trap_entry in trap.S */
void
trap ( void )
{
  /* call user level handler */
  u_handler ();
}

/* set high level user handler interruption in trap.c */
int
trap_init ( user_handler h )
{
  fd = open ( KMOD_IPI_PATH, O_RDWR );
  if ( fd < 0 )
    return -1;

  u_handler = h;
  return 0;
}

void
trap_send_interrupt ( int core )
{
  req.core = core;
  ioctl ( fd, KMOD_IPI_SEND, &req );
}

void
trap_free ( void )
{
  close ( fd );
}
