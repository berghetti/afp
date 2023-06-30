
#include <stdbool.h>
#include <stdint.h>

#include <generic/rte_atomic.h>

#include "afp_internal.h"

uint16_t tot_workers;
struct rte_ring *wait_queue;

__thread uint16_t worker_id;

/* hardware queue */
__thread uint16_t hwq;

__thread rte_atomic16_t in_long_request = RTE_ATOMIC16_INIT ( 0 );
