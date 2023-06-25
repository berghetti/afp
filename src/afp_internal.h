#ifndef AFP_INTERNAL_H
#define AFP_INTERNAL_H

#include <stdbool.h>

#include <rte_ether.h>

#define MAX_WORKERS 32

#define WAIT_QUEUE_SIZE 128 * 1024U  // long requests queue

#define BURST_SIZE 64                  // DPDK burst
#define QUEUE_SIZE ( BURST_SIZE * 2 )  // worker queue size (real size is -1)

// per worker state
struct afp_ctx
{
  bool in_long_request;
  uint16_t worker_id;
  uint16_t tot_workers;
  uint16_t hwq;                 // hardware queue idx
  struct queue *rxq;            // worker queue
  struct queue *rxqs;           // all workers queues
  struct rte_ring *wait_queue;  // long requests contexts
};

typedef struct afp_ctx afp_ctx_t;

struct config
{
  uint16_t port_id;
  uint16_t num_queues;
  struct rte_ether_addr my_ether_addr;
};

#endif
