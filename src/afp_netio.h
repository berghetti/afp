
#ifndef AFP_NETIO_H
#define AFP_NETIO_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <rte_ring_core.h>
#include <rte_ring_elem.h>

#include "queue.h"

// per worker state
struct afp_ctx
{
  uint16_t worker_id;
  uint16_t tot_workers;
  uint16_t hwq;  // hardware queue idx
  struct queue *rxq;
  struct queue *rxqs;
  struct rte_ring *wait_queue;
};

typedef struct afp_ctx afp_ctx_t;

struct config
{
  uint16_t port_id;
  uint16_t num_queues;
  struct rte_ether_addr my_ether_addr;
};

struct sock
{
  struct rte_mbuf *pkt;
};

void
afp_netio_init ( struct config * );

size_t
afp_recv ( afp_ctx_t *, void **, uint16_t *, struct sock * );

ssize_t
afp_send ( afp_ctx_t *, void *, uint16_t, struct sock * );

#endif  // AFP_NETIO_H
