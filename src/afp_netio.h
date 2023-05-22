
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

bool
has_work_in_queues ( struct queue *rxq, uint16_t hwq );

size_t
afp_recv ( afp_ctx_t *, void **, uint16_t *, struct sock * );

ssize_t
afp_send ( afp_ctx_t *, void *, uint16_t, struct sock * );

#endif  // AFP_NETIO_H
