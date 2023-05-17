#ifndef DPDK_HEADER_
#define DPDK_HEADER_

#include <stdint.h>

struct rte_mempool *
dpdk_init ( uint16_t port_id, uint16_t num_queues );

#endif
