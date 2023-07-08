#ifndef AFP_INTERNAL_H
#define AFP_INTERNAL_H

#include <stdbool.h>

#include <rte_ether.h>

#define MAX_WORKERS 16

#define WAIT_QUEUE_SIZE 128 * 1024U  // long requests queue

#define BURST_SIZE 16                  // DPDK burst
#define QUEUE_SIZE ( BURST_SIZE * 4 )  // worker queue size (real size is -1)

struct config
{
  uint16_t port_id;
  uint16_t num_queues;
  struct rte_ether_addr my_ether_addr;
};

#endif
