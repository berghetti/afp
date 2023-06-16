
#ifndef KMOD_IPI_H
#define KMOD_IPI_H

#include <linux/ioctl.h>

#define KMOD_IPI_NAME "kmod_ipi"
#define KMOD_IPI_PATH "/dev/kmod_ipi"

#define MAJOR_NUM 280

/*
 * core: core to send IPI
 * _trap_entry: low level entry point to when go back from kernel on 'core'
 * */
struct req_ipi
{
  int core;
  void ( *_trap_entry ) ( void );
};

// IOCTLs
#define KMOD_IPI_SEND _IOW ( MAJOR_NUM, 0, struct req_ipi * )

#endif
