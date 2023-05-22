#ifndef AFP_INTERNAL_H
#define AFP_INTERNAL_H

#include <rte_ether.h>

#define MAX_WORKERS 32

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

#endif
