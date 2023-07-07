
#pragma once

#include <stdint.h>
struct rte_mempool *
dpdk_init ( uint16_t port_id, uint16_t rxq_count, uint16_t txq_count );
