
#ifndef AFP_NETIO_H
#define AFP_NETIO_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_ring_core.h>
#include <rte_ring_elem.h>

#include "afp_internal.h"
#include "queue.h"

struct sock
{
  struct rte_mbuf *pkt;
};

void
afp_netio_init ( struct config * );

void
afp_netio_init_per_worker ( void );

bool
afp_netio_has_work ( void );

size_t
afp_recv ( void **, uint16_t *, struct sock * );

ssize_t
afp_send ( void *, uint16_t, struct sock * );

#endif  // AFP_NETIO_H
